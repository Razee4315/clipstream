#include "ui/ThumbnailCache.h"

#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QPointer>
#include <QThreadPool>

namespace {
constexpr int kEdge = 96;      // crisp for a 32px badge up to 300% scaling
constexpr int kMaxEntries = 400;
} // namespace

ThumbnailCache::ThumbnailCache(QObject* parent) : QObject(parent) {}

QPixmap ThumbnailCache::get(const QString& path) {
    const auto found = m_thumbnails.constFind(path);
    if (found != m_thumbnails.constEnd())
        return *found;
    if (m_pending.contains(path))
        return {};
    m_pending.insert(path);

    QPointer<ThumbnailCache> self(this);
    QThreadPool::globalInstance()->start([self, path] {
        QImage image = QImageReader(path).read();
        if (!image.isNull()) {
            image = image.scaled(kEdge, kEdge, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            image = image.copy((image.width() - kEdge) / 2, (image.height() - kEdge) / 2, kEdge, kEdge);
        }
        // Pixmaps belong to the GUI thread; hand the image back before converting.
        QMetaObject::invokeMethod(qApp, [self, path, image] {
            if (!self)
                return;
            if (self->m_thumbnails.size() >= kMaxEntries)
                self->m_thumbnails.clear();
            self->m_pending.remove(path);
            self->m_thumbnails.insert(path, QPixmap::fromImage(image));
            emit self->ready();
        }, Qt::QueuedConnection);
    });
    return {};
}
