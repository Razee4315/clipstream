#include "ui/EntryDelegate.h"

#include "core/ClipEntry.h"
#include "theme.h"
#include "ui/HistoryModel.h"

#include <QApplication>
#include <QDateTime>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPixmapCache>

namespace {

QColor badgeColor(ContentType type) {
    switch (type) {
        case ContentType::Url:      return QColor(0x3b, 0x82, 0xf6);
        case ContentType::Code:     return QColor(0xa7, 0x8b, 0xfa);
        case ContentType::Color:    return QColor(0x6b, 0x67, 0x60);
        case ContentType::FilePath: return QColor(0xf5, 0x9e, 0x0b);
        case ContentType::Image:    return QColor(0x10, 0xb9, 0x81);
        case ContentType::Text:     break;
    }
    return QColor(0x55, 0x55, 0x5c);
}

QString relativeTime(const QDateTime& utc) {
    const qint64 secs = utc.toLocalTime().secsTo(QDateTime::currentDateTime());
    if (secs < 60)    return QStringLiteral("now");
    if (secs < 3600)  return QStringLiteral("%1m").arg(secs / 60);
    if (secs < 86400) return QStringLiteral("%1h").arg(secs / 3600);
    return QStringLiteral("%1d").arg(secs / 86400);
}

QString appDisplayName(const QString& app) {
    if (app.isEmpty())
        return QStringLiteral("Unknown");
    QString name = app;
    name.remove(QStringLiteral(".exe"), Qt::CaseInsensitive);
    return name;
}

// Cheap vector glyphs so we don't ship an icon font. Drawn in white on the badge.
void drawGlyph(QPainter* p, const QRectF& r, ContentType type, const QString& content) {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    if (type == ContentType::Color) {
        QColor swatch(content.trimmed());
        if (!swatch.isValid())
            swatch = badgeColor(type);
        p->setPen(QPen(QColor(255, 255, 255, 60), 1));
        p->setBrush(swatch);
        p->drawRoundedRect(r.adjusted(7, 7, -7, -7), 4, 4);
        p->restore();
        return;
    }

    QPen pen(QColor(255, 255, 255, 235));
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);

    const QRectF g = r.adjusted(9, 9, -9, -9);
    switch (type) {
        case ContentType::Url: {
            // a simple chain-link
            p->drawRoundedRect(QRectF(g.left(), g.center().y() - 3, g.width() * 0.6, 6), 3, 3);
            p->drawRoundedRect(QRectF(g.right() - g.width() * 0.6, g.center().y() - 3, g.width() * 0.6, 6), 3, 3);
            break;
        }
        case ContentType::Code: {
            const qreal midY = g.center().y();
            QPainterPath lt;
            lt.moveTo(g.center().x() - 1, g.top());
            lt.lineTo(g.left(), midY);
            lt.lineTo(g.center().x() - 1, g.bottom());
            QPainterPath rt;
            rt.moveTo(g.center().x() + 1, g.top());
            rt.lineTo(g.right(), midY);
            rt.lineTo(g.center().x() + 1, g.bottom());
            p->drawPath(lt);
            p->drawPath(rt);
            break;
        }
        case ContentType::FilePath: {
            QRectF folder = g.adjusted(0, g.height() * 0.18, 0, 0);
            p->drawRoundedRect(folder, 2, 2);
            p->drawLine(QPointF(folder.left(), folder.top()),
                        QPointF(folder.left() + folder.width() * 0.45, folder.top()));
            break;
        }
        case ContentType::Image:
        case ContentType::Text:
        default: {
            const qreal step = g.height() / 4.0;
            for (int i = 1; i <= 3; ++i) {
                const qreal y = g.top() + step * i - step / 2;
                const qreal right = (i == 3) ? g.left() + g.width() * 0.6 : g.right();
                p->drawLine(QPointF(g.left(), y), QPointF(right, y));
            }
            break;
        }
    }
    p->restore();
}

} // namespace

EntryDelegate::EntryDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize EntryDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    return QSize(option.rect.width(), Theme::RowHeight);
}

void EntryDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const {
    const auto* model = qobject_cast<const HistoryModel*>(index.model());
    if (!model || !model->isValidRow(index.row())) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    const ClipEntry& e = model->entryAt(index.row());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect full = option.rect.adjusted(Theme::S2, 2, -Theme::S2, -2);
    const bool selected = option.state & QStyle::State_Selected;
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(Theme::SurfaceAlt));
        painter->drawRoundedRect(full, Theme::RadiusSm, Theme::RadiusSm);
        // accent rail on the left edge of the selected row
        painter->setBrush(QColor(Theme::Accent));
        painter->drawRoundedRect(QRect(full.left(), full.top() + 8, 3, full.height() - 16), 2, 2);
    }

    // Badge / thumbnail.
    const int badge = Theme::BadgeSize;
    const QRect badgeRect(full.left() + Theme::S3, full.center().y() - badge / 2, badge, badge);
    if (e.isImage() && !e.imagePath.isEmpty()) {
        QPixmap pm;
        const QString key = QStringLiteral("clip_") + e.imagePath;
        if (!QPixmapCache::find(key, &pm)) {
            QPixmap src(e.imagePath);
            if (!src.isNull()) {
                pm = src.scaled(badge, badge, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                QPixmapCache::insert(key, pm);
            }
        }
        QPainterPath clip;
        clip.addRoundedRect(badgeRect, 6, 6);
        painter->setClipPath(clip);
        if (!pm.isNull())
            painter->drawPixmap(badgeRect, pm, QRect((pm.width() - badge) / 2, (pm.height() - badge) / 2, badge, badge));
        else {
            painter->setBrush(badgeColor(ContentType::Image));
            painter->drawRoundedRect(badgeRect, 6, 6);
        }
        painter->setClipping(false);
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(badgeColor(e.type));
        painter->drawRoundedRect(badgeRect, 8, 8);
        drawGlyph(painter, badgeRect, e.type, e.content);
    }

    // Text block to the right of the badge.
    const int textLeft = badgeRect.right() + Theme::S3;
    const int textRight = full.right() - Theme::S3;
    const QRect textArea(textLeft, full.top(), textRight - textLeft, full.height());

    const QString title = e.sensitive
        ? QStringLiteral("•••••  sensitive — hidden")
        : e.content.simplified();

    QFont titleFont = option.font;
    titleFont.setPixelSize(Theme::FsBody);
    painter->setFont(titleFont);
    painter->setPen(QColor(e.sensitive ? Theme::TextMuted : Theme::TextPrimary));
    const QFontMetrics tfm(titleFont);
    const QString elidedTitle = tfm.elidedText(title, Qt::ElideRight, textArea.width());
    painter->drawText(QRect(textArea.left(), textArea.top() + 9, textArea.width(), tfm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    QFont metaFont = option.font;
    metaFont.setPixelSize(Theme::FsMeta);
    painter->setFont(metaFont);
    painter->setPen(QColor(Theme::TextMuted));
    const QString meta = appDisplayName(e.sourceApp) + QStringLiteral("  ·  ") + relativeTime(e.createdAt);
    const QFontMetrics mfm(metaFont);
    painter->drawText(QRect(textArea.left(), textArea.bottom() - mfm.height() - 7, textArea.width(), mfm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, mfm.elidedText(meta, Qt::ElideRight, textArea.width()));

    // Pin marker.
    if (e.pinned) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(Theme::Accent));
        painter->drawEllipse(QPoint(full.right() - Theme::S2, full.top() + Theme::S3), 3, 3);
    }

    painter->restore();
}
