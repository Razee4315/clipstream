#include "platform/HotkeyManager.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QSet>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

UINT toWinModifiers(Qt::KeyboardModifiers mods) {
    UINT m = 0;
    if (mods & Qt::AltModifier)     m |= MOD_ALT;
    if (mods & Qt::ControlModifier) m |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier)   m |= MOD_SHIFT;
    if (mods & Qt::MetaModifier)    m |= MOD_WIN;
    m |= MOD_NOREPEAT; // don't auto-repeat while held
    return m;
}

// Map the Qt keys we currently support to Win32 virtual-key codes. Extend this
// as the hotkey picker grows; 0 means "unsupported".
UINT toVirtualKey(Qt::Key key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<UINT>('A' + (key - Qt::Key_A));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<UINT>('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return static_cast<UINT>(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
        case Qt::Key_Space:  return VK_SPACE;
        case Qt::Key_Insert: return VK_INSERT;
        default:             return 0;
    }
}

} // namespace

// Nested class → has access to the enclosing HotkeyManager's signal.
class HotkeyManager::Impl : public QAbstractNativeEventFilter {
public:
    explicit Impl(HotkeyManager* owner) : m_owner(owner) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }

    ~Impl() override {
        for (int id : m_registered)
            ::UnregisterHotKey(nullptr, id);
        if (auto* app = QCoreApplication::instance())
            app->removeNativeEventFilter(this);
    }

    bool nativeEventFilter(const QByteArray& type, void* message, qintptr*) override {
        if (type != "windows_generic_MSG")
            return false;
        auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            const int id = static_cast<int>(msg->wParam);
            if (m_registered.contains(id)) {
                emit m_owner->activated(id);
                return true;
            }
        }
        return false;
    }

    HotkeyManager* m_owner;
    QSet<int> m_registered;
};

HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>(this)) {}

HotkeyManager::~HotkeyManager() = default;

bool HotkeyManager::registerHotkey(int id, Qt::KeyboardModifiers mods, Qt::Key key) {
    const UINT vk = toVirtualKey(key);
    if (vk == 0)
        return false;
    if (!::RegisterHotKey(nullptr, id, toWinModifiers(mods), vk))
        return false;
    d->m_registered.insert(id);
    return true;
}

void HotkeyManager::unregisterHotkey(int id) {
    if (d->m_registered.remove(id))
        ::UnregisterHotKey(nullptr, id);
}
