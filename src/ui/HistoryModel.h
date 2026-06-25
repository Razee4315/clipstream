#pragma once

#include "core/ClipEntry.h"

#include <QAbstractListModel>
#include <QVector>

// A flat list model backing the overlay's QListView. Holds the current page of
// clip entries; the delegate reads each ClipEntry via entryAt().
class HistoryModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit HistoryModel(QObject* parent = nullptr);

    void setEntries(QVector<ClipEntry> entries);
    const ClipEntry& entryAt(int row) const;
    bool isValidRow(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    QVector<ClipEntry> m_entries;
};
