#include "ui/OverlayWindow.h"

#include "core/ClipboardMonitor.h"
#include "core/ContentClassifier.h"
#include "core/Database.h"
#include "core/MathEval.h"
#include "platform/PasteSimulator.h"
#include "platform/PopupInput.h"
#include "theme.h"
#include "ui/EntryDelegate.h"
#include "ui/HistoryModel.h"
#include "ui/IconFactory.h"
#include "ui/RowActionsBar.h"
#include "ui/SettingsDialog.h"

#include <QApplication>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QMessageBox>
#include <QClipboard>
#include <QColor>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QEasingCurve>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QProcess>
#include <QPropertyAnimation>
#include <QScreen>
#include <QScrollBar>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString colorToHsl(const QColor& c) {
    return QStringLiteral("hsl(%1, %2%, %3%)")
        .arg(qMax(0, c.hslHue()))
        .arg(qRound(c.hslSaturationF() * 100))
        .arg(qRound(c.lightnessF() * 100));
}

QString colorToRgb(const QColor& c) {
    return QStringLiteral("rgb(%1, %2, %3)").arg(c.red()).arg(c.green()).arg(c.blue());
}

QString toTitleCase(const QString& s) {
    QString out = s.toLower();
    bool atStart = true;
    for (QChar& c : out) {
        if (atStart && c.isLetter()) {
            c = c.toUpper();
            atStart = false;
        } else if (c.isSpace()) {
            atStart = true;
        }
    }
    return out;
}

} // namespace

