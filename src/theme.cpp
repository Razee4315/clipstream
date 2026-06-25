#include "theme.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace Theme {
namespace {

// Palette literals (bg, surface, surfaceAlt, border, accent, textPrimary, textMuted).
const Palette kDark{
    QStringLiteral("#161618"), QStringLiteral("#202023"), QStringLiteral("#2b2b30"),
    QStringLiteral("#34343a"), QStringLiteral("#3b82f6"), QStringLiteral("#f2f2f4"),
    QStringLiteral("#9a9aa2")};

const Palette kLight{
    QStringLiteral("#f4f4f6"), QStringLiteral("#ffffff"), QStringLiteral("#ececf0"),
    QStringLiteral("#d8d8de"), QStringLiteral("#2563eb"), QStringLiteral("#1b1b1f"),
    QStringLiteral("#6c6c75")};

const Palette kMidnight{
    QStringLiteral("#0f1117"), QStringLiteral("#171a23"), QStringLiteral("#20242f"),
    QStringLiteral("#2a2f3d"), QStringLiteral("#6366f1"), QStringLiteral("#e6e8ee"),
    QStringLiteral("#8b90a0")};

const Palette kNord{
    QStringLiteral("#2e3440"), QStringLiteral("#3b4252"), QStringLiteral("#434c5e"),
    QStringLiteral("#4c566a"), QStringLiteral("#88c0d0"), QStringLiteral("#eceff4"),
    QStringLiteral("#a6adbb")};

const Palette kForest{
    QStringLiteral("#10140e"), QStringLiteral("#1a2016"), QStringLiteral("#242c1e"),
    QStringLiteral("#333d2a"), QStringLiteral("#4ade80"), QStringLiteral("#e8f0e4"),
    QStringLiteral("#9aa890")};

const Palette kRose{
    QStringLiteral("#1c1518"), QStringLiteral("#261c20"), QStringLiteral("#31242a"),
    QStringLiteral("#3f2f36"), QStringLiteral("#fb7185"), QStringLiteral("#f4e9ec"),
    QStringLiteral("#b59aa3")};

const Palette kSolarized{
    QStringLiteral("#fdf6e3"), QStringLiteral("#fffbf0"), QStringLiteral("#eee8d5"),
    QStringLiteral("#ddd6c1"), QStringLiteral("#268bd2"), QStringLiteral("#073642"),
    QStringLiteral("#657b83")};

const QVector<ThemeDef>& allThemes() {
    static const QVector<ThemeDef> defs = {
        {QStringLiteral("system"),    QStringLiteral("System"),          kDark,      true},
        {QStringLiteral("dark"),      QStringLiteral("Dark"),            kDark,      true},
        {QStringLiteral("light"),     QStringLiteral("Light"),           kLight,     false},
        {QStringLiteral("midnight"),  QStringLiteral("Midnight"),        kMidnight,  true},
        {QStringLiteral("nord"),      QStringLiteral("Nord"),            kNord,      true},
        {QStringLiteral("forest"),    QStringLiteral("Forest"),          kForest,    true},
        {QStringLiteral("rose"),      QStringLiteral("Rosé"),            kRose,      true},
        {QStringLiteral("solarized"), QStringLiteral("Solarized Light"), kSolarized, false},
    };
    return defs;
}

QString g_id = QStringLiteral("system");

bool systemIsDark() {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

const ThemeDef& current() {
    for (const ThemeDef& t : allThemes())
        if (t.id == g_id)
            return t;
    return allThemes().first();
}

} // namespace

const QVector<ThemeDef>& themes() { return allThemes(); }

void setThemeId(const QString& id) { g_id = id; }
QString themeId() { return g_id; }

bool isDark() {
    if (g_id == QLatin1String("system"))
        return systemIsDark();
    return current().dark;
}

const Palette& palette() {
    if (g_id == QLatin1String("system"))
        return systemIsDark() ? kDark : kLight;
    return current().palette;
}

} // namespace Theme
