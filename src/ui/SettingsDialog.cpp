#include "ui/SettingsDialog.h"

#include "core/Database.h"
#include "platform/Autostart.h"
#include "theme.h"
#include "ui/IconFactory.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QScrollArea>

namespace {

// One palette-driven stylesheet so the settings window matches the overlay.
QString buildStyleSheet() {
    const Theme::Palette& p = Theme::palette();
    QString css = QStringLiteral(
        "QDialog { background:@bg; }"
        "QLabel { color:@text; }"
        "QWidget { color:@text; }"
        "#muted { color:@muted; font-size:12px; }"
        "QTabWidget::pane { border:1px solid @border; border-radius:10px; background:@surface; }"
        "QScrollArea, QScrollArea > QWidget > QWidget { background:@surface; }"
        "QTabBar::tab { background:transparent; color:@muted; padding:9px 12px; border-bottom:2px solid transparent; }"
        "QTabBar::tab:selected { color:@text; border-bottom-color:@accent; }"
        "#danger { color:#ef4444; }"
        "QPushButton:disabled { color:@muted; background:@bg; }"
        "QPushButton:focus { border-color:@accent; }"
        "QGroupBox { color:@muted; border:1px solid @border; border-radius:@rmd px;"
        "  margin-top:11px; padding:10px 10px 8px 10px; font-weight:600; }"
        "QGroupBox::title { subcontrol-origin:margin; subcontrol-position:top left;"
        "  left:10px; padding:0 5px; }"
        "QLineEdit, QSpinBox, QComboBox { background:@surface; color:@text;"
        "  border:1px solid @border; border-radius:8px; padding:5px 8px; min-height:20px; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border:1px solid @accent; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox::down-arrow { image:url(:/chevron-down.svg); width:14px; height:14px; }"
        "QSpinBox { padding-right:22px; }"
        "QSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:22px; border:none; }"
        "QSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:22px; border:none; }"
        "QSpinBox::up-arrow { image:url(:/chevron-up.svg); width:12px; height:12px; }"
        "QSpinBox::down-arrow { image:url(:/chevron-down.svg); width:12px; height:12px; }"
        "QComboBox QAbstractItemView { background:@surface; color:@text;"
        "  border:1px solid @border; selection-background-color:@accent; selection-color:#ffffff; outline:none; }"
        "QCheckBox { color:@text; spacing:8px; }"
        "QCheckBox::indicator { width:16px; height:16px; border:1px solid @border; border-radius:4px; background:@surfaceAlt; }"
        "QCheckBox::indicator:checked { background:@accent; border-color:@accent; image:url(:/check.svg); }"
        "QCheckBox::indicator:hover { border-color:@accent; }"
        "QScrollBar:vertical { background:transparent; width:8px; margin:2px; }"
        "QScrollBar::handle:vertical { background:@border; min-height:24px; border-radius:3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }"
        "QListWidget { background:@surface; color:@text; border:1px solid @border; border-radius:8px; }"
        "QListWidget::item { padding:5px 6px; border-radius:4px; }"
        "QListWidget::item:selected { background:@accent; color:#ffffff; }"
        "QPushButton { background:@surfaceAlt; color:@text; border:1px solid @border;"
        "  border-radius:8px; padding:6px 12px; }"
        "QPushButton:hover { border:1px solid @accent; }"
        "QPushButton:pressed { background:@border; }");
    css.replace(QLatin1String("@bg"), p.bg);
    css.replace(QLatin1String("@surfaceAlt"), p.surfaceAlt);
    css.replace(QLatin1String("@surface"), p.surface);
    css.replace(QLatin1String("@border"), p.border);
    css.replace(QLatin1String("@accent"), p.accent);
    css.replace(QLatin1String("@text"), p.textPrimary);
    css.replace(QLatin1String("@muted"), p.textMuted);
    css.replace(QLatin1String("@rmd"), QString::number(Theme::RadiusMd));
    return css;
}

} // namespace

SettingsDialog::SettingsDialog(Database* db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle(QStringLiteral("ClipStream Settings"));
    resize(480, 520);
    setMinimumSize(420, 440);
    setStyleSheet(buildStyleSheet());
    buildUi();
    loadIgnoredApps();
}

void SettingsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);
    auto* title = new QLabel(QStringLiteral("Settings"), this);
    title->setStyleSheet(QStringLiteral("font-size:20px; font-weight:700;"));
    auto* heading = new QHBoxLayout;
    heading->addWidget(title);
    heading->addStretch();
    auto* mark = new QLabel(this);
    mark->setFixedSize(24, 24);
    mark->setPixmap(QIcon(Theme::isDark() ? QStringLiteral(":/logo-dark.svg")
                                        : QStringLiteral(":/logo.svg"))
                        .pixmap(mark->size(), devicePixelRatioF()));
    heading->addWidget(mark);
    auto* brand = new QLabel(this);
    brand->setAccessibleName(QStringLiteral("ClipStream"));
    brand->setFixedSize(112, 24);
    brand->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    brand->setPixmap(QIcon(Theme::isDark() ? QStringLiteral(":/wordmark-dark.svg")
                                         : QStringLiteral(":/wordmark.svg"))
                         .pixmap(brand->size(), devicePixelRatioF()));
    heading->addWidget(brand);
    root->addLayout(heading);
    auto* subtitle = new QLabel(QStringLiteral("Make ClipStream work your way."), this);
    subtitle->setObjectName(QStringLiteral("muted"));
    root->addWidget(subtitle);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    root->addWidget(tabs, 1);
    auto page = [tabs](const QString& name) {
        auto* scroll = new QScrollArea(tabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget(scroll);
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(12, 14, 12, 12);
        layout->setSpacing(8);
        scroll->setWidget(content);
        tabs->addTab(scroll, name);
        return layout;
    };
    auto hint = [](QVBoxLayout* layout, const QString& text) {
        auto* label = new QLabel(text);
        label->setObjectName(QStringLiteral("muted"));
        label->setWordWrap(true);
        layout->addWidget(label);
    };
    auto* general = page(QStringLiteral("General"));
    auto* form = new QFormLayout();
    form->setSpacing(12);
    m_themeCombo = new QComboBox(this);
    m_themeCombo->setObjectName(QStringLiteral("themeChoice"));
    for (const auto& theme : Theme::themes()) m_themeCombo->addItem(theme.label, theme.id);
    m_themeCombo->setCurrentIndex(qMax(0, m_themeCombo->findData(m_db->setting(QStringLiteral("theme"), QStringLiteral("system")))));
    form->addRow(QStringLiteral("Theme"), m_themeCombo);
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this] {
        const QString id = m_themeCombo->currentData().toString();
        m_db->setSetting(QStringLiteral("theme"), id);
        Theme::setThemeId(id);
        setStyleSheet(buildStyleSheet());
        emit settingsChanged();
    });
    auto* size = new QComboBox(this);
    size->setObjectName(QStringLiteral("overlaySize"));
    size->addItem(QStringLiteral("Compact"), QStringLiteral("compact"));
    size->addItem(QStringLiteral("Comfortable"), QStringLiteral("comfortable"));
    size->setCurrentIndex(qMax(0, size->findData(m_db->setting(QStringLiteral("overlay_size"), QStringLiteral("compact")))));
    form->addRow(QStringLiteral("Popup size"), size);
    connect(size, &QComboBox::currentIndexChanged, this, [this, size] {
        m_db->setSetting(QStringLiteral("overlay_size"), size->currentData().toString());
        emit settingsChanged();
    });
    auto* anchor = new QComboBox(this);
    anchor->setObjectName(QStringLiteral("popupAnchor"));
    anchor->addItem(QStringLiteral("At the text cursor"), QStringLiteral("caret"));
    anchor->addItem(QStringLiteral("At the mouse pointer"), QStringLiteral("mouse"));
    anchor->setCurrentIndex(qMax(0, anchor->findData(m_db->setting(QStringLiteral("popup_anchor"), QStringLiteral("caret")))));
    form->addRow(QStringLiteral("Open popup"), anchor);
    connect(anchor, &QComboBox::currentIndexChanged, this, [this, anchor] {
        m_db->setSetting(QStringLiteral("popup_anchor"), anchor->currentData().toString());
    });
    general->addLayout(form);
    m_launchAtStartup = new QCheckBox(QStringLiteral("Start with Windows"), this);
    m_launchAtStartup->setChecked(platform::isLaunchAtStartupEnabled());
    connect(m_launchAtStartup, &QCheckBox::toggled, this, [](bool enabled) { platform::setLaunchAtStartup(enabled); });
    general->addWidget(m_launchAtStartup);
    hint(general, QStringLiteral("Ctrl+Shift+V opens your history without taking focus. Click a clip to paste it into the original app."));
    hint(general, QStringLiteral("To find the text cursor in browsers, ClipStream asks the app's accessibility support. "
                                 "If the app does not report one, the popup opens at the mouse pointer."));
    hint(general, QStringLiteral("Ctrl+F  Search    ↑↓  Navigate    Enter  Paste\nCtrl+Space  Preview    Ctrl+P  Pin    Esc  Close"));
    general->addStretch();

    auto* privacy = page(QStringLiteral("Privacy"));
    m_pauseCapture = new QCheckBox(QStringLiteral("Pause clipboard capture"), this);
    m_pauseCapture->setChecked(m_db->setting(QStringLiteral("paused")) == QLatin1String("1"));
    connect(m_pauseCapture, &QCheckBox::toggled, this, [this](bool paused) {
        m_db->setSetting(QStringLiteral("paused"), paused ? QStringLiteral("1") : QStringLiteral("0"));
        emit pauseToggled(paused);
    });
    privacy->addWidget(m_pauseCapture);
    m_discardSensitive = new QCheckBox(QStringLiteral("Skip detected passwords and secrets"), this);
    m_discardSensitive->setChecked(m_db->setting(QStringLiteral("discard_sensitive")) == QLatin1String("1"));
    connect(m_discardSensitive, &QCheckBox::toggled, this, [this](bool enabled) {
        m_db->setSetting(QStringLiteral("discard_sensitive"), enabled ? QStringLiteral("1") : QStringLiteral("0"));
    });
    privacy->addWidget(m_discardSensitive);
    hint(privacy, QStringLiteral("Detection is best-effort. Exclude sensitive apps below to prevent their clips from being saved."));
    // Remove sits beside the heading so it stays visible without scrolling.
    auto* ignoredRow = new QHBoxLayout();
    ignoredRow->addWidget(new QLabel(QStringLiteral("Excluded apps"), this));
    ignoredRow->addStretch();
    auto* remove = new QPushButton(QStringLiteral("Remove selected"), this);
    remove->setObjectName(QStringLiteral("removeApp"));
    remove->setEnabled(false);
    ignoredRow->addWidget(remove);
    privacy->addLayout(ignoredRow);
    auto* addRow = new QHBoxLayout();
    m_appInput = new QLineEdit(this);
    m_appInput->setPlaceholderText(QStringLiteral("App name, e.g. KeePass.exe"));
    m_appInput->setAccessibleName(QStringLiteral("Excluded app name"));
    auto* add = new QPushButton(QStringLiteral("Add"), this);
    add->setEnabled(false);
    connect(m_appInput, &QLineEdit::textChanged, add, [add](const QString& text) { add->setEnabled(!text.trimmed().isEmpty()); });
    addRow->addWidget(m_appInput, 1);
    addRow->addWidget(add);
    privacy->addLayout(addRow);
    m_appList = new QListWidget(this);
    m_appList->setAccessibleName(QStringLiteral("Excluded apps"));
    m_appList->setFixedHeight(118); // the list scrolls; the page should not
    privacy->addWidget(m_appList);
    privacy->addStretch();
    connect(m_appList, &QListWidget::itemSelectionChanged, remove, [this, remove] {
        remove->setEnabled(!m_appList->selectedItems().isEmpty());
    });
    connect(add, &QPushButton::clicked, this, &SettingsDialog::addIgnoredApp);
    connect(m_appInput, &QLineEdit::returnPressed, this, &SettingsDialog::addIgnoredApp);
    connect(remove, &QPushButton::clicked, this, &SettingsDialog::removeSelectedApp);

    auto* history = page(QStringLiteral("History"));
    auto* historyForm = new QFormLayout();
    historyForm->setSpacing(12);
    m_maxEntries = new QSpinBox(this);
    m_maxEntries->setRange(50, 100000);
    m_maxEntries->setSingleStep(50);
    m_maxEntries->setSuffix(QStringLiteral(" clips"));
    m_maxEntries->setValue(m_db->setting(QStringLiteral("max_entries"), QStringLiteral("1000")).toInt());
    connect(m_maxEntries, &QSpinBox::valueChanged, this, [this](int value) {
        m_db->setSetting(QStringLiteral("max_entries"), QString::number(value));
    });
    historyForm->addRow(QStringLiteral("History limit"), m_maxEntries);
    m_retentionDays = new QSpinBox(this);
    m_retentionDays->setRange(1, 3650);
    m_retentionDays->setSuffix(QStringLiteral(" days"));
    m_retentionDays->setValue(m_db->setting(QStringLiteral("retention_days"), QStringLiteral("30")).toInt());
    connect(m_retentionDays, &QSpinBox::valueChanged, this, [this](int value) {
        m_db->setSetting(QStringLiteral("retention_days"), QString::number(value));
    });
    historyForm->addRow(QStringLiteral("Keep clips for"), m_retentionDays);
    history->addLayout(historyForm);
    hint(history, QStringLiteral("Limits apply as new clips arrive. Pinned clips are always kept."));
    auto* cleanup = new QPushButton(QStringLiteral("Apply limits now"), this);
    connect(cleanup, &QPushButton::clicked, this, [this] {
        const auto paths = m_db->cleanup(m_retentionDays->value(), m_maxEntries->value());
        for (const QString& path : paths) QFile::remove(path);
        emit settingsChanged();
    });
    history->addWidget(cleanup, 0, Qt::AlignLeft);
    auto clear = [this](bool includePinned) {
        const QString title = includePinned ? QStringLiteral("Delete everything?") : QStringLiteral("Clear unpinned history?");
        const QString detail = includePinned ? QStringLiteral("All saved clips, including pinned snippets, will be deleted. This cannot be undone.")
            : QStringLiteral("Unpinned clips will be deleted. Your pinned clips and snippets will be kept. This cannot be undone.");
        if (QMessageBox::question(this, title, detail, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        const auto paths = m_db->clearHistory(includePinned);
        for (const QString& path : paths) QFile::remove(path);
        emit settingsChanged();
    };
    auto* clearUnpinned = new QPushButton(QStringLiteral("Clear unpinned history"), this);
    clearUnpinned->setObjectName(QStringLiteral("clearUnpinned"));
    connect(clearUnpinned, &QPushButton::clicked, this, [clear] { clear(false); });
    history->addWidget(clearUnpinned);
    auto* clearAll = new QPushButton(QStringLiteral("Delete everything, including pins…"), this);
    clearAll->setObjectName(QStringLiteral("danger"));
    connect(clearAll, &QPushButton::clicked, this, [clear] { clear(true); });
    history->addWidget(clearAll);
    history->addStretch();
    auto* footer = new QHBoxLayout();
    auto* saved = new QLabel(QStringLiteral("Changes save automatically"), this);
    saved->setObjectName(QStringLiteral("muted"));
    footer->addWidget(saved);
    footer->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    footer->addWidget(buttons);
    root->addLayout(footer);
}

void SettingsDialog::loadIgnoredApps() {
    m_appList->clear();
    m_appList->addItems(m_db->ignoredApps());
}

void SettingsDialog::addIgnoredApp() {
    const QString name = m_appInput->text().trimmed();
    if (name.isEmpty())
        return;
    m_db->addIgnoredApp(name);
    m_appInput->clear();
    loadIgnoredApps();
    emit settingsChanged();
}

void SettingsDialog::removeSelectedApp() {
    auto* item = m_appList->currentItem();
    if (!item)
        return;
    m_db->removeIgnoredApp(item->text());
    loadIgnoredApps();
    emit settingsChanged();
}