OverlayWindow::OverlayWindow(Database* db, ClipboardMonitor* monitor, QWidget* parent)
    : QWidget(parent), m_db(db), m_monitor(monitor) {
    setWindowTitle(QStringLiteral("ClipStream"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    m_popupInput = new PopupInput(this);
    connect(m_popupInput, &PopupInput::shortcut, this, [this](int key, Qt::KeyboardModifiers mods) {
        if (!isVisible() || !m_browsingWithoutFocus) return;
        QKeyEvent event(QEvent::KeyPress, key, mods);
        QApplication::sendEvent(m_search, &event);
    });
    connect(m_popupInput, &PopupInput::dismissRequested, this, [this] {
        if (!m_childDialogOpen) hide();
    });
    buildUi();
}

void OverlayWindow::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::ShadowMargin, Theme::ShadowMargin,
                              Theme::ShadowMargin, Theme::ShadowMargin);

    m_card = new QWidget(this);
    m_card->setObjectName(QStringLiteral("card"));
    outer->addWidget(m_card);

    auto* shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(40);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 170));
    m_card->setGraphicsEffect(shadow);

    auto* col = new QVBoxLayout(m_card);
    col->setContentsMargins(Theme::S4, Theme::S4, Theme::S4, Theme::S3);
    col->setSpacing(Theme::S2);

    auto* header = new QHBoxLayout();
    auto* brand = new QLabel(QStringLiteral("ClipStream"), m_card);
    brand->setObjectName(QStringLiteral("brand"));
    auto* mark = new QLabel(m_card);
    mark->setObjectName(QStringLiteral("brandMark"));
    mark->setPixmap(QPixmap(QStringLiteral(":/icon.png")).scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    mark->setFixedSize(28, 28);
    mark->setAlignment(Qt::AlignCenter);
    header->addWidget(mark);
    header->addSpacing(4);
    header->addWidget(brand);
    header->addStretch();

    m_pauseBtn = new QToolButton(m_card);
    m_pauseBtn->setObjectName(QStringLiteral("capture"));
    m_pauseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pauseBtn, &QToolButton::clicked, this, [this] {
        if (!m_monitor) return;
        const bool paused = !m_monitor->isPaused();
        m_monitor->setPaused(paused);
        m_db->setSetting(QStringLiteral("paused"), paused ? QStringLiteral("1") : QStringLiteral("0"));
        updateCaptureState();
    });
    header->addWidget(m_pauseBtn);
    m_settingsBtn = new QToolButton(m_card);
    m_settingsBtn->setObjectName(QStringLiteral("iconBtn"));
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setFixedSize(32, 32);
    m_settingsBtn->setIconSize(QSize(18, 18));
    m_settingsBtn->setToolTip(QStringLiteral("Settings"));
    m_settingsBtn->setAccessibleName(QStringLiteral("Settings"));
    connect(m_settingsBtn, &QToolButton::clicked, this, &OverlayWindow::openSettings);
    header->addWidget(m_settingsBtn);
    col->addLayout(header);
    auto* searchRow = new QHBoxLayout();
    m_searchIcon = new QLabel(m_card);
    m_searchIcon->setFixedSize(20, 20);
    searchRow->addWidget(m_searchIcon);
    m_search = new QLineEdit(m_card);
    m_search->setObjectName(QStringLiteral("search"));
    m_search->setPlaceholderText(QStringLiteral("Search clips or apps…"));
    m_search->setAccessibleName(QStringLiteral("Search clipboard"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumHeight(22);
    m_search->installEventFilter(this);
    searchRow->addWidget(m_search, 1);
    col->addLayout(searchRow);

    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(4);
    m_filters = new QButtonGroup(this);
    const QStringList labels = {QStringLiteral("All clips"), QStringLiteral("Pinned"),
        QStringLiteral("Text"), QStringLiteral("Images"), QStringLiteral("Links")};
    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(labels[i], m_card);
        button->setObjectName(QStringLiteral("filter"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        m_filters->addButton(button, i);
        filterRow->addWidget(button);
    }
    m_filters->button(0)->setChecked(true);
    filterRow->addStretch();
    connect(m_filters, &QButtonGroup::idClicked, this, [this](int id) {
        m_filter = static_cast<ClipFilter>(id);
        reload();
        selectRow(0);
        m_search->setFocus();
    });
    col->addLayout(filterRow);

    auto* section = new QHBoxLayout();
    m_count = new QLabel(m_card);
    m_count->setObjectName(QStringLiteral("count"));
    section->addWidget(m_count);
    section->addStretch();
    m_newBtn = new QToolButton(m_card);
    m_newBtn->setObjectName(QStringLiteral("newSnippet"));
    m_newBtn->setText(QStringLiteral("New snippet"));
    m_newBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_newBtn->setToolTip(QStringLiteral("Save reusable text · Ctrl+N"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    connect(m_newBtn, &QToolButton::clicked, this, &OverlayWindow::newSnippet);
    section->addWidget(m_newBtn);
    col->addLayout(section);

    // --- History list ---------------------------------------------------------
    m_model = new HistoryModel(this);
    m_delegate = new EntryDelegate(this);
    m_list = new QListView(m_card);
    m_list->setObjectName(QStringLiteral("list"));
    m_list->setAccessibleName(QStringLiteral("Clipboard history"));
    m_list->setModel(m_model);
    m_list->setItemDelegate(m_delegate);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setMouseTracking(true);
    m_list->viewport()->setMouseTracking(true); // hover state → inline buttons
    m_list->installEventFilter(this);
    col->addWidget(m_list, 1);

    connect(m_list, &QListView::clicked, this, [this](const QModelIndex&) { pasteCurrent(); });
    connect(m_list, &QListView::customContextMenuRequested, this,
            [this](const QPoint& pos) {
                const QModelIndex index = m_list->indexAt(pos);
                if (!index.isValid()) return;
                m_list->setCurrentIndex(index);
                showContextMenu(m_list->viewport()->mapToGlobal(pos));
            });
    // Floating action bar over the selected row (pin/copy/edit/delete).
    m_actions = new RowActionsBar(m_list->viewport());
    m_actions->hide();
    connect(m_actions, &RowActionsBar::pinClicked, this, [this] { pinCurrent(); });
    connect(m_actions, &RowActionsBar::copyClicked, this, [this] { copyCurrent(); });
    connect(m_actions, &RowActionsBar::editClicked, this, [this] { editCurrent(); });
    connect(m_actions, &RowActionsBar::deleteClicked, this, [this] { deleteCurrent(); });
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { positionActionsBar(); });
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this] { positionActionsBar(); });

    m_empty = new QWidget(m_card);
    auto* emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->addStretch();
    m_emptyIcon = new QLabel(m_empty);
    m_emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyIcon);
    m_emptyTitle = new QLabel(m_empty);
    m_emptyTitle->setObjectName(QStringLiteral("emptyTitle"));
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyTitle);
    m_emptyHint = new QLabel(m_empty);
    m_emptyHint->setObjectName(QStringLiteral("emptyHint"));
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyHint);
    emptyLayout->addStretch();
    m_empty->hide();
    col->addWidget(m_empty, 1);

    auto* footerRow = new QHBoxLayout();
    auto* footer = new QLabel(QStringLiteral("Click to paste · Ctrl+F Search"), m_card);
    footer->setObjectName(QStringLiteral("footer"));
    footerRow->addWidget(footer);
    footerRow->addStretch();
    m_previewBtn = new QPushButton(QStringLiteral("Preview"), m_card);
    m_previewBtn->setObjectName(QStringLiteral("previewButton"));
    m_previewBtn->setToolTip(QStringLiteral("Preview full clip · Ctrl+Space"));
    connect(m_previewBtn, &QPushButton::clicked, this, &OverlayWindow::previewCurrent);
    footerRow->addWidget(m_previewBtn);
    col->addLayout(footerRow);

    connect(m_search, &QLineEdit::textChanged, this, [this] { reload(); selectRow(0); });

    setFixedSize(Theme::OverlayWidth + 2 * Theme::ShadowMargin,
                 Theme::OverlayHeight + 2 * Theme::ShadowMargin);

    m_fade = new QPropertyAnimation(this, "windowOpacity", this);
    m_fade->setDuration(110);
    m_fade->setStartValue(0.0);
    m_fade->setEndValue(1.0);
    m_fade->setEasingCurve(QEasingCurve::OutCubic);

    applyTheme();
}

