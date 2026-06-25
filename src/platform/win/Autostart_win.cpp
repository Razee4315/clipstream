#include "platform/Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace {
const char* kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
QString valueName() { return QStringLiteral("ClipStream"); }
} // namespace

namespace platform {

bool isLaunchAtStartupEnabled() {
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return run.contains(valueName());
}

void setLaunchAtStartup(bool enabled) {
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(valueName(), QStringLiteral("\"%1\"").arg(exe));
    } else {
        run.remove(valueName());
    }
}

} // namespace platform
