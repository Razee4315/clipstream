#include "core/MathEval.h"

#include <QRegularExpression>

namespace {

// Recursive-descent parser over the trimmed expression string. `pos` walks the
// input; `ok` is cleared on any malformed token so callers can bail out.
struct Parser {
    QString s;
    int pos = 0;
    bool ok = true;

    void skipSpaces() {
        while (pos < s.size() && s[pos].isSpace())
            ++pos;
    }

    double parseExpression() {
        double value = parseTerm();
        skipSpaces();
        while (pos < s.size() && (s[pos] == QLatin1Char('+') || s[pos] == QLatin1Char('-'))) {
            const QChar op = s[pos++];
            const double rhs = parseTerm();
            value = (op == QLatin1Char('+')) ? value + rhs : value - rhs;
            skipSpaces();
        }
        return value;
    }

    double parseTerm() {
        double value = parseFactor();
        skipSpaces();
        while (pos < s.size() && (s[pos] == QLatin1Char('*') || s[pos] == QLatin1Char('/'))) {
            const QChar op = s[pos++];
            const double rhs = parseFactor();
            if (op == QLatin1Char('/')) {
                if (rhs == 0.0) { ok = false; return 0.0; }
                value /= rhs;
            } else {
                value *= rhs;
            }
            skipSpaces();
        }
        return value;
    }

    double parseFactor() {
        skipSpaces();
        if (pos >= s.size()) { ok = false; return 0.0; }

        if (s[pos] == QLatin1Char('+')) { ++pos; return parseFactor(); }
        if (s[pos] == QLatin1Char('-')) { ++pos; return -parseFactor(); }

        if (s[pos] == QLatin1Char('(')) {
            ++pos;
            const double value = parseExpression();
            skipSpaces();
            if (pos < s.size() && s[pos] == QLatin1Char(')'))
                ++pos;
            else
                ok = false;
            return value;
        }

        const int start = pos;
        while (pos < s.size() && (s[pos].isDigit() || s[pos] == QLatin1Char('.')))
            ++pos;
        if (pos == start) { ok = false; return 0.0; }
        bool numberOk = false;
        const double value = s.mid(start, pos - start).toDouble(&numberOk);
        if (!numberOk)
            ok = false;
        return value;
    }
};

} // namespace

namespace MathEval {

bool looksLikeExpression(const QString& text) {
    const QString t = text.trimmed();
    if (t.size() < 3 || t.size() > 100)
        return false;
    // Only digits, operators, parentheses, dots, spaces — and at least one op.
    static const QRegularExpression allowed(QStringLiteral(R"(^[0-9+\-*/(). ]+$)"));
    static const QRegularExpression hasOp(QStringLiteral(R"([+\-*/])"));
    static const QRegularExpression hasDigit(QStringLiteral(R"([0-9])"));
    return allowed.match(t).hasMatch() && hasOp.match(t).hasMatch()
        && hasDigit.match(t).hasMatch();
}

std::optional<double> evaluate(const QString& text) {
    if (!looksLikeExpression(text))
        return std::nullopt;
    Parser p{text.trimmed()};
    const double result = p.parseExpression();
    p.skipSpaces();
    if (!p.ok || p.pos != p.s.size())
        return std::nullopt;
    return result;
}

} // namespace MathEval
