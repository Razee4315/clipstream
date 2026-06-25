#include "ui/OverlayWindow.h"
#include "theme.h"

#include <QCursor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

OverlayWindow::OverlayWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("ClipStream"));
    // Tool window so it stays out of the taskbar; frameless + on-top + translucent
    // gives the rounded floating-card look.
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    buildUi();
}

void OverlayWindow::buildUi() {
    // Outer layout reserves a transparent gutter so the card's drop shadow shows.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::ShadowMargin, Theme::ShadowMargin,
                              Theme::ShadowMargin, Theme::ShadowMargin);

    m_card = new QWidget(this);
    m_card->setObjectName(QStringLiteral("card"));
    outer->addWidget(m_card);

    auto* shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(40);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 160));
    m_card->setGraphicsEffect(shadow);

    auto* inner = new QVBoxLayout(m_card);
    inner->setContentsMargins(Theme::S5, Theme::S5, Theme::S5, Theme::S5);
    inner->setSpacing(Theme::S3);

    auto* title = new QLabel(QStringLiteral("ClipStream"), m_card);
    title->setObjectName(QStringLiteral("title"));

    auto* hint = new QLabel(
        QStringLiteral("Phase 0 — the overlay is alive.\n\n"
                       "Ctrl+Shift+V toggles it. Esc closes it.\n"
                       "History, search and paste land next."),
        m_card);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);

    inner->addWidget(title);
    inner->addWidget(hint);
    inner->addStretch();

    setFixedSize(Theme::OverlayWidth + 2 * Theme::ShadowMargin,
                 Theme::OverlayHeight + 2 * Theme::ShadowMargin);

    // Styling is driven entirely by theme.h tokens (single source of truth).
    setStyleSheet(
        QStringLiteral(
            "#card { background-color: %1; border: 1px solid %2; border-radius: %3px; }"
            "#title { color: %4; font-size: %5px; font-weight: 700; }"
            "#hint  { color: %6; font-size: %7px; }")
            .arg(QString::fromUtf8(Theme::Surface))
            .arg(QString::fromUtf8(Theme::Border))
            .arg(Theme::RadiusLg)
            .arg(QString::fromUtf8(Theme::TextPrimary))
            .arg(Theme::FsTitle)
            .arg(QString::fromUtf8(Theme::TextMuted))
            .arg(Theme::FsBody));
}

void OverlayWindow::showAtCursor() {
    const QPoint cursor = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();

    int x = cursor.x() - width() / 2;
    int y = cursor.y() + 12;

    // Keep the whole card on the active screen.
    x = qBound(area.left(), x, area.right() - width());
    if (y + height() > area.bottom())
        y = cursor.y() - height() - 12; // flip above the cursor if no room below
    y = qBound(area.top(), y, area.bottom() - height());

    move(x, y);
    show();
    raise();
    activateWindow();
}

void OverlayWindow::toggleAtCursor() {
    if (isVisible())
        hide();
    else
        showAtCursor();
}

void OverlayWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

void OverlayWindow::changeEvent(QEvent* event) {
    // Click-away / Alt-Tab: a launcher-style overlay should dismiss itself.
    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow())
        hide();
    QWidget::changeEvent(event);
}
