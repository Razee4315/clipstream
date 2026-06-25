#include "AppController.h"

#include "core/ClipboardMonitor.h"
#include "core/ContentClassifier.h"
#include "core/Database.h"
#include "platform/HotkeyManager.h"
#include "theme.h"
#include "ui/OverlayWindow.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

AppController::AppController(QObject* parent)
    : QObject(parent),
      m_db(std::make_unique<Database>()),
      m_monitor(std::make_unique<ClipboardMonitor>()),
      m_hotkey(std::make_unique<HotkeyManager>()) {}

AppController::~AppController() = default;

bool AppController::initialize() {
    if (!m_db->open())
        return false;

    // Retention pass on startup (drop old/excess clips and their image files).
    const int maxEntries = m_db->setting(QStringLiteral("max_entries"), QStringLiteral("1000")).toInt();
    const int retentionDays = m_db->setting(QStringLiteral("retention_days"), QStringLiteral("30")).toInt();
    const QStringList orphanImages = m_db->cleanup(retentionDays, maxEntries);
    for (const QString& path : orphanImages)
        QFile::remove(path);

    m_monitor->setPaused(m_db->setting(QStringLiteral("paused")) == QLatin1String("1"));
    seedDefaultIgnoredApps();

    Theme::setThemeId(m_db->setting(QStringLiteral("theme"), QStringLiteral("system")));

    m_overlay = std::make_unique<OverlayWindow>(m_db.get(), m_monitor.get());

    setupTray();
    setupHotkey();
    connectCapture();
    maybeShowOnboarding();
    return true;
}

void AppController::setupTray() {
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon.png")), this);
    m_tray->setToolTip(QStringLiteral("ClipStream — Clipboard Manager"));

    auto* menu = new QMenu(m_overlay.get());
    QAction* openAct = menu->addAction(QStringLiteral("Open  (Ctrl+Shift+V)"));

    QAction* pauseAct = menu->addAction(QStringLiteral("Pause capture"));
    pauseAct->setCheckable(true);
    pauseAct->setChecked(m_monitor->isPaused());
    // Keep the checkmark honest no matter who toggled pause (settings dialog too).
    connect(menu, &QMenu::aboutToShow, this,
            [this, pauseAct] { pauseAct->setChecked(m_monitor->isPaused()); });
    connect(pauseAct, &QAction::toggled, this, [this](bool paused) {
        m_monitor->setPaused(paused);
        m_db->setSetting(QStringLiteral("paused"), paused ? QStringLiteral("1") : QStringLiteral("0"));
    });

    menu->addSeparator();
    QAction* quitAct = menu->addAction(QStringLiteral("Quit ClipStream"));

    connect(openAct, &QAction::triggered, this, [this] { m_overlay->showAtCursor(); });
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger)
                    m_overlay->toggleAtCursor();
            });

    m_tray->setContextMenu(menu);
    m_tray->show();
}

void AppController::maybeShowOnboarding() {
    if (m_db->setting(QStringLiteral("onboarded")) == QLatin1String("1"))
        return;
    m_tray->showMessage(
        QStringLiteral("ClipStream is running"),
        QStringLiteral("Press Ctrl+Shift+V anywhere to open your clipboard history. "
                       "It lives quietly in the tray."),
        QSystemTrayIcon::Information, 6000);
    m_db->setSetting(QStringLiteral("onboarded"), QStringLiteral("1"));
}

void AppController::seedDefaultIgnoredApps() {
    if (m_db->setting(QStringLiteral("seeded")) == QLatin1String("1"))
        return;
    // Don't capture clipboard from common password managers by default.
    const QStringList managers = {
        QStringLiteral("1Password.exe"), QStringLiteral("Bitwarden.exe"),
        QStringLiteral("KeePass.exe"),   QStringLiteral("KeePassXC.exe"),
        QStringLiteral("LastPass.exe"),  QStringLiteral("Dashlane.exe"),
        QStringLiteral("NordPass.exe"),  QStringLiteral("ProtonPass.exe"),
    };
    for (const QString& app : managers)
        m_db->addIgnoredApp(app);
    m_db->setSetting(QStringLiteral("seeded"), QStringLiteral("1"));
}

void AppController::setupHotkey() {
    connect(m_hotkey.get(), &HotkeyManager::activated, this, [this](int id) {
        if (id == kToggleHotkeyId)
            m_overlay->toggleAtCursor();
    });

    const bool ok = m_hotkey->registerHotkey(
        kToggleHotkeyId, Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_V);
    if (!ok)
        m_tray->showMessage(
            QStringLiteral("ClipStream"),
            QStringLiteral("Couldn't register Ctrl+Shift+V — open from the tray icon."),
            QSystemTrayIcon::Warning);
}

void AppController::connectCapture() {
    connect(m_monitor.get(), &ClipboardMonitor::textCaptured, this, &AppController::onTextCaptured);
    connect(m_monitor.get(), &ClipboardMonitor::imageCaptured, this, &AppController::onImageCaptured);
}

bool AppController::isIgnored(const QString& sourceApp) const {
    if (sourceApp.isEmpty())
        return false;
    const QString lower = sourceApp.toLower();
    const QStringList ignored = m_db->ignoredApps();
    for (const QString& app : ignored)
        if (lower.contains(app.toLower()))
            return true;
    return false;
}

void AppController::onTextCaptured(const QString& text, const QString& sourceApp) {
    if (isIgnored(sourceApp))
        return;

    const bool sensitive = ContentClassifier::looksSensitive(text);
    if (sensitive && m_db->setting(QStringLiteral("discard_sensitive")) == QLatin1String("1"))
        return; // privacy: never persist detected secrets

    ClipEntry e;
    e.content = text;
    e.sourceApp = sourceApp;
    e.type = ContentClassifier::classify(text);
    e.sensitive = sensitive;
    m_db->insertEntry(e);
    m_overlay->reload();
}

void AppController::onImageCaptured(const QImage& image, const QString& sourceApp) {
    if (isIgnored(sourceApp))
        return;

    const QString fileName = QStringLiteral("img_%1.png")
                                 .arg(QDateTime::currentMSecsSinceEpoch());
    const QString path = m_db->imagesDir() + QLatin1Char('/') + fileName;
    if (!image.save(path, "PNG")) {
        qWarning("ClipStream: failed to save clipboard image to %s", qPrintable(path));
        return;
    }

    ClipEntry e;
    e.content = QStringLiteral("Image %1×%2").arg(image.width()).arg(image.height());
    e.sourceApp = sourceApp;
    e.type = ContentType::Image;
    e.imagePath = path;
    m_db->insertEntry(e);
    m_overlay->reload();
}
