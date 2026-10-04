#include "AppController.h"
#include "theme.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ClipStream"));
    app.setApplicationDisplayName(QStringLiteral("ClipStream"));
    app.setOrganizationName(QStringLiteral("ClipStream"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    app.setQuitOnLastWindowClosed(false); // the app lives in the system tray

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QLockFile instance(dataDir + QStringLiteral("/instance.lock"));
    instance.setStaleLockTime(0); // a long-running tray app is not a stale lock
    if (!instance.tryLock(0)) {
        QMessageBox::information(nullptr, QStringLiteral("ClipStream"),
            instance.error() == QLockFile::LockFailedError
                ? QStringLiteral("ClipStream is already running. Press Ctrl+Shift+V or open it from the system tray.")
                : QStringLiteral("ClipStream could not access its data folder."));
        return 0;
    }

    QFont font(Theme::fontFamily());
    font.setPixelSize(Theme::FsBody);
    app.setFont(font);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, QStringLiteral("ClipStream"),
                              QStringLiteral("No system tray is available — ClipStream needs one to run."));
        return 1;
    }

    AppController controller;
    if (!controller.initialize()) {
        QMessageBox::critical(nullptr, QStringLiteral("ClipStream"),
                              QStringLiteral("Failed to initialise the clipboard database."));
        return 1;
    }
    return app.exec();
}
