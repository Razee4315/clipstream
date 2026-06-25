#pragma once

#include <QStyledItemDelegate>

// Draws a clip row: a type badge (or image thumbnail), the content on one elided
// line, and an "app · time" meta line. Inline action buttons are a separate
// floating widget (RowActionsBar) positioned over the selected row; this
// delegate just keeps a right-side gutter clear for it.
class EntryDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit EntryDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};
