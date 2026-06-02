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

// Compute token-LCS length (longest common subsequence of tokens, order-aware).
std::size_t lcsLength(const std::vector<std::string> &a, const std::vector<std::string> &b);

// Compute LCS-based Dice coefficient (strict similarity: 2*LCS / (len_a + len_b)).
double lcsRatio(const Fragment &a, const Fragment &b);

} // namespace archcheck::scan::duplication
