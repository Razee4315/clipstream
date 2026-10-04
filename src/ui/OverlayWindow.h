#pragma once

#include "core/ClipEntry.h"
#include "platform/PasteSimulator.h"

#include <QPixmap>
#include <QWidget>
#include <functional>
#include <optional>

class Database;
class ClipboardMonitor;
class HistoryModel;
class EntryDelegate;
class RowActionsBar;
class QLineEdit;
class QListView;
class QLabel;
class QToolButton;
class QModelIndex;
class QPropertyAnimation;
class QMenu;
class QButtonGroup;
class QPushButton;
class PopupInput;
class QClipboard;
class QScreen;
class QTimer;

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
    void warmUp();      // pay the one-off first-show costs ahead of time

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool nativeEvent(const QByteArray& type, void* message, qintptr* result) override;

private:
    enum class PasteFormat { Plain, Upper, Lower, Title, Trim };

    void buildUi();
    void selectRow(int row);
    int currentRow() const;
    bool hasSelection() const;
    const ClipEntry* currentEntry() const;
    std::optional<ClipEntry> currentFullEntry() const;

    void pasteCurrent(PasteFormat format = PasteFormat::Plain);
    void copyCurrent();
    void pinCurrent();
    void editCurrent();
    void deleteCurrent();
    void showFormatMenu();
    void showContextMenu(const QPoint& globalPos);
    void positionActionsBar();
    void openSettings();
    void newSnippet();
    void previewCurrent();
    void updateCaptureState();
    void activateForSearch();
    void finishPaste();
    bool writeClipboard(const std::function<void(QClipboard*)>& write);
    void hideIfAbandoned();
    void resizeOverlay(const QScreen* screen = nullptr);
    void runPrimarySmartAction();
    void addSmartActions(QMenu& menu, const ClipEntry& entry);

    bool putOnClipboard(const ClipEntry& entry, PasteFormat format);
    void copyRawText(const QString& text);

    Database* m_db = nullptr;
    ClipboardMonitor* m_monitor = nullptr;

    QWidget* m_card = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_searchIcon = nullptr;
    QToolButton* m_settingsBtn = nullptr;
    QLabel* m_count = nullptr;
    QListView* m_list = nullptr;
    HistoryModel* m_model = nullptr;
    EntryDelegate* m_delegate = nullptr;
    RowActionsBar* m_actions = nullptr;
    QPropertyAnimation* m_fade = nullptr;
    QButtonGroup* m_filters = nullptr;
    ClipFilter m_filter = ClipFilter::All;
    QWidget* m_empty = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyIcon = nullptr;
    QLabel* m_emptyHint = nullptr;
    QToolButton* m_pauseBtn = nullptr;
    QToolButton* m_newBtn = nullptr;
    QPushButton* m_previewBtn = nullptr;
    bool m_childDialogOpen = false;
    bool m_browsingWithoutFocus = false;
    bool m_pasting = false;
    platform::PasteTarget m_pasteTarget;
    PopupInput* m_popupInput = nullptr;
    QPixmap m_shadow; // drawn once per size, not re-blurred on every repaint
    QTimer* m_searchDelay = nullptr;
    qint64 m_lastQueryMs = 0;
signals:
    void pasteFailed();
    void clipboardBusy();
};
