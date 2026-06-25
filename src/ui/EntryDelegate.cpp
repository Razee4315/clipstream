#include "ui/EntryDelegate.h"

#include "core/ClipEntry.h"
#include "theme.h"
#include "ui/HistoryModel.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPixmapCache>

namespace {

constexpr int kButton = 26; // inline action button size
constexpr int kGap = 2;

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

// Draw one inline action button (background chip + icon).
void drawActionButton(QPainter* p, const QRect& rect, EntryDelegate::Action action,
                      bool pinned, const Theme::Palette& pal, bool dark) {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // chip background
    p->setPen(Qt::NoPen);
    p->setBrush(dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 16));
    p->drawRoundedRect(rect, 7, 7);

    const QRectF g = QRectF(rect).adjusted(7, 7, -7, -7);
    const qreal cx = g.center().x();
    QColor ink(pal.textPrimary);
    QPen pen(ink);
    pen.setWidthF(1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    switch (action) {
        case EntryDelegate::Action::Pin: {
            const QColor pinCol = pinned ? QColor(pal.accent) : QColor(pal.textMuted);
            pen.setColor(pinCol);
            p->setPen(pen);
            p->setBrush(pinned ? pinCol : QBrush(Qt::NoBrush));
            p->drawEllipse(QPointF(cx, g.top() + 4), 3.2, 3.2);
            p->setBrush(Qt::NoBrush);
            p->drawLine(QPointF(cx, g.top() + 7), QPointF(cx, g.bottom()));
            break;
        }
        case EntryDelegate::Action::Copy: {
            p->setPen(pen);
            p->setBrush(Qt::NoBrush);
            QRectF back(g.left() + 3, g.top(), g.width() - 4, g.height() - 4);
            QRectF front(g.left(), g.top() + 3, g.width() - 4, g.height() - 3);
            p->drawRoundedRect(back, 2, 2);
            p->setBrush(dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 16));
            p->drawRoundedRect(front, 2, 2);
            break;
        }
        case EntryDelegate::Action::Edit: {
            p->setPen(pen);
            p->drawLine(QPointF(g.left() + 1, g.bottom() - 1), QPointF(g.right() - 2, g.top() + 2));
            p->drawLine(QPointF(g.left() + 1, g.bottom() - 1), QPointF(g.left() + 1, g.bottom() - 4));
            p->drawLine(QPointF(g.left() + 1, g.bottom() - 1), QPointF(g.left() + 4, g.bottom() - 1));
            break;
        }
        case EntryDelegate::Action::Delete: {
            pen.setColor(QColor(0xef, 0x44, 0x44));
            p->setPen(pen);
            p->setBrush(Qt::NoBrush);
            p->drawLine(QPointF(g.left(), g.top() + 3), QPointF(g.right(), g.top() + 3)); // lid
            p->drawLine(QPointF(cx - 2.5, g.top()), QPointF(cx + 2.5, g.top()));          // handle
            QRectF body(g.left() + 1.5, g.top() + 3, g.width() - 3, g.height() - 3);
            p->drawRoundedRect(body, 1.5, 1.5);
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

QVector<EntryDelegate::ActionButton> EntryDelegate::actionButtons(const QRect& rowRect,
                                                                  bool isImage) const {
    QVector<Action> actions{Action::Pin, Action::Copy};
    if (!isImage)
        actions << Action::Edit;
    actions << Action::Delete;

    const int n = actions.size();
    const int totalW = n * kButton + (n - 1) * kGap;
    int x = rowRect.right() - Theme::S2 - totalW;
    const int y = rowRect.center().y() - kButton / 2;

    QVector<ActionButton> out;
    out.reserve(n);
    for (Action a : actions) {
        out.append({a, QRect(x, y, kButton, kButton)});
        x += kButton + kGap;
    }
    return out;
}

void EntryDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const {
    const auto* model = qobject_cast<const HistoryModel*>(index.model());
    if (!model || !model->isValidRow(index.row())) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    const ClipEntry& e = model->entryAt(index.row());
    const Theme::Palette& pal = Theme::palette();
    const bool dark = Theme::isDark();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect full = option.rect.adjusted(Theme::S2, 2, -Theme::S2, -2);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool showButtons = selected || hovered;

    if (selected || hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(pal.surfaceAlt));
        painter->drawRoundedRect(full, Theme::RadiusSm, Theme::RadiusSm);
        if (selected) {
            painter->setBrush(QColor(pal.accent));
            painter->drawRoundedRect(QRect(full.left(), full.top() + 8, 3, full.height() - 16), 2, 2);
        }
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

    // Reserve the button strip on the right at all times so text never reflows.
    const auto buttons = actionButtons(full, e.isImage());
    const int rightLimit = buttons.first().rect.left() - Theme::S2;

    const int textLeft = badgeRect.right() + Theme::S3;
    const QRect textArea(textLeft, full.top(), rightLimit - textLeft, full.height());

    const QString title = e.sensitive
        ? QStringLiteral("•••••  sensitive — hidden")
        : e.content.simplified();

    QFont titleFont = (e.type == ContentType::Code)
                          ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                          : option.font;
    titleFont.setPixelSize(Theme::FsBody);
    painter->setFont(titleFont);
    painter->setPen(QColor(e.sensitive ? pal.textMuted : pal.textPrimary));
    const QFontMetrics tfm(titleFont);
    painter->drawText(QRect(textArea.left(), textArea.top() + 9, textArea.width(), tfm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      tfm.elidedText(title, Qt::ElideRight, textArea.width()));

    QFont metaFont = option.font;
    metaFont.setPixelSize(Theme::FsMeta);
    painter->setFont(metaFont);
    painter->setPen(QColor(pal.textMuted));
    const QString meta = appDisplayName(e.sourceApp) + QStringLiteral("  ·  ") + relativeTime(e.createdAt);
    const QFontMetrics mfm(metaFont);
    painter->drawText(QRect(textArea.left(), textArea.bottom() - mfm.height() - 7, textArea.width(), mfm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, mfm.elidedText(meta, Qt::ElideRight, textArea.width()));

    if (showButtons) {
        for (const ActionButton& b : buttons)
            drawActionButton(painter, b.rect, b.action, e.pinned, pal, dark);
    } else if (e.pinned) {
        // Compact pinned marker when the buttons aren't shown.
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(pal.accent));
        painter->drawEllipse(QPoint(full.right() - Theme::S2 - 2, full.center().y()), 3, 3);
    }

    painter->restore();
}

bool EntryDelegate::editorEvent(QEvent* event, QAbstractItemModel* /*model*/,
                                const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() != QEvent::MouseButtonRelease)
        return false;

    const auto* histModel = qobject_cast<const HistoryModel*>(index.model());
    if (!histModel || !histModel->isValidRow(index.row()))
        return false;

    const bool isImage = histModel->entryAt(index.row()).isImage();
    const QRect full = option.rect.adjusted(Theme::S2, 2, -Theme::S2, -2);
    const QPoint pos = static_cast<QMouseEvent*>(event)->position().toPoint();

    for (const ActionButton& b : actionButtons(full, isImage)) {
        if (b.rect.contains(pos)) {
            emit actionClicked(index, b.action);
            return true; // consume — don't trigger row activation/paste
        }
    }
    return false;
}
