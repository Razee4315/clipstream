#include "AppController.h"

#include "core/ClipboardMonitor.h"
#include "core/ContentClassifier.h"
#include "core/Database.h"
#include "platform/HotkeyManager.h"
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

    m_overlay = std::make_unique<OverlayWindow>(m_db.get(), m_monitor.get());

    setupTray();
    setupHotkey();
    connectCapture();
    return true;
}

void AppController::setupTray() {
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon.png")), this);
    m_tray->setToolTip(QStringLiteral("ClipStream — Clipboard Manager"));

    auto* menu = new QMenu(m_overlay.get());
    QAction* openAct = menu->addAction(QStringLiteral("Open  (Ctrl+Shift+V)"));
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

    ClipEntry e;
    e.content = text;
    e.sourceApp = sourceApp;
    e.type = ContentClassifier::classify(text);
    e.sensitive = ContentClassifier::looksSensitive(text);
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
