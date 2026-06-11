#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "archcheck/scan/duplication/token_normalizer.h"

namespace archcheck::scan
{

// A function (or function-like) definition located in a token stream.
struct FunctionSpan
{
  std::string qualifiedName; // "ns::Cls::method", "~Name", "operator+", ...
  int paramArity = 0;        // top-level parameter count; 0 for () and (void)
  int startLine = 0;         // line of the name token
  int endLine = 0;           // line of the closing brace
  std::size_t bodyBegin = 0; // token index of the opening '{'
  std::size_t bodyEnd = 0;   // token index of the matching '}'
};

// Conservative function-definition discovery over a lex() token stream (#101).
// Recognises `name ( params ) [qualifiers] [-> type] [: init-list] {`; control
// headers (if/for/while/switch/catch) are excluded, lambdas belong to their
// enclosing function. Preprocessor-directive tokens must be filtered out by the
// caller (see stripDirectiveTokens in local_complexity_metrics.h). Misses are
// acceptable by design — both diff sides miss identically.
[[nodiscard]] std::vector<FunctionSpan> discoverFunctions(const std::vector<duplication::Token> &tokens);

} // namespace archcheck::scan
