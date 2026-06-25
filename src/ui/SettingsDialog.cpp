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

namespace {

// One palette-driven stylesheet so the settings window matches the overlay.
QString buildStyleSheet() {
    const Theme::Palette& p = Theme::palette();
    QString css = QStringLiteral(
        "QDialog { background:@bg; }"
        "QLabel { color:@text; }"
        "QGroupBox { color:@muted; border:1px solid @border; border-radius:@rmd px;"
        "  margin-top:11px; padding:10px 10px 8px 10px; font-weight:600; }"
        "QGroupBox::title { subcontrol-origin:margin; subcontrol-position:top left;"
        "  left:10px; padding:0 5px; }"
        "QLineEdit, QSpinBox, QComboBox { background:@surface; color:@text;"
        "  border:1px solid @border; border-radius:8px; padding:5px 8px; min-height:20px; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border:1px solid @accent; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox QAbstractItemView { background:@surface; color:@text;"
        "  border:1px solid @border; selection-background-color:@accent; selection-color:#ffffff; outline:none; }"
        "QCheckBox { color:@text; spacing:8px; }"
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
    setMinimumWidth(380);
    setStyleSheet(buildStyleSheet());
    buildUi();
    loadIgnoredApps();
}

void SettingsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::S4, Theme::S4, Theme::S4, Theme::S4);
    root->setSpacing(Theme::S3);

    // --- Header ---------------------------------------------------------------
    auto* header = new QHBoxLayout();
    header->setSpacing(Theme::S2);
    auto* headerIcon = new QLabel(this);
    headerIcon->setPixmap(IconFactory::pixmap(QStringLiteral("settings"),
                                              QColor(Theme::palette().textPrimary), 18));
    auto* headerTitle = new QLabel(QStringLiteral("Settings"), this);
    headerTitle->setStyleSheet(QStringLiteral("font-size:16px; font-weight:700;"));
    header->addWidget(headerIcon);
    header->addWidget(headerTitle);
    header->addStretch();
    root->addLayout(header);

    // --- Appearance -----------------------------------------------------------
    auto* appearanceBox = new QGroupBox(QStringLiteral("Appearance"), this);
    auto* appearanceForm = new QFormLayout(appearanceBox);

