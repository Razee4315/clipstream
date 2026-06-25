#include "core/ClipboardMonitor.h"

#include "platform/ForegroundApp.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

ClipboardMonitor::ClipboardMonitor(QObject* parent) : QObject(parent) {
    m_clipboard = QApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardMonitor::handleChange);
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
            m_lastText.clear();
            emit imageCaptured(image, sourceApp);
            return;
        }
    }

    if (mime->hasText()) {
        const QString text = mime->text().trimmed();
        if (text.isEmpty() || text == m_lastText)
            return; // ignore blanks and repeats of the last text we saw
        m_lastText = text;
        emit textCaptured(text, sourceApp);
    }
}
