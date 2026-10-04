#include "platform/PopupInput.h"
#include <QCoreApplication>
#include <QCursor>
#include <QTimer>
#include <QWidget>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

PopupInput::PopupInput(QObject* parent) : QObject(parent), m_timer(new QTimer(this)) {
    QCoreApplication::instance()->installNativeEventFilter(this);
    m_timer->setInterval(25);
}
PopupInput::~PopupInput() {
    stop();
    if (QCoreApplication::instance()) QCoreApplication::instance()->removeNativeEventFilter(this);
}
void PopupInput::stop() {
    for (const auto& binding : m_bindings) ::UnregisterHotKey(nullptr, binding.id);
    m_bindings.clear();
    m_timer->stop();
    disconnect(m_timer, nullptr, this, nullptr);
}
void PopupInput::start(QWidget* popup, platform::PasteTarget target) {
    stop();
    if (!target.window) return;
    int id = 0x4000;
    auto bind = [&](UINT vk, int key, Qt::KeyboardModifiers mods = Qt::NoModifier, bool repeat = false) {
        UINT flags = repeat ? 0 : MOD_NOREPEAT;
        if (mods & Qt::ControlModifier) flags |= MOD_CONTROL;
        if (mods & Qt::ShiftModifier) flags |= MOD_SHIFT;
        if (::RegisterHotKey(nullptr, id, flags, vk)) m_bindings.append({id, key, mods});
        ++id;
    };
    // Holding an arrow scrolls through the list; everything else fires once.
    bind(VK_UP, Qt::Key_Up, Qt::NoModifier, true); bind(VK_DOWN, Qt::Key_Down, Qt::NoModifier, true);
    bind(VK_RETURN, Qt::Key_Return); bind(VK_ESCAPE, Qt::Key_Escape);
    bind(VK_RETURN, Qt::Key_Return, Qt::ShiftModifier);
    bind(VK_DELETE, Qt::Key_Delete, Qt::ShiftModifier);
    bind(VK_F2, Qt::Key_F2);
    bind(VK_SPACE, Qt::Key_Space, Qt::ControlModifier);
    for (int key : {Qt::Key_F, Qt::Key_N, Qt::Key_P, Qt::Key_O})
        bind(key, key, Qt::ControlModifier);
    for (int i = 0; i < 9; ++i) bind('1' + i, Qt::Key_1 + i, Qt::ControlModifier);
    auto mouseDown = [] {
        return (::GetAsyncKeyState(VK_LBUTTON) & 0x8000)
            || (::GetAsyncKeyState(VK_RBUTTON) & 0x8000)
            || (::GetAsyncKeyState(VK_MBUTTON) & 0x8000);
    };
    m_mouseDown = mouseDown();
    connect(m_timer, &QTimer::timeout, this, [this, popup, target, mouseDown] {
        const bool down = mouseDown();
        const bool outside = down && !m_mouseDown && !popup->geometry().contains(QCursor::pos());
        m_mouseDown = down;
        const HWND foreground = ::GetForegroundWindow();
        DWORD process = 0;
        ::GetWindowThreadProcessId(foreground, &process);
        // No foreground window is a transient state during a switch, not a dismissal.
        if (outside || (foreground && foreground != reinterpret_cast<HWND>(target.window)
                        && process != ::GetCurrentProcessId())) emit dismissRequested();
    });
    m_timer->start();
}
bool PopupInput::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    const auto* msg = static_cast<MSG*>(message);
    if (msg->message != WM_HOTKEY) return false;
    for (const auto& binding : m_bindings) {
        if (binding.id == static_cast<int>(msg->wParam)) {
            // Copy before emitting: the receiver may stop() and invalidate bindings.
            const auto key = binding.key;
            const auto modifiers = binding.modifiers;
            emit shortcut(key, modifiers);
            return true;
        }
    }
    return false;
}
