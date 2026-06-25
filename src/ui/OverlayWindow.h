#pragma once

#include "core/ClipEntry.h"
#include "ui/EntryDelegate.h"

#include <QWidget>

class Database;
class ClipboardMonitor;
class HistoryModel;
class QLineEdit;
class QListView;
class QLabel;
class QModelIndex;
class QPropertyAnimation;

// The frameless launcher overlay: search box + history list + keyboard-driven
// actions (paste, format-paste, pin, edit, delete, copy). Talks to the Database
// for data and to the ClipboardMonitor so self-initiated clipboard writes aren't
// re-captured.
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    OverlayWindow(Database* db, ClipboardMonitor* monitor, QWidget* parent = nullptr);

    void showAtCursor();
    void toggleAtCursor();
    void reload();      // re-run the current query and refresh the list
    void applyTheme();  // (re)build the stylesheet from the current palette

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum class PasteFormat { Plain, Upper, Lower, Title, Trim };

    void buildUi();
    void selectRow(int row);
    int currentRow() const;
    bool hasSelection() const;
    const ClipEntry* currentEntry() const;

    void pasteCurrent(PasteFormat format = PasteFormat::Plain);
    void copyCurrent();
    void pinCurrent();
    void editCurrent();
    void deleteCurrent();
    void showFormatMenu();
    void showContextMenu(const QPoint& globalPos);
    void onRowAction(const QModelIndex& index, EntryDelegate::Action action);
    void openSettings();
    void newSnippet();
    void runPrimarySmartAction();
    void addSmartActions(QMenu& menu, const ClipEntry& entry);

    void putOnClipboard(const ClipEntry& entry, PasteFormat format);
    void copyRawText(const QString& text);

    Database* m_db = nullptr;
    ClipboardMonitor* m_monitor = nullptr;

    QWidget* m_card = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_count = nullptr;
    QListView* m_list = nullptr;
    HistoryModel* m_model = nullptr;
    EntryDelegate* m_delegate = nullptr;
    QPropertyAnimation* m_fade = nullptr;
};
