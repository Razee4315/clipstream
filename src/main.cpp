#include "AppController.h"
#include "theme.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QMessageBox>
#include <QSystemTrayIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ClipStream"));
    app.setApplicationDisplayName(QStringLiteral("ClipStream"));
    app.setOrganizationName(QStringLiteral("ClipStream"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    app.setQuitOnLastWindowClosed(false); // the app lives in the system tray

    QFont font(Theme::fontFamily());
    font.setPixelSize(Theme::FsBody);
    app.setFont(font);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, QStringLiteral("ClipStream"),
                              QStringLiteral("No system tray is available — ClipStream needs one to run."));
        return 1;
    }

    AppController controller;
    return app.exec();
}