void OverlayWindow::applyTheme() {
    const Theme::Palette& p = Theme::palette();
    QString css = QStringLiteral(
        "QWidget { color:@text; }"
        "#card { background:@surface; border:1px solid @border; border-radius:18px; }"
        "#brand { font-size:17px; font-weight:700; letter-spacing:-0.5px; }"
        "#brandMark { background:transparent; }"
        "#subtitle, #emptyHint { color:@muted; font-size:12px; }"
        "#search { background:@alt; border:1px solid @border; border-radius:10px;"
        " padding:6px 10px; font-size:13px; selection-background-color:@accent; }"
        "#search:focus { border-color:@accent; }"
        "#count { color:@muted; font-size:11px; font-weight:600; padding:4px 0; }"
        "QToolButton, QPushButton { background:transparent; border:1px solid transparent;"
        " border-radius:7px; padding:6px 9px; }"
        "QToolButton:hover, QPushButton:hover { background:@alt; }"
        "#iconBtn { padding:0; }"
        "QToolButton:focus, QPushButton:focus { border-color:@accent; }"
        "#filter { color:@muted; font-size:12px; padding:5px 9px; }"
        "#filter:checked { color:@text; background:@alt; border:1px solid @border; font-weight:600; }"
        "#capture { color:@accent; font-size:11px; background:@alt; }"
        "#newSnippet { color:@accent; font-size:12px; }"
        "#list { background:transparent; outline:none; }"
        "#list::item { border:none; }"
        "#footer { color:@muted; font-size:11px; }"
        "#previewButton { color:@muted; font-size:11px; border-color:@border; }"
        "#previewButton:disabled { color:@border; }"
        "#emptyIcon { color:@accent; font-size:38px; }"
        "#emptyTitle { font-size:18px; font-weight:600; padding:8px; }"
        "QDialog, QMessageBox { background:@surface; }"
        "QPlainTextEdit { background:@alt; border:1px solid @border; border-radius:8px; padding:12px; }"
        "QMenu { background:@surface; color:@text; border:1px solid @border; padding:6px; }"
        "QMenu::item { padding:8px 24px; border-radius:4px; }"
        "QMenu::item:selected { background:@alt; }"
        "QScrollBar:vertical { background:transparent; width:8px; margin:2px; }"
        "QScrollBar::handle:vertical { background:@border; border-radius:3px; min-height:24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }");
    css.replace(QStringLiteral("@surface"), p.surface);
    css.replace(QStringLiteral("@border"), p.border);
    css.replace(QStringLiteral("@accent"), p.accent);
    css.replace(QStringLiteral("@text"), p.textPrimary);
    css.replace(QStringLiteral("@muted"), p.textMuted);
    css.replace(QStringLiteral("@alt"), p.surfaceAlt);
    setStyleSheet(css);
    m_emptyIcon->setPixmap(IconFactory::pixmap(QStringLiteral("search"), QColor(p.accent), 36));
    m_newBtn->setIcon(IconFactory::icon(QStringLiteral("plus"), QColor(p.accent), 14));
    updateCaptureState();
    resizeOverlay();

    // Themed icons (recoloured per palette).
    if (m_searchIcon)
        m_searchIcon->setPixmap(IconFactory::pixmap(QStringLiteral("search"), QColor(p.textMuted), 16));
    if (m_settingsBtn)
        m_settingsBtn->setIcon(IconFactory::icon(QStringLiteral("settings"), QColor(p.textMuted), 16));
    if (m_actions)
        m_actions->retheme();
}

