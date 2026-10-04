#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

// Small square thumbnails for image clips. Decoding a full screenshot takes tens
// of milliseconds, so it happens on a worker thread; the row shows a placeholder
// until ready() announces the thumbnail.
class ThumbnailCache : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailCache(QObject* parent = nullptr);

    // The thumbnail if it has been decoded, otherwise a null pixmap (and the
    // decode is started). A file that cannot be read stays a null pixmap.
    QPixmap get(const QString& path);

signals:
    void ready();

private:
    QHash<QString, QPixmap> m_thumbnails;
    QSet<QString> m_pending;
};
