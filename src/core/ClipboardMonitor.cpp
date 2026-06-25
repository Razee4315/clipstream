#include "core/ClipboardMonitor.h"

#include "platform/ForegroundApp.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>

namespace {

// Cheap content hash so multiple dataChanged signals for the same screenshot
// (Windows fires several) collapse to one captured entry.
quint64 imageHash(const QImage& img) {
    quint64 h = 1469598103934665603ULL; // FNV-1a
    const auto mix = [&h](quint64 v) { h = (h ^ v) * 1099511628211ULL; };
    mix(static_cast<quint64>(img.width()));
    mix(static_cast<quint64>(img.height()));
    const qsizetype bytes = img.sizeInBytes();
    mix(static_cast<quint64>(bytes));
    const uchar* bits = img.constBits();
    if (bits) {
        const qsizetype step = bytes > 4096 ? bytes / 4096 : 1;
        for (qsizetype i = 0; i < bytes; i += step)
            mix(bits[i]);
    }
    return h;
}

} // namespace

ClipboardMonitor::ClipboardMonitor(QObject* parent) : QObject(parent) {
    m_clipboard = QApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardMonitor::handleChange);

    m_imageDebounce = new QTimer(this);
    m_imageDebounce->setSingleShot(true);
    m_imageDebounce->setInterval(220);
    connect(m_imageDebounce, &QTimer::timeout, this, &ClipboardMonitor::flushPendingImage);
}

void ClipboardMonitor::flushPendingImage() {
    if (m_pendingImage.isNull())
        return;
    const quint64 hash = imageHash(m_pendingImage);
    const QImage image = m_pendingImage;
    const QString source = m_pendingSource;
    m_pendingImage = QImage();
    m_pendingSource.clear();
    if (hash == m_lastImageHash)
        return; // identical to the previous capture
    m_lastImageHash = hash;
    m_lastText.clear();
    emit imageCaptured(image, source);
}

void ClipboardMonitor::handleChange() {
    if (m_ignoreNext) {
        m_ignoreNext = false; // this change was caused by us — skip it
        return;
    }
    if (m_paused)
        return;

    const QMimeData* mime = m_clipboard->mimeData();
    if (!mime)
        return;

    // The app that owns the foreground window right now is the one that copied.
    const QString sourceApp = platform::foregroundAppName();

    if (mime->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            // Stash the latest image and (re)arm the debounce — a burst of
            // updates for one screenshot collapses into a single capture.
            m_pendingImage = image;
            if (m_pendingSource.isEmpty())
                m_pendingSource = sourceApp;
            m_imageDebounce->start();
            return;
        }
    }

    if (mime->hasText()) {
        const QString text = mime->text().trimmed();
        if (text.isEmpty() || text == m_lastText)
            return; // ignore blanks and repeats of the last text we saw
        m_lastText = text;
        m_lastImageHash = 0;
        emit textCaptured(text, sourceApp);
    }
}
