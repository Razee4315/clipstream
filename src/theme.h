#pragma once

// ClipStream design tokens — the single source of truth for every visual
// constant. Spacing/radius/type are compile-time; colours live in a runtime
// Palette so the app can switch between dark and light. See QT-AGENT-GUIDE.md §11.

#include <QString>
#include <QtGlobal>

namespace Theme {

// ---- Spacing scale (4 px grid — use ONLY these) -----------------------------
constexpr int S1 = 4, S2 = 8, S3 = 12, S4 = 16, S5 = 24, S6 = 32;

// ---- Radius -----------------------------------------------------------------
constexpr int RadiusSm = 8, RadiusMd = 12, RadiusLg = 16;

// ---- Type scale (a few deliberate tiers) ------------------------------------
constexpr int FsTitle = 16, FsBody = 13, FsMeta = 11, FsMicro = 10;

// ---- Overlay geometry -------------------------------------------------------
constexpr int OverlayWidth  = 380;
constexpr int OverlayHeight = 480;
constexpr int ShadowMargin  = 24; // transparent gutter so the drop shadow shows

// ---- List rows --------------------------------------------------------------
constexpr int RowHeight = 56;
constexpr int BadgeSize = 34;

// ---- Colour palette (switchable) --------------------------------------------
enum class Mode { System, Dark, Light };

struct Palette {
    QString bg;
    QString surface;
    QString surfaceAlt;
    QString border;
    QString accent;
    QString textPrimary;
    QString textMuted;
};

void setMode(Mode mode);
Mode mode();
bool isDark();                  // resolves System against the OS colour scheme
const Palette& palette();

inline QString fontFamily() {
#if defined(Q_OS_WIN)
    return QStringLiteral("Segoe UI");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("SF Pro Text");
#else
    return QStringLiteral("Inter");
#endif
}

} // namespace Theme