void OverlayWindow::updateCaptureState() {
    const bool paused = m_monitor && m_monitor->isPaused();
    m_pauseBtn->setText(paused ? QStringLiteral("Paused") : QStringLiteral("●  Capturing"));
    m_pauseBtn->setToolTip(paused ? QStringLiteral("Resume clipboard capture") : QStringLiteral("Pause clipboard capture"));
    m_pauseBtn->setAccessibleName(m_pauseBtn->toolTip());
}

void OverlayWindow::reload() {
    const int keepRow = currentRow();
    const qint64 keepId = currentEntry() ? currentEntry()->id : -1;
    m_model->setEntries(m_db->search(m_search->text(), 200, m_filter));
    const int count = m_model->rowCount();
    m_count->setText(QStringLiteral("%1%2 %3").arg(count).arg(count == 200 ? QStringLiteral("+") : QString())
        .arg(m_search->text().trimmed().isEmpty() ? QStringLiteral("clips") : QStringLiteral("results")));
    m_list->setVisible(count > 0);
    m_empty->setVisible(count == 0);
    m_previewBtn->setEnabled(count > 0);
    const bool searching = !m_search->text().trimmed().isEmpty();
    m_emptyTitle->setText(searching ? QStringLiteral("No matching clips") :
        m_filter == ClipFilter::All ? QStringLiteral("Your next idea starts here") : QStringLiteral("Nothing here yet"));
    m_emptyHint->setText(searching ? QStringLiteral("Try a different word or app name, or switch to All clips.") :
        m_filter == ClipFilter::Pinned ? QStringLiteral("Pin a useful clip to keep it close. Or create your first snippet above.") :
        QStringLiteral("Copy text or an image and it will appear here.\nOpen your history anytime with Ctrl+Shift+V."));
    int row = qBound(0, keepRow, qMax(0, count - 1));
    for (int i = 0; i < count; ++i)
        if (m_model->entryAt(i).id == keepId) { row = i; break; }
    if (count > 0) selectRow(row);
    positionActionsBar();
    updateCaptureState();
}

void OverlayWindow::selectRow(int row) {
    const QModelIndex idx = m_model->index(row, 0);
    if (idx.isValid()) {
        m_list->setCurrentIndex(idx);
        m_list->scrollTo(idx, QAbstractItemView::EnsureVisible);
    }
}

int OverlayWindow::currentRow() const {
    return m_list->currentIndex().row();
}

bool OverlayWindow::hasSelection() const {
    return m_model->isValidRow(currentRow());
}

const ClipEntry* OverlayWindow::currentEntry() const {
    const int row = currentRow();
    return m_model->isValidRow(row) ? &m_model->entryAt(row) : nullptr;
}

void OverlayWindow::resizeOverlay() {
    const bool comfortable = m_db->setting(QStringLiteral("overlay_size")) == QLatin1String("comfortable");
    const QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QSize available = screen->availableGeometry().size();
    setFixedSize(qMin((comfortable ? 520 : Theme::OverlayWidth) + 2 * Theme::ShadowMargin, available.width()),
                 qMin((comfortable ? 600 : Theme::OverlayHeight) + 2 * Theme::ShadowMargin, available.height()));
    if (isVisible()) { // switching size in Settings must not push the popup off screen
        const QRect area = screen->availableGeometry();
        move(qBound(area.left(), x(), qMax(area.left(), area.right() - width() + 1)),
             qBound(area.top(), y(), qMax(area.top(), area.bottom() - height() + 1)));
    }
    QTimer::singleShot(0, this, &OverlayWindow::positionActionsBar);
}

void OverlayWindow::activateForSearch() {
    m_popupInput->stop();
    m_browsingWithoutFocus = false;
    platform::setPopupNonActivating(winId(), false);
    activateWindow();
    m_search->setFocus();
    hideIfAbandoned();
}

