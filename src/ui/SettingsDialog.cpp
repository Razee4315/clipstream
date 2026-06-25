#include "ui/SettingsDialog.h"

#include "core/Database.h"
#include "platform/Autostart.h"
#include "theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(Database* db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle(QStringLiteral("ClipStream Settings"));
    setMinimumWidth(420);
    buildUi();
    loadIgnoredApps();
}

void SettingsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::S5, Theme::S5, Theme::S5, Theme::S5);
    root->setSpacing(Theme::S4);

    // --- Capture --------------------------------------------------------------
    auto* captureBox = new QGroupBox(QStringLiteral("Capture"), this);
    auto* captureForm = new QFormLayout(captureBox);

    m_pauseCapture = new QCheckBox(QStringLiteral("Pause clipboard capture"), captureBox);
    m_pauseCapture->setChecked(m_db->setting(QStringLiteral("paused")) == QLatin1String("1"));
    connect(m_pauseCapture, &QCheckBox::toggled, this, [this](bool on) {
        m_db->setSetting(QStringLiteral("paused"), on ? QStringLiteral("1") : QStringLiteral("0"));
        emit pauseToggled(on);
    });
    captureForm->addRow(m_pauseCapture);

    m_launchAtStartup = new QCheckBox(QStringLiteral("Launch ClipStream at login"), captureBox);
    m_launchAtStartup->setChecked(platform::isLaunchAtStartupEnabled());
    connect(m_launchAtStartup, &QCheckBox::toggled, this,
            [](bool on) { platform::setLaunchAtStartup(on); });
    captureForm->addRow(m_launchAtStartup);

    m_discardSensitive = new QCheckBox(
        QStringLiteral("Never store passwords / secrets"), captureBox);
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
    historyForm->addRow(QStringLiteral("Max items kept"), m_maxEntries);

    m_retentionDays = new QSpinBox(historyBox);
    m_retentionDays->setRange(1, 3650);
    m_retentionDays->setValue(m_db->setting(QStringLiteral("retention_days"), QStringLiteral("30")).toInt());
    connect(m_retentionDays, &QSpinBox::valueChanged, this, [this](int v) {
        m_db->setSetting(QStringLiteral("retention_days"), QString::number(v));
        emit settingsChanged();
    });
    historyForm->addRow(QStringLiteral("Delete after (days)"), m_retentionDays);

    root->addWidget(historyBox);

    // --- Ignored apps ---------------------------------------------------------
    auto* ignoreBox = new QGroupBox(QStringLiteral("Ignored apps"), this);
    auto* ignoreLayout = new QVBoxLayout(ignoreBox);

    auto* hint = new QLabel(QStringLiteral("Clipboard from these apps is never saved."), ignoreBox);
    hint->setStyleSheet(QStringLiteral("color:%1;").arg(QString::fromUtf8(Theme::TextMuted)));
    ignoreLayout->addWidget(hint);

    auto* addRow = new QHBoxLayout();
    m_appInput = new QLineEdit(ignoreBox);
    m_appInput->setPlaceholderText(QStringLiteral("e.g. 1Password.exe, KeePass.exe"));
    auto* addBtn = new QPushButton(QStringLiteral("Add"), ignoreBox);
    addRow->addWidget(m_appInput);
    addRow->addWidget(addBtn);
    ignoreLayout->addLayout(addRow);

    m_appList = new QListWidget(ignoreBox);
    m_appList->setMaximumHeight(120);
    ignoreLayout->addWidget(m_appList);

    auto* removeBtn = new QPushButton(QStringLiteral("Remove selected"), ignoreBox);
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
