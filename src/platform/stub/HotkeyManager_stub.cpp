#include "platform/HotkeyManager.h"

// Non-Windows stub. Global hotkeys need platform-specific code (X11 XGrabKey,
// macOS RegisterEventHotKey). Until those land, registration reports failure so
// the app cleanly falls back to tray-only activation.
class HotkeyManager::Impl {};

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent), d(nullptr) {}
HotkeyManager::~HotkeyManager() = default;

bool HotkeyManager::registerHotkey(int, Qt::KeyboardModifiers, Qt::Key) { return false; }
void HotkeyManager::unregisterHotkey(int) {}
