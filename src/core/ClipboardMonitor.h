#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class QClipboard;
class QTimer;

// Watches the system clipboard and emits a signal whenever new content appears.
// Event-driven via QClipboard::dataChanged — no polling, ~0% idle CPU (the key
// improvement over the old timer-based implementation).
class ClipboardMonitor : public QObject {
    Q_OBJECT
public:
    explicit ClipboardMonitor(QObject* parent = nullptr);

    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused; }

    // Call right before we set the clipboard ourselves (e.g. paste/copy actions)
    // so the resulting change isn't captured back as a new clip.
    void ignoreNextChange() { m_ignoreNext = true; }

signals:
    void textCaptured(const QString& text, const QString& sourceApp);
    void imageCaptured(const QImage& image, const QString& sourceApp);

private:
    void handleChange();
    void flushPendingImage();

    QClipboard* m_clipboard = nullptr;
    bool m_paused = false;
    bool m_ignoreNext = false;
    QString m_lastText;
    quint64 m_lastImageHash = 0; // dedups repeated dataChanged for one image

    // Screenshot tools often post an image as a short burst of clipboard
    // updates; we debounce so the burst becomes a single captured entry.
    QTimer* m_imageDebounce = nullptr;
    QImage m_pendingImage;
    QString m_pendingSource;
};
