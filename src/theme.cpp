#include "theme.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace Theme {
namespace {

Mode g_mode = Mode::System;

const Palette kDark{
    QStringLiteral("#161618"), QStringLiteral("#202023"), QStringLiteral("#27272b"),
    QStringLiteral("#323236"), QStringLiteral("#3b82f6"), QStringLiteral("#f2f2f4"),
    QStringLiteral("#9a9aa2"),
};

const Palette kLight{
    QStringLiteral("#f4f4f6"), QStringLiteral("#ffffff"), QStringLiteral("#ececf0"),
    QStringLiteral("#d8d8de"), QStringLiteral("#2563eb"), QStringLiteral("#1b1b1f"),
    QStringLiteral("#6c6c75"),
};

bool systemIsDark() {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

} // namespace

void setMode(Mode mode) { g_mode = mode; }
Mode mode() { return g_mode; }

bool isDark() {
    switch (g_mode) {
        case Mode::Dark:  return true;
        case Mode::Light: return false;
        case Mode::System:
        default:          return systemIsDark();
    }
}

const Palette& palette() { return isDark() ? kDark : kLight; }

} // namespace Theme
