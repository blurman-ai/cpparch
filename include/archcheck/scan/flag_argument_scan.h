#pragma once

#include <string>
#include <vector>

#include "archcheck/rules/violation.h"
#include "archcheck/scan/duplication/token_normalizer.h"

namespace archcheck::scan
{

// Flag-argument heuristic (ARG.1, #093). Over discovered function signatures,
// flags interface drift toward boolean flags — a cheap "behaviour is leaking into
// the signature" early-warning, NOT a strict semantic rule. Token-level, advisory.
//
//   ARG.1.flag_argument_signature:
//     - >= 2 boolean parameters in one signature, or
//     - exactly 1 boolean parameter whose name reads like a flag
//       (enable/disable/use/force/skip/with/without/no/is/allow, on a word
//       boundary), so `bool node` does not trip but `bool enableX` does.
//
// `bool*` / `bool&` are NOT counted — a pointer/reference is an out-parameter, a
// different pattern. Declarations and definitions dedup by name+arity.
[[nodiscard]] rules::ViolationList detectFlagArguments(const std::vector<duplication::Token> &tokens,
                                                       const std::string &file);

} // namespace archcheck::scan