void OverlayWindow::showAtCursor() {
    if (m_pasting) return;
    m_pasteTarget = platform::capturePasteTarget();
    m_browsingWithoutFocus = m_pasteTarget.window != 0;
    platform::setPopupNonActivating(winId(), m_browsingWithoutFocus);
    m_search->clear();          // textChanged → reload() with full history
    reload();
    selectRow(0);

    const QPoint cursor = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();

    resizeOverlay();
    int x = cursor.x() - width() / 2;
    int y = cursor.y() + 12;
    x = qBound(area.left(), x, area.right() - width() + 1);
    if (y + height() > area.bottom())
        y = cursor.y() - height() - 12;
    y = qBound(area.top(), y, area.bottom() - height() + 1);

    move(x, y);
    setWindowOpacity(0.0); // fade in from transparent
    show();
    raise();
    if (m_browsingWithoutFocus) m_popupInput->start(this, m_pasteTarget);
    else activateForSearch();
    m_fade->start();
    // The list lays out during show(); reposition the action bar once geometry
    // is final, otherwise it lands in the wrong spot on the very first open.
    QTimer::singleShot(0, this, [this] { positionActionsBar(); });
}

void OverlayWindow::toggleAtCursor() {
    // A settings/edit dialog owns the interaction; bring it forward instead of
    // hiding or re-targeting the popup underneath it.
    if (QWidget* modal = QApplication::activeModalWidget()) {
        modal->raise();
        modal->activateWindow();
        return;
    }
    if (QWidget* popup = QApplication::activePopupWidget())
        popup->close();
    if (isVisible())
        hide();
    else
        showAtCursor();
}

// A menu closes when another app takes focus, and activation can be refused;
// either way an inactive popup with no destination must not linger on screen.
void OverlayWindow::hideIfAbandoned() {
    QTimer::singleShot(250, this, [this] {
        if (isVisible() && !m_browsingWithoutFocus && !m_childDialogOpen && !m_pasting
            && !isActiveWindow() && !QApplication::activePopupWidget())
            hide();
    });
}

// Another app can hold the clipboard open for a moment, and the write is then
// dropped without an error. Pasting after that would insert the previous
// clipboard content, so confirm the write landed and retry briefly if not.
bool OverlayWindow::writeClipboard(const std::function<void(QClipboard*)>& write) {
    QClipboard* cb = QApplication::clipboard();
    for (int attempt = 0; attempt < 6; ++attempt) {
        if (attempt) QThread::msleep(25);
        if (m_monitor) m_monitor->ignoreNextChange();
        write(cb);
        if (platform::ownsClipboard()) return true;
    }
    if (m_monitor) m_monitor->ignoreNextChange(false); // nothing changed, so nothing to skip
    emit clipboardBusy();
    return false;
}

bool OverlayWindow::putOnClipboard(const ClipEntry& entry, PasteFormat format) {
    if (entry.isImage()) {
        const QImage img(entry.imagePath);
        if (!img.isNull())
            return writeClipboard([&img](QClipboard* cb) { cb->setImage(img); });
        QScopedValueRollback<bool> dialogGuard(m_childDialogOpen, true);
        activateForSearch();
        QMessageBox::information(this, QStringLiteral("Image unavailable"),
            QStringLiteral("This image is no longer available on disk. Copy it again to add it to your history."));
        return false;
    }

    QString text = entry.content;
    switch (format) {
        case PasteFormat::Upper: text = text.toUpper(); break;
        case PasteFormat::Lower: text = text.toLower(); break;
        case PasteFormat::Title: text = toTitleCase(text); break;
        case PasteFormat::Trim:  text = text.trimmed();   break;
        case PasteFormat::Plain: break;
    }
    return writeClipboard([&text](QClipboard* cb) { cb->setText(text); });
}

void OverlayWindow::pasteCurrent(PasteFormat format) {
    const ClipEntry* e = currentEntry();
    if (!e || m_pasting)
        return;
    const ClipEntry entry = *e;
    if (!putOnClipboard(entry, format)) return;
    finishPaste();
}

void OverlayWindow::finishPaste() {
    if (m_pasting) return;
    m_pasting = true;
    // Hand focus back while we still own it; Windows always allows that.
    if (isActiveWindow()) platform::restorePasteTarget(m_pasteTarget);
    hide();
    platform::pasteToTarget(m_pasteTarget, this, [this](bool ok) {
        m_pasting = false;
        if (!ok) emit pasteFailed();
    });
}

