#include "core/Database.h"

#include <QDateTime>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimeZone>
#include <QVariant>

namespace {

QString typeToString(ContentType t) {
    switch (t) {
        case ContentType::Url:      return QStringLiteral("url");
        case ContentType::Code:     return QStringLiteral("code");
        case ContentType::Color:    return QStringLiteral("color");
        case ContentType::FilePath: return QStringLiteral("file");
        case ContentType::Image:    return QStringLiteral("image");
        case ContentType::Text:     break;
    }
    return QStringLiteral("text");
}

ContentType typeFromString(const QString& s) {
    if (s == QLatin1String("url"))   return ContentType::Url;
    if (s == QLatin1String("code"))  return ContentType::Code;
    if (s == QLatin1String("color")) return ContentType::Color;
    if (s == QLatin1String("file"))  return ContentType::FilePath;
    if (s == QLatin1String("image")) return ContentType::Image;
    return ContentType::Text;
}

QDateTime parseUtc(const QString& s) {
    QDateTime dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    dt.setTimeZone(QTimeZone(QTimeZone::UTC));
    return dt;
}

} // namespace

Database::Database(QObject* parent) : QObject(parent) {}

Database::~Database() {
    if (m_db.isOpen())
        m_db.close();
}

bool Database::open() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_imagesDir = dir + QStringLiteral("/images");
    QDir().mkpath(m_imagesDir);

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("clipstream"));
    m_db.setDatabaseName(dir + QStringLiteral("/clipstream.db"));
    if (!m_db.open()) {
        qWarning("ClipStream DB open failed: %s", qPrintable(m_db.lastError().text()));
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

    return createSchema();
}

QString Database::imagesDir() const {
    return m_imagesDir;
}

bool Database::createSchema() {
    QSqlQuery q(m_db);

    const bool baseOk = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS clipboard_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  content TEXT NOT NULL,"
        "  source_app TEXT,"
        "  content_type TEXT DEFAULT 'text',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  is_pinned INTEGER DEFAULT 0,"
        "  image_path TEXT,"
        "  sensitive INTEGER DEFAULT 0)"));
    if (!baseOk) {
        qWarning("ClipStream schema failed: %s", qPrintable(q.lastError().text()));
        return false;
    }

    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_created_at ON clipboard_history(created_at DESC)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_pinned ON clipboard_history(is_pinned)"));

    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS ignored_apps ("
                          "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "  app_name TEXT UNIQUE NOT NULL)"));
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS settings ("
                          "  key TEXT PRIMARY KEY, value TEXT NOT NULL)"));

    // FTS5 is optional: if the bundled SQLite lacks it, fall back to LIKE search.
    m_ftsAvailable = q.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS history_fts USING fts5("
        "  content, content='clipboard_history', content_rowid='id')"));
    if (m_ftsAvailable) {
        q.exec(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS history_ai AFTER INSERT ON clipboard_history "
            "WHEN new.sensitive = 0 BEGIN "
            "  INSERT INTO history_fts(rowid, content) VALUES (new.id, new.content); END"));
        q.exec(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS history_ad AFTER DELETE ON clipboard_history BEGIN "
            "  INSERT INTO history_fts(history_fts, rowid, content) VALUES('delete', old.id, old.content); END"));
        q.exec(QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS history_au AFTER UPDATE ON clipboard_history BEGIN "
            "  INSERT INTO history_fts(history_fts, rowid, content) VALUES('delete', old.id, old.content); "
            "  INSERT INTO history_fts(rowid, content) SELECT new.id, new.content WHERE new.sensitive = 0; END"));
    } else {
        qInfo("ClipStream: FTS5 unavailable, using LIKE search.");
    }
    return true;
}

