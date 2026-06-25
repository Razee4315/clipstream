#pragma once

#include <QStyledItemDelegate>
#include <QVector>

// Draws a clip row: a type badge (or image thumbnail), the content on one elided
// line, an "app · time" meta line, and — on the selected/hovered row — a set of
// inline action buttons (pin, copy, edit, delete) so the user never has to
// right-click. Clicks on those buttons are reported via actionClicked().
class EntryDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum class Action { Pin, Copy, Edit, Delete };

    explicit EntryDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option, const QModelIndex& index) override;

signals:
    void actionClicked(const QModelIndex& index, EntryDelegate::Action action);

private:
    struct ActionButton {
        Action action;
        QRect rect;
    };
    // Button hit-rects for a row, right-aligned. `isImage` drops the Edit button.
    QVector<ActionButton> actionButtons(const QRect& rowRect, bool isImage) const;
};
