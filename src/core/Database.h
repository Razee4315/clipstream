#pragma once

#include "core/ClipEntry.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// Persistence layer: SQLite (via QtSql) with an FTS5 full-text index over the
// history. All clipboard data, ignored-app rules, and settings live here.
// Images are stored as PNG files on disk; only their path is kept in the DB.
class Database : public QObject {
    Q_OBJECT
public:
    explicit Database(QObject* parent = nullptr);
    ~Database() override;

    // Open (creating if needed) the DB under the app data dir. False on failure.
    bool open();

    // Directory where image PNGs are written. Created on open().
    QString imagesDir() const;

    // Insert a clip. Text duplicates bump the existing row's timestamp instead of
    // adding a new row. Returns the row id (or -1 on error).
    qint64 insertEntry(const ClipEntry& entry);

    // Empty query → recent history (pinned first). Otherwise full-text search.
    QVector<ClipEntry> search(const QString& query, int limit = 200);

    std::optional<ClipEntry> entryById(qint64 id);
    bool updateContent(qint64 id, const QString& content);
    bool togglePin(qint64 id);
    bool removeEntry(qint64 id);

    // Delete every entry (including pinned). Returns image paths to clean up.
    QStringList clearHistory();

    // Retention: drop unpinned rows older than maxAgeDays, then keep at most
    // maxEntries unpinned rows. Returns deleted image paths so the caller can
    // remove the orphaned files.
    QStringList cleanup(int maxAgeDays, int maxEntries);

    // Ignored apps (clipboard from these is never stored).
    QStringList ignoredApps();
    void addIgnoredApp(const QString& app);
    void removeIgnoredApp(const QString& app);

    // Key/value settings.
    QString setting(const QString& key, const QString& defaultValue = QString());
    void setSetting(const QString& key, const QString& value);

private:
    bool createSchema();
    static ClipEntry entryFromQuery(const class QSqlQuery& q);

    QSqlDatabase m_db;
    bool m_ftsAvailable = false;
    QString m_imagesDir;
};
