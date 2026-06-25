#pragma once

#include <QDialog>

class Database;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QCheckBox;

// Configuration UI: ignored apps (clipboard from them is never stored), history
// retention limits, capture pause, and launch-at-startup. Reads/writes through
// the Database settings store.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Database* db, QWidget* parent = nullptr);

signals:
    void pauseToggled(bool paused);
    void settingsChanged();

private:
    void buildUi();
    void loadIgnoredApps();
    void addIgnoredApp();
    void removeSelectedApp();

    Database* m_db;
    QListWidget* m_appList = nullptr;
    QLineEdit* m_appInput = nullptr;
    QSpinBox* m_maxEntries = nullptr;
    QSpinBox* m_retentionDays = nullptr;
    QCheckBox* m_pauseCapture = nullptr;
    QCheckBox* m_launchAtStartup = nullptr;
    QCheckBox* m_discardSensitive = nullptr;
};