qint64 Database::insertEntry(const ClipEntry& entry) {
    // Text de-dup: re-copying the same text bumps the existing row to the top.
    if (!entry.isImage()) {
        QSqlQuery dup(m_db);
        dup.prepare(QStringLiteral(
            "SELECT id FROM clipboard_history WHERE content = ? AND content_type != 'image' LIMIT 1"));
        dup.addBindValue(entry.content);
        if (dup.exec() && dup.next()) {
            const qint64 id = dup.value(0).toLongLong();
            QSqlQuery bump(m_db);
            bump.prepare(QStringLiteral(
                "UPDATE clipboard_history SET created_at = CURRENT_TIMESTAMP, "
                "source_app = COALESCE(?, source_app) WHERE id = ?"));
            bump.addBindValue(entry.sourceApp.isEmpty() ? QVariant() : entry.sourceApp);
            bump.addBindValue(id);
            bump.exec();
            return id;
        }
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO clipboard_history (content, source_app, content_type, image_path, sensitive) "
        "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(entry.content);
    q.addBindValue(entry.sourceApp.isEmpty() ? QVariant() : entry.sourceApp);
    q.addBindValue(typeToString(entry.type));
    q.addBindValue(entry.imagePath.isEmpty() ? QVariant() : entry.imagePath);
    q.addBindValue(entry.sensitive ? 1 : 0);
    if (!q.exec()) {
        qWarning("ClipStream insert failed: %s", qPrintable(q.lastError().text()));
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

ClipEntry Database::entryFromQuery(const QSqlQuery& q) {
    ClipEntry e;
    e.id        = q.value(0).toLongLong();
    e.content   = q.value(1).toString();
    e.sourceApp = q.value(2).toString();
    e.type      = typeFromString(q.value(3).toString());
    e.createdAt = parseUtc(q.value(4).toString());
    e.pinned    = q.value(5).toInt() != 0;
    e.imagePath = q.value(6).toString();
    e.sensitive = q.value(7).toInt() != 0;
    return e;
}

QVector<ClipEntry> Database::search(const QString& query, int limit) {
    static const QString cols = QStringLiteral(
        "id, content, source_app, content_type, created_at, is_pinned, image_path, sensitive");
    QVector<ClipEntry> out;
    QSqlQuery q(m_db);
    const QString trimmed = query.trimmed();

    if (trimmed.isEmpty()) {
        q.prepare(QStringLiteral("SELECT %1 FROM clipboard_history "
                                 "ORDER BY is_pinned DESC, created_at DESC LIMIT ?").arg(cols));
        q.addBindValue(limit);
    } else if (m_ftsAvailable) {
        q.prepare(QStringLiteral(
            "SELECT %1 FROM clipboard_history h JOIN history_fts f ON h.id = f.rowid "
            "WHERE history_fts MATCH ? ORDER BY h.is_pinned DESC, rank LIMIT ?").arg(QStringLiteral(
            "h.id, h.content, h.source_app, h.content_type, h.created_at, h.is_pinned, h.image_path, h.sensitive")));
        q.addBindValue(QString(trimmed).remove(QLatin1Char('"')) + QLatin1Char('*'));
        q.addBindValue(limit);
    } else {
        q.prepare(QStringLiteral("SELECT %1 FROM clipboard_history WHERE content LIKE ? "
                                 "ORDER BY is_pinned DESC, created_at DESC LIMIT ?").arg(cols));
        q.addBindValue(QLatin1Char('%') + trimmed + QLatin1Char('%'));
        q.addBindValue(limit);
    }

    if (!q.exec()) {
        qWarning("ClipStream search failed: %s", qPrintable(q.lastError().text()));
        return out;
    }
    while (q.next())
        out.append(entryFromQuery(q));
    return out;
}

std::optional<ClipEntry> Database::entryById(qint64 id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, content, source_app, content_type, created_at, is_pinned, image_path, sensitive "
        "FROM clipboard_history WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return entryFromQuery(q);
    return std::nullopt;
}

bool Database::updateContent(qint64 id, const QString& content) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE clipboard_history SET content = ? WHERE id = ?"));
    q.addBindValue(content);
    q.addBindValue(id);
    return q.exec();
}

bool Database::togglePin(qint64 id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE clipboard_history SET is_pinned = NOT is_pinned WHERE id = ?"));
    q.addBindValue(id);
    return q.exec();
}

bool Database::removeEntry(qint64 id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM clipboard_history WHERE id = ?"));
    q.addBindValue(id);
    return q.exec();
}

QStringList Database::cleanup(int maxAgeDays, int maxEntries) {
    QStringList orphanImages;

    // Collect image paths about to be removed so the caller can delete the files.
    QSqlQuery doomed(m_db);
    doomed.prepare(QStringLiteral(
        "SELECT image_path FROM clipboard_history WHERE is_pinned = 0 AND image_path IS NOT NULL AND ("
        "  created_at < datetime('now', ?) OR id NOT IN ("
        "    SELECT id FROM clipboard_history ORDER BY is_pinned DESC, created_at DESC LIMIT ?))"));
    doomed.addBindValue(QStringLiteral("-%1 days").arg(maxAgeDays));
    doomed.addBindValue(maxEntries);
    if (doomed.exec()) {
        while (doomed.next()) {
            const QString p = doomed.value(0).toString();
            if (!p.isEmpty())
                orphanImages << p;
        }
    }

    QSqlQuery byAge(m_db);
    byAge.prepare(QStringLiteral(
        "DELETE FROM clipboard_history WHERE is_pinned = 0 AND created_at < datetime('now', ?)"));
    byAge.addBindValue(QStringLiteral("-%1 days").arg(maxAgeDays));
    byAge.exec();

    QSqlQuery byCount(m_db);
    byCount.prepare(QStringLiteral(
        "DELETE FROM clipboard_history WHERE is_pinned = 0 AND id NOT IN ("
        "  SELECT id FROM clipboard_history ORDER BY is_pinned DESC, created_at DESC LIMIT ?)"));
    byCount.addBindValue(maxEntries);
    byCount.exec();

    return orphanImages;
}

QStringList Database::ignoredApps() {
    QStringList apps;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT app_name FROM ignored_apps ORDER BY app_name")))
        while (q.next())
            apps << q.value(0).toString();
    return apps;
}

void Database::addIgnoredApp(const QString& app) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO ignored_apps (app_name) VALUES (?)"));
    q.addBindValue(app);
    q.exec();
}

void Database::removeIgnoredApp(const QString& app) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM ignored_apps WHERE app_name = ?"));
    q.addBindValue(app);
    q.exec();
}

QString Database::setting(const QString& key, const QString& defaultValue) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return defaultValue;
}

void Database::setSetting(const QString& key, const QString& value) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}
