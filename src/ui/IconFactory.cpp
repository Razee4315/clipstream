#include "ui/IconFactory.h"

#include <QHash>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace {

// Lucide icons (ISC license) — 24×24, stroke = currentColor. Trimmed to the
// inner markup; wrapped with a common <svg> header at render time.
const QHash<QString, QString>& glyphs() {
    static const QHash<QString, QString> g = {
        {QStringLiteral("pin"),
         QStringLiteral("<path d='M12 17v5'/><path d='M9 10.76a2 2 0 0 1-1.11 1.79l-1.78.9A2 2 0 0 0 5 15.24V16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-.76a2 2 0 0 0-1.11-1.79l-1.78-.9A2 2 0 0 1 15 10.76V7a1 1 0 0 1 1-1 2 2 0 0 0 0-4H8a2 2 0 0 0 0 4 1 1 0 0 1 1 1z'/>")},
        {QStringLiteral("copy"),
         QStringLiteral("<rect width='14' height='14' x='8' y='8' rx='2' ry='2'/><path d='M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2'/>")},
        {QStringLiteral("check"),
         QStringLiteral("<path d='M20 6 9 17l-5-5'/>")},
        {QStringLiteral("edit"),
         QStringLiteral("<path d='M12 20h9'/><path d='M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4Z'/>")},
        {QStringLiteral("trash"),
         QStringLiteral("<path d='M3 6h18'/><path d='M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6'/><path d='M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2'/><line x1='10' x2='10' y1='11' y2='17'/><line x1='14' x2='14' y1='11' y2='17'/>")},
        {QStringLiteral("settings"),
         QStringLiteral("<path d='M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z'/><circle cx='12' cy='12' r='3'/>")},
        {QStringLiteral("search"),
         QStringLiteral("<circle cx='11' cy='11' r='8'/><path d='m21 21-4.3-4.3'/>")},
        {QStringLiteral("plus"),
         QStringLiteral("<path d='M5 12h14'/><path d='M12 5v14'/>")},
    };
    return g;
}

} // namespace

namespace IconFactory {

QPixmap pixmap(const QString& name, const QColor& color, int px) {
    const QString key = QStringLiteral("ic:%1:%2:%3").arg(name, color.name(QColor::HexRgb)).arg(px);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached))
        return cached;

    const QString inner = glyphs().value(name);
    QByteArray svg = QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
        "stroke='%1' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>%2</svg>")
        .arg(color.name(QColor::HexRgb), inner).toUtf8();

    QSvgRenderer renderer(svg);
    constexpr int scale = 2; // crisp on hi-DPI
    QPixmap pm(px * scale, px * scale);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    renderer.render(&p, QRectF(0, 0, px * scale, px * scale));
    p.end();
    pm.setDevicePixelRatio(scale);

    QPixmapCache::insert(key, pm);
    return pm;
}

QIcon icon(const QString& name, const QColor& color, int px) {
    return QIcon(pixmap(name, color, px));
}

} // namespace IconFactory
