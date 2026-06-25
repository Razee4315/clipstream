#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Renders the bundled Lucide-style SVG glyphs into themed icons. SVGs use
// `currentColor`, which we substitute with the requested colour before
// rasterising — so one glyph serves every palette. Results are cached.
namespace IconFactory {

QPixmap pixmap(const QString& name, const QColor& color, int px = 18);
QIcon   icon(const QString& name, const QColor& color, int px = 18);

} // namespace IconFactory
