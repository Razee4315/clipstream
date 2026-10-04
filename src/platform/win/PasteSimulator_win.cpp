#include "platform/PasteSimulator.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QSemaphore>
#include <QThread>
#include <QTimer>
#include <QObject>
#include <atomic>
#include <memory>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>

namespace platform {
namespace {
bool valid(PasteTarget target) {
    const HWND window = reinterpret_cast<HWND>(target.window);
    DWORD process = 0;
    return window && ::IsWindow(window) && ::GetWindowThreadProcessId(window, &process)
        && process == target.process && process != ::GetCurrentProcessId();
}
// The last rectangle of a text range: the line the caret or selection ends on.
QRect lastRectangle(IUIAutomationTextRange* range) {
    SAFEARRAY* rectangles = nullptr;
    if (FAILED(range->GetBoundingRectangles(&rectangles)) || !rectangles) return {};
    QRect last;
    LONG upper = -1;
    double* values = nullptr;
    if (SUCCEEDED(::SafeArrayGetUBound(rectangles, 1, &upper)) && upper >= 3
        && SUCCEEDED(::SafeArrayAccessData(rectangles, reinterpret_cast<void**>(&values)))) {
        const double* r = values + (upper + 1) - 4; // x, y, width, height
        last = QRect(qRound(r[0]), qRound(r[1]), qMax(1, qRound(r[2])), qRound(r[3]));
        ::SafeArrayUnaccessData(rectangles);
    }
    ::SafeArrayDestroy(rectangles);
    return last;
}
QRect focusedCaret(IUIAutomation* automation) {
    IUIAutomationElement* focused = nullptr;
    if (FAILED(automation->GetFocusedElement(&focused)) || !focused) return {};
    IUIAutomationTextRange* range = nullptr;
    IUIAutomationTextPattern2* caretPattern = nullptr;
    if (SUCCEEDED(focused->GetCurrentPatternAs(UIA_TextPattern2Id, __uuidof(IUIAutomationTextPattern2),
                                               reinterpret_cast<void**>(&caretPattern))) && caretPattern) {
        BOOL active = FALSE;
        caretPattern->GetCaretRange(&active, &range);
        caretPattern->Release();
    }
    if (!range) { // Chromium has no caret range; the selection is the caret
        IUIAutomationTextPattern* text = nullptr;
        if (SUCCEEDED(focused->GetCurrentPatternAs(UIA_TextPatternId, __uuidof(IUIAutomationTextPattern),
                                                   reinterpret_cast<void**>(&text))) && text) {
            IUIAutomationTextRangeArray* selection = nullptr;
            int count = 0;
            if (SUCCEEDED(text->GetSelection(&selection)) && selection) {
                if (SUCCEEDED(selection->get_Length(&count)) && count > 0)
                    selection->GetElement(0, &range);
                selection->Release();
            }
            text->Release();
        }
    }
    QRect caret;
    if (range) {
        caret = lastRectangle(range);
        if (caret.isEmpty()) { // an empty range may have no rectangle of its own
            range->ExpandToEnclosingUnit(TextUnit_Character);
            caret = lastRectangle(range);
        }
        range->Release();
    }
    focused->Release();
    return caret;
}
// Apps that draw their own caret (browsers, Electron, modern UI frameworks) only
// report it through UI Automation. Those calls cross into the other app, so they
// run on a worker thread and the popup waits a bounded time for the answer.
class CaretLocator {
public:
    static CaretLocator& instance() {
        static CaretLocator* locator = new CaretLocator; // lives for the whole process
        return *locator;
    }
    QRect locate(int timeoutMs) {
        if (m_busy.exchange(true)) return {}; // still waiting on an app that is slow to answer
        auto answer = std::make_shared<Answer>();
        QMetaObject::invokeMethod(m_worker, [this, answer] {
            if (m_automation) answer->caret = focusedCaret(m_automation);
            m_busy = false;
            answer->ready.release();
        }, Qt::QueuedConnection);
        return answer->ready.tryAcquire(1, timeoutMs) ? answer->caret : QRect();
    }
private:
    struct Answer { QSemaphore ready; QRect caret; };
    CaretLocator() : m_worker(new QObject) {
        m_worker->moveToThread(&m_thread);
        m_thread.start();
        QMetaObject::invokeMethod(m_worker, [this] {
            ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(::CoCreateInstance(__uuidof(CUIAutomation8), nullptr, CLSCTX_INPROC_SERVER,
                                          __uuidof(IUIAutomation), reinterpret_cast<void**>(&m_automation))))
                m_automation = nullptr;
            IUIAutomation2* limits = nullptr;
            if (m_automation && SUCCEEDED(m_automation->QueryInterface(__uuidof(IUIAutomation2),
                                                                     reinterpret_cast<void**>(&limits))) && limits) {
                // Never let an unresponsive app hold the worker for long.
                limits->put_ConnectionTimeout(300);
                limits->put_TransactionTimeout(300);
                limits->Release();
            }
        }, Qt::QueuedConnection);
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, m_worker, [this] {
            if (m_automation) { m_automation->Release(); m_automation = nullptr; }
            ::CoUninitialize();
            m_thread.quit();
        });
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [this] { m_thread.wait(1000); });
    }
    QThread m_thread;
    QObject* m_worker;
    IUIAutomation* m_automation = nullptr; // worker thread only
    std::atomic_bool m_busy{false};
};
// The taskbar and tray are foreground when the popup is opened from the tray
// icon; they are never a useful paste destination.
bool isShellWindow(HWND window) {
    wchar_t name[64] = {};
    ::GetClassNameW(window, name, 64);
    for (const wchar_t* shell : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"NotifyIconOverflowWindow",
                                 L"TopLevelWindowForOverflowXamlIsland"})
        if (!::lstrcmpW(name, shell)) return true;
    return false;
}
// 1 elevated, 0 not, -1 unknown.
int elevation(HANDLE process) {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(process, TOKEN_QUERY, &token)) return -1;
    TOKEN_ELEVATION state{};
    DWORD size = 0;
    const BOOL ok = ::GetTokenInformation(token, TokenElevation, &state, sizeof(state), &size);
    ::CloseHandle(token);
    return ok ? (state.TokenIsElevated ? 1 : 0) : -1;
}
// Windows silently drops SendInput aimed at an administrator window from a
// normal process, so report that as a failure instead of pretending to paste.
bool blockedByElevation(PasteTarget target) {
    if (elevation(::GetCurrentProcess()) != 0) return false;
    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, target.process);
    if (!process) return false;
    const int state = elevation(process);
    ::CloseHandle(process);
    return state == 1;
}
bool modifiersHeld() {
    for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN})
        if (::GetAsyncKeyState(key) & 0x8000) return true;
    return false;
}
bool restoreFocusedControl(PasteTarget target) {
    if (!target.focus) return true;
    const HWND window = reinterpret_cast<HWND>(target.window);
    const HWND focus = reinterpret_cast<HWND>(target.focus);
    if (!::IsWindow(focus) || (focus != window && !::IsChild(window, focus))) return false;
    const DWORD thread = ::GetWindowThreadProcessId(window, nullptr);
    GUITHREADINFO info{sizeof(GUITHREADINFO)};
    if (!::GetGUIThreadInfo(thread, &info)) return false;
    if (info.hwndFocus == focus) return true;
    // Only needed when a target did not restore its previous child control.
    // Attach briefly after modifiers are released; never leave input queues joined.
    DWORD_PTR response = 0;
    if (!::SendMessageTimeoutW(window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 100, &response)) return false;
    const DWORD current = ::GetCurrentThreadId();
    if (!::AttachThreadInput(current, thread, TRUE)) return false;
    ::SetFocus(focus);
    ::AttachThreadInput(current, thread, FALSE);
    return ::GetGUIThreadInfo(thread, &info) && info.hwndFocus == focus;
}
}
PasteTarget capturePasteTarget() {
    HWND window = ::GetForegroundWindow();
    DWORD process = 0;
    ::GetWindowThreadProcessId(window, &process);
    PasteTarget target{reinterpret_cast<quintptr>(window), process};
    GUITHREADINFO info{sizeof(GUITHREADINFO)};
    if (::GetGUIThreadInfo(::GetWindowThreadProcessId(window, nullptr), &info))
        target.focus = reinterpret_cast<quintptr>(info.hwndFocus);
    return valid(target) && !isShellWindow(window) ? target : PasteTarget{};
}
bool ownsClipboard() {
    return QGuiApplication::clipboard()->ownsClipboard();
}
QRect caretRect(PasteTarget target) {
    if (!valid(target)) return {};
    const HWND window = reinterpret_cast<HWND>(target.window);
    RECT frame{};
    ::GetWindowRect(window, &frame);
    // A caret reported outside its own window is stale or in another coordinate space.
    const auto inside = [&frame](const QRect& caret) {
        return !caret.isEmpty() && caret.center().x() >= frame.left && caret.center().x() <= frame.right
            && caret.center().y() >= frame.top && caret.center().y() <= frame.bottom;
    };
    GUITHREADINFO info{sizeof(GUITHREADINFO)};
    if (::GetGUIThreadInfo(::GetWindowThreadProcessId(window, nullptr), &info)
        && info.hwndCaret && info.rcCaret.bottom > info.rcCaret.top) {
        POINT topLeft{info.rcCaret.left, info.rcCaret.top};
        ::ClientToScreen(info.hwndCaret, &topLeft);
        const QRect caret(topLeft.x, topLeft.y, qMax(1L, info.rcCaret.right - info.rcCaret.left),
                          info.rcCaret.bottom - info.rcCaret.top);
        if (inside(caret)) return caret;
    }
    const QRect caret = CaretLocator::instance().locate(70);
    return inside(caret) ? caret : QRect();
}
void prepareCaretLookup() {
    CaretLocator::instance();
}
bool restorePasteTarget(PasteTarget target) {
    if (!valid(target)) return false;
    HWND window = reinterpret_cast<HWND>(target.window);
    return ::GetForegroundWindow() == window || ::SetForegroundWindow(window);
}
void pasteToTarget(PasteTarget target, QObject* context, std::function<void(bool)> finished) {
    if (!restorePasteTarget(target) || blockedByElevation(target)) { finished(false); return; }
    auto* timer = new QTimer(context);
    timer->setInterval(20);
    QObject::connect(timer, &QTimer::timeout, context,
        [target, timer, finished = std::move(finished), attempts = 0]() mutable {
        // Never send to a different window, even if the user changes focus mid-paste.
        if (!valid(target) || ::GetForegroundWindow() != reinterpret_cast<HWND>(target.window)
            || ++attempts > 75) {
            timer->stop(); timer->deleteLater(); finished(false); return;
        }
        // A held Shift from Ctrl+Shift+V must not turn this into Ctrl+Shift+V.
        if (modifiersHeld()) return;
        if (!restoreFocusedControl(target) || ::GetForegroundWindow() != reinterpret_cast<HWND>(target.window)) {
            timer->stop(); timer->deleteLater(); finished(false); return;
        }
        INPUT inputs[4] = {};
        for (auto& input : inputs) input.type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;
        inputs[1].ki.wVk = 'V';
        inputs[2].ki.wVk = 'V'; inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[3].ki.wVk = VK_CONTROL; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        const UINT sent = ::SendInput(4, inputs, sizeof(INPUT));
        if (sent > 0 && sent < 4) {
            INPUT release[2] = {inputs[2], inputs[3]};
            ::SendInput(2, release, sizeof(INPUT));
        }
        timer->stop(); timer->deleteLater(); finished(sent == 4);
    });
    timer->start();
}
void setPopupNonActivating(quintptr window, bool enabled) {
    HWND handle = reinterpret_cast<HWND>(window);
    const LONG_PTR style = ::GetWindowLongPtr(handle, GWL_EXSTYLE);
    ::SetWindowLongPtr(handle, GWL_EXSTYLE, enabled ? style | WS_EX_NOACTIVATE : style & ~WS_EX_NOACTIVATE);
}
bool handlePopupNativeEvent(void* message, qintptr* result) {
    const auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_MOUSEACTIVATE
        && (::GetWindowLongPtr(msg->hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
        *result = MA_NOACTIVATE;
        return true;
    }
    return false;
}
} // namespace platform
