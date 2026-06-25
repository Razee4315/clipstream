#pragma once

#include "core/ClipEntry.h"

#include <QString>

// Pure functions that inspect clipboard text. No state, no Qt objects — easy to
// unit-test and reuse. Image classification is decided by the caller (the
// monitor knows when the clipboard held a bitmap).
namespace ContentClassifier {

// Categorise a piece of text: URL, colour, file path, code, or plain text.
ContentType classify(const QString& text);

// Heuristic "does this look like a secret?" check (passwords, card numbers,
// long high-entropy tokens). Used to mask entries and exclude them from search.
bool looksSensitive(const QString& text);

} // namespace ContentClassifier
