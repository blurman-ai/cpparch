#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "archcheck/scan/duplication/fragmenter.h"

namespace archcheck::scan::duplication
{

// Compute weighted Jaccard similarity (bag-of-tokens with IDF weights).
double weightedJaccard(const Fragment &a, const Fragment &b, const std::unordered_map<std::string, double> &idf);

// Compute plain Jaccard similarity (unweighted bag-of-tokens).
double plainJaccard(const Fragment &a, const Fragment &b);

// Compute line-based Jaccard similarity (normalized lines).
double lineOverlap(const Fragment &a, const Fragment &b);

// Count distinct substantive verbatim lines shared by both fragments (the absolute
// run lineOverlap throws away when it divides by the union). A renamed copy shares
// few identical raw lines; a real edited copy shares many even when the ratio is low.
std::size_t sharedLineCount(const Fragment &a, const Fragment &b);

// Count tokens shared by both fragments that are rare (document frequency <= cap).
// A real copy shares project-specific anchors (names, constants); a framework idiom
// (Qt dialog scaffold, RAII ceremony) shares only common tokens — its rare count is ~0.
std::size_t sharedRareCount(const Fragment &a, const Fragment &b,
                            const std::unordered_map<std::string, int> &df, std::size_t cap);

// Compute token-LCS length (longest common subsequence of tokens, order-aware).
std::size_t lcsLength(const std::vector<std::string> &a, const std::vector<std::string> &b);

// Compute LCS-based Dice coefficient (strict similarity: 2*LCS / (len_a + len_b)).
double lcsRatio(const Fragment &a, const Fragment &b);

} // namespace archcheck::scan::duplication
