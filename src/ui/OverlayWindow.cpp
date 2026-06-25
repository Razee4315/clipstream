#include "ui/OverlayWindow.h"

#include "core/ClipboardMonitor.h"
#include "core/ContentClassifier.h"
#include "core/Database.h"
#include "core/MathEval.h"
#include "platform/PasteSimulator.h"
#include "theme.h"
#include "ui/EntryDelegate.h"
#include "ui/HistoryModel.h"
#include "ui/IconFactory.h"
#include "ui/RowActionsBar.h"
#include "ui/SettingsDialog.h"

#include <QApplication>
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
    col->setContentsMargins(Theme::S3, Theme::S3, Theme::S3, Theme::S2);
    col->setSpacing(Theme::S2);

    // --- Search row -----------------------------------------------------------
    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(Theme::S2);

    m_searchIcon = new QLabel(m_card);
    m_searchIcon->setFixedSize(18, 18);
    searchRow->addWidget(m_searchIcon);

    m_search = new QLineEdit(m_card);
    m_search->setObjectName(QStringLiteral("search"));
    m_search->setPlaceholderText(QStringLiteral("Search clipboard…"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    searchRow->addWidget(m_search, 1);

    m_count = new QLabel(QStringLiteral("0"), m_card);
    m_count->setObjectName(QStringLiteral("count"));
    searchRow->addWidget(m_count);

    m_settingsBtn = new QToolButton(m_card);
    m_settingsBtn->setObjectName(QStringLiteral("iconBtn"));
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setFocusPolicy(Qt::NoFocus);
    m_settingsBtn->setFixedSize(28, 28);
    m_settingsBtn->setIconSize(QSize(16, 16));
    m_settingsBtn->setToolTip(QStringLiteral("Settings"));
    connect(m_settingsBtn, &QToolButton::clicked, this, &OverlayWindow::openSettings);
    searchRow->addWidget(m_settingsBtn);

    col->addLayout(searchRow);

    // --- History list ---------------------------------------------------------
    m_model = new HistoryModel(this);
    m_delegate = new EntryDelegate(this);
    m_list = new QListView(m_card);
    m_list->setObjectName(QStringLiteral("list"));
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

    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex&) { pasteCurrent(); });
    connect(m_list, &QListView::customContextMenuRequested, this,
            [this](const QPoint& pos) { showContextMenu(m_list->viewport()->mapToGlobal(pos)); });
    // Hovering a row selects it, so the action buttons appear on hover.
    connect(m_list, &QListView::entered, this, [this](const QModelIndex& idx) {
        if (idx.isValid())
            m_list->setCurrentIndex(idx);
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

    // --- Footer hints ---------------------------------------------------------
    auto* footer = new QLabel(
        QStringLiteral("↑↓ Move     ⏎ Paste     Ctrl+N New     Esc Close"), m_card);
    footer->setObjectName(QStringLiteral("footer"));
    footer->setAlignment(Qt::AlignCenter);
    col->addWidget(footer);

    connect(m_search, &QLineEdit::textChanged, this, [this] { reload(); });

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
    setStyleSheet(
        QStringLiteral(
            "#card { background-color:%1; border:1px solid %2; border-radius:%3px; }"
            "#search { background-color:%4; border:1px solid %2; border-radius:%5px;"
            "         padding:6px 10px; color:%6; font-size:%7px; selection-background-color:%8; }"
            "#search:focus { border:1px solid %8; }"
            "#count { color:%9; font-size:%10px; padding:0 4px; }"
            "#iconBtn { background:transparent; border:none; color:%9; font-size:15px; border-radius:6px; }"
            "#iconBtn:hover { background-color:%4; color:%6; }"
            "#list { background:transparent; }"
            "#list::item { border:none; }"
            "#footer { color:%9; font-size:%11px; padding-top:4px; }"
            "QScrollBar:vertical { background:transparent; width:8px; margin:2px; }"
            "QScrollBar::handle:vertical { background:%2; border-radius:4px; min-height:24px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
            .arg(p.surface)       // 1
            .arg(p.border)        // 2
            .arg(Theme::RadiusLg) // 3
            .arg(p.surfaceAlt)    // 4
            .arg(Theme::RadiusMd) // 5
            .arg(p.textPrimary)   // 6
            .arg(Theme::FsBody)   // 7
            .arg(p.accent)        // 8
            .arg(p.textMuted)     // 9
            .arg(Theme::FsMeta)   // 10
            .arg(Theme::FsMicro));// 11

    // Themed icons (recoloured per palette).
    if (m_searchIcon)
        m_searchIcon->setPixmap(IconFactory::pixmap(QStringLiteral("search"), QColor(p.textMuted), 16));
    if (m_settingsBtn)
        m_settingsBtn->setIcon(IconFactory::icon(QStringLiteral("settings"), QColor(p.textMuted), 16));
    if (m_actions)
        m_actions->retheme();
}

void OverlayWindow::reload() {
    const int keepRow = currentRow();
    m_model->setEntries(m_db->search(m_search->text()));
    m_count->setText(QString::number(m_model->rowCount()));
    if (m_model->rowCount() > 0)
        selectRow(qBound(0, keepRow < 0 ? 0 : keepRow, m_model->rowCount() - 1));
    else
        m_actions->hide();
    positionActionsBar();
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

void OverlayWindow::showAtCursor() {
    m_search->clear();          // textChanged → reload() with full history
    reload();
    selectRow(0);

    const QPoint cursor = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();

    int x = cursor.x() - width() / 2;
    int y = cursor.y() + 12;
    x = qBound(area.left(), x, area.right() - width());
    if (y + height() > area.bottom())
        y = cursor.y() - height() - 12;
    y = qBound(area.top(), y, area.bottom() - height());

    move(x, y);
    setWindowOpacity(0.0); // fade in from transparent
    show();
    raise();
    activateWindow();
    m_search->setFocus();
    m_fade->start();
}

void OverlayWindow::toggleAtCursor() {
    if (isVisible())
        hide();
    else
        showAtCursor();
}

void OverlayWindow::putOnClipboard(const ClipEntry& entry, PasteFormat format) {
    QClipboard* cb = QApplication::clipboard();
    if (m_monitor)
        m_monitor->ignoreNextChange();

    if (entry.isImage() && !entry.imagePath.isEmpty()) {
        const QImage img(entry.imagePath);
        if (!img.isNull())
            cb->setImage(img);
        return;
    }

    QString text = entry.content;
    switch (format) {
        case PasteFormat::Upper: text = text.toUpper(); break;
        case PasteFormat::Lower: text = text.toLower(); break;
        case PasteFormat::Title: text = toTitleCase(text); break;
        case PasteFormat::Trim:  text = text.trimmed();   break;
        case PasteFormat::Plain: break;
    }
    cb->setText(text);
}

void OverlayWindow::pasteCurrent(PasteFormat format) {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    hide();
    putOnClipboard(*e, format);
    // Let focus return to the previously active window before sending Ctrl+V.
    QTimer::singleShot(80, [] { platform::simulatePaste(); });
}

void OverlayWindow::copyCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    putOnClipboard(*e, PasteFormat::Plain);
    // Stay open and confirm visually instead of hiding, so the copy feels acknowledged.
    if (m_actions && m_actions->isVisible())
        m_actions->flashCopied();
}

void OverlayWindow::pinCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    const int row = currentRow();
    m_db->togglePin(e->id);
    reload();
    selectRow(row);
}

void OverlayWindow::editCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e || e->isImage())
        return;
    const qint64 id = e->id;
    const int row = currentRow();
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, QStringLiteral("Edit clip"), QStringLiteral("Content:"), e->content, &ok);
    if (ok) {
        m_db->updateContent(id, text);
        reload();
        selectRow(row);
    }
    activateWindow();
    m_search->setFocus();
}

