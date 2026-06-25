#include "core/ContentClassifier.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace {

// Compiled once (static locals) — these run on every captured clip.
const QRegularExpression& urlRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(^(https?|ftp|file)://[^\s]+$)"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression& colorRe() {
    static const QRegularExpression re(QStringLiteral(
        R"(^(#(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})|rgba?\([^)]*\)|hsla?\([^)]*\))$)"));
    return re;
}

const QRegularExpression& winPathRe() {
    static const QRegularExpression re(QStringLiteral(R"(^[a-zA-Z]:[\\/].+)"));
    return re;
}

// Lightweight high-entropy estimate: distinct chars / length. Long strings with
// many distinct characters and no spaces look like tokens/keys.
double charDiversity(const QString& s) {
    if (s.isEmpty())
        return 0.0;
    QSet<QChar> seen;
    for (QChar c : s)
        seen.insert(c);
    return static_cast<double>(seen.size()) / s.size();
}

} // namespace

namespace ContentClassifier {

ContentType classify(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty())
        return ContentType::Text;

    if (!t.contains(QLatin1Char('\n'))) {
        if (urlRe().match(t).hasMatch())
            return ContentType::Url;
        if (colorRe().match(t).hasMatch())
            return ContentType::Color;
        if (winPathRe().match(t).hasMatch() || (t.startsWith(QLatin1Char('/')) && QFileInfo(t).isAbsolute()))
            return ContentType::FilePath;
    }

    static const QStringList codeMarkers = {
        QStringLiteral("fn "),    QStringLiteral("function "), QStringLiteral("def "),
        QStringLiteral("class "), QStringLiteral("const "),    QStringLiteral("let "),
        QStringLiteral("import "),QStringLiteral("#include"),   QStringLiteral("public "),
        QStringLiteral("void "),  QStringLiteral("=> "),        QStringLiteral("</"),
    };
    for (const QString& marker : codeMarkers) {
        if (t.contains(marker))
            return ContentType::Code;
    }

    return ContentType::Text;
}

bool looksSensitive(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty() || t.contains(QLatin1Char('\n')) || t.contains(QLatin1Char(' ')))
        return false; // secrets are usually single tokens

    // Credit-card-ish: 13–19 digits (optionally space/dash grouped).
    static const QRegularExpression cardRe(QStringLiteral(R"(^(?:\d[ -]?){13,19}$)"));
    if (cardRe.match(t).hasMatch())
        return true;

    // Long, dense, mixed-character tokens (API keys, JWTs, passwords).
    if (t.size() >= 20 && charDiversity(t) > 0.45) {
        const bool hasDigit = t.contains(QRegularExpression(QStringLiteral("[0-9]")));
        const bool hasAlpha = t.contains(QRegularExpression(QStringLiteral("[A-Za-z]")));
        if (hasDigit && hasAlpha)
            return true;
    }
    return false;
}

} // namespace ContentClassifier
