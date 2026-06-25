#include "AppController.h"

#include "platform/HotkeyManager.h"
#include "ui/OverlayWindow.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

AppController::AppController(QObject* parent)
    : QObject(parent),
      m_overlay(std::make_unique<OverlayWindow>()),
      m_hotkey(std::make_unique<HotkeyManager>()) {
    setupTray();
    setupHotkey();
}

AppController::~AppController() = default;

void AppController::setupTray() {
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon.png")), this);
    m_tray->setToolTip(QStringLiteral("ClipStream — Clipboard Manager"));

    // Parented to the overlay (a widget) so it's cleaned up with the UI.
    auto* menu = new QMenu(m_overlay.get());
    QAction* openAct = menu->addAction(QStringLiteral("Open  (Ctrl+Shift+V)"));
    menu->addSeparator();
    QAction* quitAct = menu->addAction(QStringLiteral("Quit ClipStream"));

    connect(openAct, &QAction::triggered, this, [this] { m_overlay->showAtCursor(); });
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) // left click
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
