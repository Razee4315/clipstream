#pragma once

#include <QDateTime>
#include <QString>

// The content category of a clip. Drives the row icon, smart actions, and
// monospace/colour rendering in the UI.
enum class ContentType {
    Text,
    Url,
    Code,
    Color,
    FilePath,
    Image,
};

// A single clipboard history record. Mirrors a row of the clipboard_history
// table. For images, `content` holds a human label (e.g. "Image 1920x1080")
// and `imagePath` points at the PNG on disk (images are never stored in the DB).
struct ClipEntry {
    qint64      id = -1;
    QString     content;
    QString     sourceApp;
    ContentType type = ContentType::Text;
    QDateTime   createdAt;
    bool        pinned = false;
    QString     imagePath;       // absolute path to PNG for Image entries
    bool        sensitive = false; // masked in UI, kept out of the search index

    bool isImage() const { return type == ContentType::Image; }
};