void OverlayWindow::copyCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    if (!putOnClipboard(*e, PasteFormat::Plain)) return;
    // Stay open and confirm visually instead of hiding, so the copy feels acknowledged.
    if (m_actions && m_actions->isVisible())
        m_actions->flashCopied();
}

void OverlayWindow::pinCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    m_db->togglePin(e->id);
    reload();
}

void OverlayWindow::editCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e || e->isImage())
        return;
    const qint64 id = e->id;
    QScopedValueRollback<bool> dialogGuard(m_childDialogOpen, true);
    activateForSearch();
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, QStringLiteral("Edit clip"), QStringLiteral("Content:"), e->content, &ok);
    if (ok) {
        m_db->updateContent(id, text);
        reload();
    }
    activateWindow();
    m_search->setFocus();
}

void OverlayWindow::deleteCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    const int row = currentRow();
    const QString imagePath = e->imagePath;
    if (!m_db->removeEntry(e->id)) return;
    if (!imagePath.isEmpty()) QFile::remove(imagePath);
    reload();
    if (m_model->rowCount() > 0)
        selectRow(qMin(row, m_model->rowCount() - 1));
}

void OverlayWindow::showFormatMenu() {
    if (!hasSelection() || currentEntry()->isImage())
        return;
    QScopedValueRollback<bool> dialogGuard(m_childDialogOpen, true);
    activateForSearch();
    QMenu menu(this);
    menu.addAction(QStringLiteral("Plain"),  [this] { pasteCurrent(PasteFormat::Plain); });
    menu.addAction(QStringLiteral("UPPERCASE"), [this] { pasteCurrent(PasteFormat::Upper); });
    menu.addAction(QStringLiteral("lowercase"), [this] { pasteCurrent(PasteFormat::Lower); });
    menu.addAction(QStringLiteral("Title Case"), [this] { pasteCurrent(PasteFormat::Title); });
    menu.addAction(QStringLiteral("Trim whitespace"), [this] { pasteCurrent(PasteFormat::Trim); });
    const QRect r = m_list->visualRect(m_list->currentIndex());
    menu.exec(m_list->viewport()->mapToGlobal(r.bottomLeft()));
    hideIfAbandoned();
}

