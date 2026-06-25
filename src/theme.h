#pragma once

// ClipStream design tokens — the single source of truth for every visual
// constant. Nothing in the UI should hardcode a color, spacing, radius, or
// font size: pull it from here. See QT-AGENT-GUIDE.md §11.

#include <QString>
#include <QtGlobal>

namespace Theme {

// ---- Color (dark palette; light palette arrives in the polish phase) --------
constexpr auto Bg          = "#161618"; // backdrop behind the translucent card
constexpr auto Surface     = "#202023"; // primary card surface
constexpr auto SurfaceAlt  = "#27272b"; // raised rows / hover
constexpr auto Border      = "#323236";
constexpr auto Accent      = "#3b82f6";
constexpr auto TextPrimary = "#f2f2f4";
constexpr auto TextMuted   = "#9a9aa2";

// ---- Spacing scale (4 px grid — use ONLY these) -----------------------------
constexpr int S1 = 4, S2 = 8, S3 = 12, S4 = 16, S5 = 24, S6 = 32;

// ---- Radius -----------------------------------------------------------------
constexpr int RadiusSm = 8, RadiusMd = 12, RadiusLg = 16;

// ---- Type scale (a few deliberate tiers) ------------------------------------
constexpr int FsTitle = 16, FsBody = 13, FsMeta = 11, FsMicro = 10;

// ---- Overlay geometry -------------------------------------------------------
constexpr int OverlayWidth  = 360;
constexpr int OverlayHeight = 460;
constexpr int ShadowMargin  = 24; // transparent gutter so the drop shadow shows

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
