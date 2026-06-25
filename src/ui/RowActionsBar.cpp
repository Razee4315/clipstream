#include "ui/RowActionsBar.h"

#include "theme.h"
#include "ui/IconFactory.h"

#include <QHBoxLayout>
#include <QTimer>
#include <QToolButton>

namespace {
constexpr int kBtn = 28;
constexpr int kIcon = 16;
} // namespace

RowActionsBar::RowActionsBar(QWidget* parent) : QWidget(parent) {
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(Theme::S1, Theme::S1, Theme::S1, Theme::S1);
    row->setSpacing(2);

    m_pin = makeButton(QStringLiteral("Pin"));
    m_copy = makeButton(QStringLiteral("Copy"));
    m_edit = makeButton(QStringLiteral("Edit"));
    m_delete = makeButton(QStringLiteral("Delete"));

    row->addWidget(m_pin);
    row->addWidget(m_copy);
    row->addWidget(m_edit);
    row->addWidget(m_delete);

    connect(m_pin, &QToolButton::clicked, this, &RowActionsBar::pinClicked);
    connect(m_copy, &QToolButton::clicked, this, &RowActionsBar::copyClicked);
    connect(m_edit, &QToolButton::clicked, this, &RowActionsBar::editClicked);
    connect(m_delete, &QToolButton::clicked, this, &RowActionsBar::deleteClicked);

    setObjectName(QStringLiteral("actionsBar"));
    retheme();
}

QToolButton* RowActionsBar::makeButton(const QString& tip) {
    auto* b = new QToolButton(this);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus); // never steal focus from the search box
    b->setFixedSize(kBtn, kBtn);
    b->setIconSize(QSize(kIcon, kIcon));
    return b;
}

int RowActionsBar::widthFor(bool editable) const {
    const int count = editable ? 4 : 3;
    return count * kBtn + (count - 1) * 2 + 2 * Theme::S1;
}

void RowActionsBar::configure(bool pinned, bool editable) {
    m_pinned = pinned;
    m_edit->setVisible(editable);
    m_pin->setToolTip(pinned ? QStringLiteral("Unpin") : QStringLiteral("Pin"));
    retheme();
    adjustSize();
}

void RowActionsBar::retheme() {
    const Theme::Palette& pal = Theme::palette();
    const QColor ink(pal.textMuted);
    const QColor danger(0xef, 0x44, 0x44);

    m_pin->setIcon(IconFactory::icon(QStringLiteral("pin"),
                                     m_pinned ? QColor(pal.accent) : ink, kIcon));
    m_copy->setIcon(IconFactory::icon(QStringLiteral("copy"), ink, kIcon));
    m_edit->setIcon(IconFactory::icon(QStringLiteral("edit"), ink, kIcon));
    m_delete->setIcon(IconFactory::icon(QStringLiteral("trash"), danger, kIcon));

    const bool dark = Theme::isDark();
    const QString hover = dark ? QStringLiteral("rgba(255,255,255,0.12)")
                               : QStringLiteral("rgba(0,0,0,0.08)");
    const QString press = dark ? QStringLiteral("rgba(255,255,255,0.20)")
                               : QStringLiteral("rgba(0,0,0,0.16)");
    setStyleSheet(QStringLiteral(
        "#actionsBar { background-color:%1; border:1px solid %2; border-radius:%3px; }"
        "QToolButton { background:transparent; border:none; border-radius:6px; }"
        "QToolButton:hover { background-color:%4; }"
        "QToolButton:pressed { background-color:%5; }")
        .arg(pal.surfaceAlt, pal.border)
        .arg(Theme::RadiusSm)
        .arg(hover, press));
}

void RowActionsBar::flashCopied() {
    const Theme::Palette& pal = Theme::palette();
    m_copy->setIcon(IconFactory::icon(QStringLiteral("check"), QColor(0x22, 0xc5, 0x5e), kIcon));
    QTimer::singleShot(1100, this, [this, muted = QColor(pal.textMuted)] {
        m_copy->setIcon(IconFactory::icon(QStringLiteral("copy"), muted, 16));
    });
}