void OverlayWindow::showContextMenu(const QPoint& globalPos) {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    QScopedValueRollback<bool> dialogGuard(m_childDialogOpen, true);
    activateForSearch();
    QMenu menu(this);
    addSmartActions(menu, *e); // type-specific actions first, if any
    menu.addAction(QStringLiteral("Preview   Ctrl+Space"), [this] { previewCurrent(); });
    menu.addAction(QStringLiteral("Paste"), [this] { pasteCurrent(); });
    menu.addAction(QStringLiteral("Copy"), [this] { copyCurrent(); });
    menu.addAction(e->pinned ? QStringLiteral("Unpin") : QStringLiteral("Pin"),
                   [this] { pinCurrent(); });
    if (!e->isImage())
        menu.addAction(QStringLiteral("Edit…"), [this] { editCurrent(); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Delete"), [this] { deleteCurrent(); });
    menu.exec(globalPos);
    hideIfAbandoned();
}

// Adds context-menu entries that only make sense for this clip's type:
// open links, open/reveal files, convert colours, evaluate arithmetic.
void OverlayWindow::addSmartActions(QMenu& menu, const ClipEntry& entry) {
    bool added = false;

    if (entry.type == ContentType::Url) {
        const QString url = entry.content.trimmed();
        menu.addAction(QStringLiteral("Open link"), [this, url] {
            hide();
            QDesktopServices::openUrl(QUrl::fromUserInput(url));
        });
        added = true;
    } else if (entry.type == ContentType::FilePath) {
        const QString path = entry.content.trimmed();
        menu.addAction(QStringLiteral("Open file"), [this, path] {
            hide();
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        menu.addAction(QStringLiteral("Show in folder"), [path] {
            QProcess::startDetached(QStringLiteral("explorer.exe"),
                                    {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
        });
        added = true;
    } else if (entry.type == ContentType::Color) {
        const QColor c(entry.content.trimmed());
        if (c.isValid()) {
            auto* sub = menu.addMenu(QStringLiteral("Copy colour as"));
            const QString hex = c.name(QColor::HexRgb);
            const QString rgb = colorToRgb(c);
            const QString hsl = colorToHsl(c);
            sub->addAction(hex, [this, hex] { copyRawText(hex); });
            sub->addAction(rgb, [this, rgb] { copyRawText(rgb); });
            sub->addAction(hsl, [this, hsl] { copyRawText(hsl); });
            added = true;
        }
    }

    if (const auto result = MathEval::evaluate(entry.content)) {
        const QString text = QString::number(*result, 'g', 12);
        menu.addAction(QStringLiteral("Paste result = %1").arg(text), [this, text] {
            if (writeClipboard([&text](QClipboard* cb) { cb->setText(text); }))
                finishPaste();
        });
        added = true;
    }

    if (added)
        menu.addSeparator();
}

void OverlayWindow::runPrimarySmartAction() {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    if (e->type == ContentType::Url) {
        const QString url = e->content.trimmed();
        hide();
        QDesktopServices::openUrl(QUrl::fromUserInput(url));
    } else if (e->type == ContentType::FilePath) {
        const QString path = e->content.trimmed();
        hide();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void OverlayWindow::copyRawText(const QString& text) {
    writeClipboard([&text](QClipboard* cb) { cb->setText(text); });
}

void OverlayWindow::newSnippet() {
    QScopedValueRollback<bool> dialogGuard(m_childDialogOpen, true);
    activateForSearch();
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, QStringLiteral("New snippet"),
        QStringLiteral("Reusable text (saved pinned):"), QString(), &ok);
    if (ok && !text.trimmed().isEmpty()) {
        ClipEntry e;
        e.content = text;
        e.sourceApp = QStringLiteral("Snippet");
        e.type = ContentClassifier::classify(text);
        e.sensitive = ContentClassifier::looksSensitive(text);
        const qint64 id = m_db->insertEntry(e);
        if (const auto saved = m_db->entryById(id); saved && !saved->pinned)
            m_db->togglePin(id);
        m_filter = ClipFilter::All;
        m_filters->button(0)->setChecked(true);
        m_search->clear();
        reload();
        selectRow(0);
    }
    activateWindow();
    m_search->setFocus();
}

void OverlayWindow::previewCurrent() {
    if (!currentEntry()) return;
    const ClipEntry entry = *currentEntry();
    QScopedValueRollback<bool> dialogGuard(m_childDialogOpen, true);
    activateForSearch();
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Clip preview"));
    dialog.resize(560, 440);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto* title = new QLabel(entry.sensitive ? QStringLiteral("Sensitive clip") : QStringLiteral("Clip preview"), &dialog);
    title->setObjectName(QStringLiteral("emptyTitle"));
    layout->addWidget(title);
    auto* meta = new QLabel(QStringLiteral("%1 · %2")
        .arg(entry.sourceApp.isEmpty() ? QStringLiteral("Clipboard") : entry.sourceApp,
             entry.createdAt.toLocalTime().toString(QStringLiteral("ddd, d MMM · h:mm AP"))), &dialog);
    meta->setTextFormat(Qt::PlainText);
    meta->setObjectName(QStringLiteral("subtitle"));
    layout->addWidget(meta);
    if (entry.isImage()) {
        auto* scroll = new QScrollArea(&dialog);
        auto* image = new QLabel(scroll);
        const QPixmap pixmap(entry.imagePath);
        if (pixmap.isNull()) image->setText(QStringLiteral("This image is no longer available on disk."));
        else image->setPixmap(pixmap);
        image->setAlignment(Qt::AlignCenter);
        scroll->setWidget(image);
        scroll->setWidgetResizable(true);
        layout->addWidget(scroll, 1);
    } else {
        auto* text = new QPlainTextEdit(&dialog);
        text->setReadOnly(true);
        text->setAccessibleName(QStringLiteral("Clip content"));
        text->setPlainText(entry.sensitive ? QStringLiteral("This clip is hidden because it may contain a password or secret.") : entry.content);
        layout->addWidget(text, 1);
        if (entry.sensitive) {
            auto* reveal = new QPushButton(QStringLiteral("Reveal content"), &dialog);
            reveal->setCheckable(true);
            connect(reveal, &QPushButton::toggled, text, [text, reveal, entry](bool shown) {
                text->setPlainText(shown ? entry.content : QStringLiteral("Sensitive content is hidden."));
                reveal->setText(shown ? QStringLiteral("Hide content") : QStringLiteral("Reveal content"));
            });
            layout->addWidget(reveal);
        }
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* copy = buttons->addButton(QStringLiteral("Copy clip"), QDialogButtonBox::ActionRole);
    connect(copy, &QPushButton::clicked, this, [this, entry, copy] {
        if (putOnClipboard(entry, PasteFormat::Plain))
            copy->setText(QStringLiteral("Copied"));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
    activateWindow();
    m_search->setFocus();
}

void OverlayWindow::positionActionsBar() {
    const ClipEntry* e = currentEntry();
    const QModelIndex idx = m_list->currentIndex();
    const QRect rect = idx.isValid() ? m_list->visualRect(idx) : QRect();
    // Hide when there's no selection or the row is scrolled out of view.
    if (!e || rect.isEmpty() || rect.bottom() <= 0 || rect.top() >= m_list->viewport()->height()) {
        m_actions->hide();
        return;
    }

    m_actions->configure(e->pinned, !e->isImage());
    const int barW = m_actions->widthFor(!e->isImage());
    const int barH = 28;
    const int x = rect.right() - Theme::S2 - barW;
    const int y = rect.bottom() - barH - 4;
    m_actions->setGeometry(x, y, barW, barH);
    m_actions->show();
    m_actions->raise();
}

void OverlayWindow::openSettings() {
    QScopedValueRollback<bool> dialogGuard(m_childDialogOpen, true);
    activateForSearch();
    SettingsDialog dlg(m_db, this);
    connect(&dlg, &SettingsDialog::pauseToggled, this,
            [this](bool paused) { if (m_monitor) m_monitor->setPaused(paused); });
    connect(&dlg, &SettingsDialog::settingsChanged, this, [this] {
        applyTheme();
        m_list->viewport()->update();
        reload();
    });
    dlg.exec();
    applyTheme();
    reload();
    activateWindow();
    m_search->setFocus();
}

bool OverlayWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_search && event->type() == QEvent::MouseButtonPress)
        activateForSearch();
    if ((watched == m_search || watched == m_list) && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        const int key = ke->key();
        const Qt::KeyboardModifiers mods = ke->modifiers();

        // Ctrl+1..9 → quick-paste the Nth visible clip.
        if ((mods & Qt::ControlModifier) && key >= Qt::Key_1 && key <= Qt::Key_9) {
            const int row = key - Qt::Key_1;
            if (m_model->isValidRow(row)) {
                selectRow(row);
                pasteCurrent();
            }
            return true;
        }

        if (mods & Qt::ControlModifier) {
            if (key == Qt::Key_Space) { previewCurrent(); return true; }
            if (key == Qt::Key_F) { activateForSearch(); m_search->selectAll(); return true; }
            if (key == Qt::Key_P) { pinCurrent(); return true; }
            if (key == Qt::Key_O) { runPrimarySmartAction(); return true; } // open url/file
            if (key == Qt::Key_N) { newSnippet(); return true; }           // new snippet
        }

        switch (key) {
            case Qt::Key_Down:
                if (m_model->rowCount() > 0)
                    selectRow(qMin(currentRow() + 1, m_model->rowCount() - 1));
                return true;
            case Qt::Key_Up:
                if (m_model->rowCount() > 0)
                    selectRow(qMax(currentRow() - 1, 0));
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (mods & Qt::ShiftModifier)
                    showFormatMenu();
                else
                    pasteCurrent();
                return true;
            case Qt::Key_Escape:
                hide();
                return true;
            case Qt::Key_F2:
                editCurrent();
                return true;
            case Qt::Key_Delete:
                if (mods & Qt::ShiftModifier) {
                    deleteCurrent();
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void OverlayWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

void OverlayWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow()
        && !m_browsingWithoutFocus && !m_childDialogOpen && !QApplication::activePopupWidget())
        hide();
    QWidget::changeEvent(event);
}

bool OverlayWindow::nativeEvent(const QByteArray& type, void* message, qintptr* result) {
    if (platform::handlePopupNativeEvent(message, result)) return true;
    return QWidget::nativeEvent(type, message, result);
}

void OverlayWindow::hideEvent(QHideEvent* event) {
    m_popupInput->stop();
    // Only restore focus when we owned it, never on an outside click.
    if (isActiveWindow()) platform::restorePasteTarget(m_pasteTarget);
    QWidget::hideEvent(event);
}
