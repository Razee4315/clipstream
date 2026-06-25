#pragma once

#include <QStyledItemDelegate>

// Draws a single clip row: a type badge (or image thumbnail) on the left, the
// content on one elided line, and an "app · time" meta line beneath. Pinned
// rows get a marker. Reads the ClipEntry straight from the HistoryModel.
class EntryDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit EntryDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};
