#pragma once

#include <string>
#include <vector>

#include "archcheck/scan/duplication/token_normalizer.h"

namespace archcheck::scan
{

// Per-function cognitive complexity (#101). `score` is the only field findings
// are based on; the rest is report diagnostics.
struct FunctionComplexity
{
  std::string qualifiedName;
  int paramArity = 0;
  int startLine = 0;
  int endLine = 0;
  int meaningfulLoc = 0;
  int score = 0;           // Sonar Cognitive Complexity (Campbell 2018), token-level
  int structuralCount = 0; // decision points without nesting weight (diagnostic)
  int maxNesting = 0;      // deepest control nesting reached (diagnostic)
};

// Remove preprocessor-directive tokens. lex() drops only `#if 0` blocks; a bare
// `#if defined(X)` line would otherwise leak a phantom `if` keyword token into
// the scorer. Handles backslash line-continuations.
[[nodiscard]] std::vector<duplication::Token> stripDirectiveTokens(const std::vector<duplication::Token> &tokens);

// Lex the source, discover function bodies and score each one. Token-level port
// of the corpus-validated #102 prototype scorer (experiments/local_complexity_drift).
[[nodiscard]] std::vector<FunctionComplexity> computeFileComplexity(const std::string &source);

} // namespace archcheck::scan
