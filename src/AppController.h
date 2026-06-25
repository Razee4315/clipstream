#pragma once

#include <QObject>
#include <memory>

class QSystemTrayIcon;
class HotkeyManager;
class OverlayWindow;

// Owns the app's long-lived pieces and wires them together: the tray icon, the
// global hotkey, and the overlay. Keeps main() tiny and avoids a God object by
// delegating real work to the dedicated classes.
class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

private:
    void setupTray();
    void setupHotkey();

    std::unique_ptr<OverlayWindow> m_overlay;
    std::unique_ptr<HotkeyManager> m_hotkey;
    QSystemTrayIcon* m_tray = nullptr;

    static constexpr int kToggleHotkeyId = 1;
};
