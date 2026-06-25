#include "ui/HistoryModel.h"

HistoryModel::HistoryModel(QObject* parent) : QAbstractListModel(parent) {}

void HistoryModel::setEntries(QVector<ClipEntry> entries) {
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

const ClipEntry& HistoryModel::entryAt(int row) const {
    return m_entries.at(row);
}

bool HistoryModel::isValidRow(int row) const {
    return row >= 0 && row < m_entries.size();
}

int HistoryModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant HistoryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !isValidRow(index.row()))
        return {};
    const ClipEntry& e = m_entries.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return e.sensitive ? QStringLiteral("(hidden sensitive content)") : e.content;
        default:
            return {};
    }
}