    m_themeCombo = new QComboBox(appearanceBox);
    const QString savedTheme = m_db->setting(QStringLiteral("theme"), QStringLiteral("system"));
    int current = 0;
    const auto& defs = Theme::themes();
    for (int i = 0; i < defs.size(); ++i) {
        m_themeCombo->addItem(defs[i].label, defs[i].id);
        if (defs[i].id == savedTheme)
            current = i;
    }
    m_themeCombo->setCurrentIndex(current);
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString id = m_themeCombo->itemData(idx).toString();
        m_db->setSetting(QStringLiteral("theme"), id);
        Theme::setThemeId(id);
        emit settingsChanged();
    });
    appearanceForm->addRow(QStringLiteral("Theme"), m_themeCombo);
    root->addWidget(appearanceBox);

    // --- Capture --------------------------------------------------------------
    auto* captureBox = new QGroupBox(QStringLiteral("Capture"), this);
    auto* captureForm = new QFormLayout(captureBox);

    m_pauseCapture = new QCheckBox(QStringLiteral("Pause capturing"), captureBox);
    m_pauseCapture->setChecked(m_db->setting(QStringLiteral("paused")) == QLatin1String("1"));
    connect(m_pauseCapture, &QCheckBox::toggled, this, [this](bool on) {
        m_db->setSetting(QStringLiteral("paused"), on ? QStringLiteral("1") : QStringLiteral("0"));
        emit pauseToggled(on);
    });
    captureForm->addRow(m_pauseCapture);

    m_launchAtStartup = new QCheckBox(QStringLiteral("Start with Windows"), captureBox);
    m_launchAtStartup->setChecked(platform::isLaunchAtStartupEnabled());
    connect(m_launchAtStartup, &QCheckBox::toggled, this,
            [](bool on) { platform::setLaunchAtStartup(on); });
    captureForm->addRow(m_launchAtStartup);

    m_discardSensitive = new QCheckBox(
        QStringLiteral("Don't save passwords or secrets"), captureBox);
    m_discardSensitive->setToolTip(
        QStringLiteral("Clips that look like passwords, card numbers or API keys are dropped, not saved."));
    m_discardSensitive->setChecked(m_db->setting(QStringLiteral("discard_sensitive")) == QLatin1String("1"));
    connect(m_discardSensitive, &QCheckBox::toggled, this, [this](bool on) {
        m_db->setSetting(QStringLiteral("discard_sensitive"),
                         on ? QStringLiteral("1") : QStringLiteral("0"));
    });
    captureForm->addRow(m_discardSensitive);

    root->addWidget(captureBox);

    // --- History retention ----------------------------------------------------
    auto* historyBox = new QGroupBox(QStringLiteral("History"), this);
    auto* historyForm = new QFormLayout(historyBox);

    m_maxEntries = new QSpinBox(historyBox);
    m_maxEntries->setRange(50, 100000);
    m_maxEntries->setSingleStep(50);
    m_maxEntries->setValue(m_db->setting(QStringLiteral("max_entries"), QStringLiteral("1000")).toInt());
    connect(m_maxEntries, &QSpinBox::valueChanged, this, [this](int v) {
        m_db->setSetting(QStringLiteral("max_entries"), QString::number(v));
        emit settingsChanged();
    });
    historyForm->addRow(QStringLiteral("Keep up to"), m_maxEntries);

    m_retentionDays = new QSpinBox(historyBox);
    m_retentionDays->setRange(1, 3650);
    m_retentionDays->setValue(m_db->setting(QStringLiteral("retention_days"), QStringLiteral("30")).toInt());
    connect(m_retentionDays, &QSpinBox::valueChanged, this, [this](int v) {
        m_db->setSetting(QStringLiteral("retention_days"), QString::number(v));
        emit settingsChanged();
    });
    historyForm->addRow(QStringLiteral("Forget after (days)"), m_retentionDays);

    auto* clearBtn = new QPushButton(QStringLiteral(" Clear all history"), historyBox);
    clearBtn->setIcon(IconFactory::icon(QStringLiteral("trash"), QColor(0xef, 0x44, 0x44), 15));
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Clear all history"),
            QStringLiteral("Delete every clip, including pinned ones? This can't be undone."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        const QStringList images = m_db->clearHistory();
        for (const QString& path : images)
            QFile::remove(path);
        emit settingsChanged();
    });
    historyForm->addRow(QString(), clearBtn);

    root->addWidget(historyBox);

    // --- Ignored apps ---------------------------------------------------------
    auto* ignoreBox = new QGroupBox(QStringLiteral("Ignored apps"), this);
    auto* ignoreLayout = new QVBoxLayout(ignoreBox);

    auto* hint = new QLabel(QStringLiteral("Clipboard from these apps is never saved."), ignoreBox);
    hint->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::palette().textMuted));
    ignoreLayout->addWidget(hint);

    auto* addRow = new QHBoxLayout();
    m_appInput = new QLineEdit(ignoreBox);
    m_appInput->setPlaceholderText(QStringLiteral("e.g. 1Password.exe, KeePass.exe"));
    auto* addBtn = new QPushButton(QStringLiteral(" Add"), ignoreBox);
    addBtn->setIcon(IconFactory::icon(QStringLiteral("plus"), QColor(Theme::palette().textPrimary), 15));
    addRow->addWidget(m_appInput);
    addRow->addWidget(addBtn);
    ignoreLayout->addLayout(addRow);

    m_appList = new QListWidget(ignoreBox);
    m_appList->setMaximumHeight(88);
    ignoreLayout->addWidget(m_appList);

    auto* removeBtn = new QPushButton(QStringLiteral(" Remove selected"), ignoreBox);
    removeBtn->setIcon(IconFactory::icon(QStringLiteral("trash"), QColor(0xef, 0x44, 0x44), 15));
    ignoreLayout->addWidget(removeBtn, 0, Qt::AlignRight);

    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addIgnoredApp);
    connect(m_appInput, &QLineEdit::returnPressed, this, &SettingsDialog::addIgnoredApp);
    connect(removeBtn, &QPushButton::clicked, this, &SettingsDialog::removeSelectedApp);

    root->addWidget(ignoreBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);
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
