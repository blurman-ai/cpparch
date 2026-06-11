#pragma once

#include <filesystem>

namespace archcheck::cli
{

// Preview/advisory single-path modes. All print to stdout; none gate CI
// except runGraph (exit 1 when the include graph has cycles).
int runScan(const std::filesystem::path &root);
int runGraph(const std::filesystem::path &root);
int runDuplication(const std::filesystem::path &root);
int runHistory(const std::filesystem::path &root);

} // namespace archcheck::cli
