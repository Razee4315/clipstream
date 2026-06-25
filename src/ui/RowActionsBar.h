#pragma once

#include <QWidget>

class QToolButton;

// A small floating toolbar the overlay positions over the active row, so the
// user can pin / copy / edit / delete with a single click (no right-click).
// Real QToolButtons give native hover + pressed feedback; copy shows a brief
// check-mark confirmation.
class RowActionsBar : public QWidget {
    Q_OBJECT
public:
    explicit RowActionsBar(QWidget* parent = nullptr);

    void configure(bool pinned, bool editable); // per-row state before showing
    void retheme();                             // recolour icons for the palette
    void flashCopied();                         // momentary copy confirmation
    int widthFor(bool editable) const;

signals:
    void pinClicked();
    void copyClicked();
    void editClicked();
    void deleteClicked();

private:
    QToolButton* makeButton(const QString& tip);

    QToolButton* m_pin = nullptr;
    QToolButton* m_copy = nullptr;
    QToolButton* m_edit = nullptr;
    QToolButton* m_delete = nullptr;
    bool m_pinned = false;
};
