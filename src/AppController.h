#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <memory>

class QSystemTrayIcon;
class Database;
class ClipboardMonitor;
class HotkeyManager;
class OverlayWindow;

// Owns the app's long-lived pieces and wires them together: database, clipboard
// monitor, global hotkey, tray, and the overlay. Keeps main() tiny and avoids a
// God object by delegating real work to the dedicated classes.
class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    bool initialize(); // open DB etc.; false if the app cannot run

private:
    void setupTray();
    void setupHotkey();
    void connectCapture();
    void seedDefaultIgnoredApps();
    void maybeShowOnboarding();

    void onTextCaptured(const QString& text, const QString& sourceApp);
    void onImageCaptured(const QImage& image, const QString& sourceApp);
    bool isIgnored(const QString& sourceApp) const;

    std::unique_ptr<Database> m_db;
    std::unique_ptr<ClipboardMonitor> m_monitor;
    std::unique_ptr<HotkeyManager> m_hotkey;
    std::unique_ptr<OverlayWindow> m_overlay;
    QSystemTrayIcon* m_tray = nullptr;

    static constexpr int kToggleHotkeyId = 1;
};
