#include "platform/PasteSimulator.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>
#include <QObject>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace platform {
namespace {
bool valid(PasteTarget target) {
    const HWND window = reinterpret_cast<HWND>(target.window);
    DWORD process = 0;
    return window && ::IsWindow(window) && ::GetWindowThreadProcessId(window, &process)
        && process == target.process && process != ::GetCurrentProcessId();
}
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
