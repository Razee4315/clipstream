#pragma once

#include <QString>
#include <optional>

// A tiny, dependency-free arithmetic evaluator for the "paste the result"
// smart action. Supports + - * / , parentheses, unary minus and decimals.
// Returns nullopt when the text isn't a self-contained arithmetic expression
// (so we never offer the action on ordinary clips).
namespace MathEval {

bool looksLikeExpression(const QString& text);
std::optional<double> evaluate(const QString& text);

} // namespace MathEval