void OverlayWindow::deleteCurrent() {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    const int row = currentRow();
    if (e->isImage() && !e->imagePath.isEmpty())
        QFile::remove(e->imagePath);
    m_db->removeEntry(e->id);
    reload();
    if (m_model->rowCount() > 0)
        selectRow(qMin(row, m_model->rowCount() - 1));
}

void OverlayWindow::showFormatMenu() {
    if (!hasSelection())
        return;
    QMenu menu(this);
    menu.addAction(QStringLiteral("Plain"),  [this] { pasteCurrent(PasteFormat::Plain); });
    menu.addAction(QStringLiteral("UPPERCASE"), [this] { pasteCurrent(PasteFormat::Upper); });
    menu.addAction(QStringLiteral("lowercase"), [this] { pasteCurrent(PasteFormat::Lower); });
    menu.addAction(QStringLiteral("Title Case"), [this] { pasteCurrent(PasteFormat::Title); });
    menu.addAction(QStringLiteral("Trim whitespace"), [this] { pasteCurrent(PasteFormat::Trim); });
    const QRect r = m_list->visualRect(m_list->currentIndex());
    menu.exec(m_list->viewport()->mapToGlobal(r.bottomLeft()));
}

void OverlayWindow::showContextMenu(const QPoint& globalPos) {
    const ClipEntry* e = currentEntry();
    if (!e)
        return;
    QMenu menu(this);
    addSmartActions(menu, *e); // type-specific actions first, if any
    menu.addAction(QStringLiteral("Paste"), [this] { pasteCurrent(); });
    menu.addAction(QStringLiteral("Copy"), [this] { copyCurrent(); });
    menu.addAction(e->pinned ? QStringLiteral("Unpin") : QStringLiteral("Pin"),
                   [this] { pinCurrent(); });
    if (!e->isImage())
        menu.addAction(QStringLiteral("Edit…"), [this] { editCurrent(); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Delete"), [this] { deleteCurrent(); });
    menu.exec(globalPos);
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
            hide();
            if (m_monitor)
                m_monitor->ignoreNextChange();
            QApplication::clipboard()->setText(text);
            QTimer::singleShot(80, [] { platform::simulatePaste(); });
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
    if (m_monitor)
        m_monitor->ignoreNextChange();
    QApplication::clipboard()->setText(text);
}

void OverlayWindow::newSnippet() {
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, QStringLiteral("New snippet"),
        QStringLiteral("Reusable text (saved pinned):"), QString(), &ok);
    if (ok && !text.trimmed().isEmpty()) {
        ClipEntry e;
        e.content = text;
        e.sourceApp = QStringLiteral("Snippet");
        e.type = ContentClassifier::classify(text);
        const qint64 id = m_db->insertEntry(e);
        if (id > 0)
            m_db->togglePin(id);
        m_search->clear();
        reload();
        selectRow(0);
    }
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
    const int barH = 34;
    const int x = rect.right() - Theme::S2 - barW;
    const int y = rect.top() + (rect.height() - barH) / 2;
    m_actions->setGeometry(x, y, barW, barH);
    m_actions->show();
    m_actions->raise();
}

void OverlayWindow::openSettings() {
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
    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow())
        hide();
    QWidget::changeEvent(event);
}
