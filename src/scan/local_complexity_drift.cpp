#include "archcheck/scan/local_complexity_drift.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "archcheck/scan/file_classification.h"
#include "archcheck/scan/local_complexity_metrics.h"

namespace archcheck::scan
{
namespace
{

constexpr const char *kRuleId = "DRIFT.LOCAL_COMPLEXITY";
// 25 = Sonar C-family / clang-tidy default threshold.
constexpr int kCognitiveThreshold = 25;
// LCX.3 soft-warning floor; calibration start per design doc §5, K confirmation
// on a second corpus slice is an open #102 item.
constexpr int kDeltaK = 5;
// LCX.2 floor: the Δ1–2 tail on already-huge functions was 72/210 corpus
// findings and not actionable (#102 verdict).
constexpr int kAboveFloor = 3;

// TEST_F(Suite, Name) bodies all share the macro symbol, so matching would
// pair unrelated tests (#102 review D6).
bool isTestMacroSymbol(const std::string &name)
{
  const std::size_t pos = name.rfind("::");
  const std::string shortName = pos == std::string::npos ? name : name.substr(pos + 2);
  static constexpr std::array<const char *, 6> kTestSymbols = {"TEST",       "TEST_F",    "TEST_P",
                                                               "TYPED_TEST", "BENCHMARK", "MOCK_METHOD"};
  return std::find(kTestSymbols.begin(), kTestSymbols.end(), shortName) != kTestSymbols.end();
}

std::vector<FunctionComplexity> authoredFunctions(const std::string &source)
{
  auto fns = computeFileComplexity(source);
  fns.erase(std::remove_if(fns.begin(), fns.end(),
                           [](const FunctionComplexity &f) { return isTestMacroSymbol(f.qualifiedName); }),
            fns.end());
  return fns;
}

// Match on (qualifiedName, paramArity); several candidates (overload set with
// equal arity) degrade to nearest start line with low confidence.
const FunctionComplexity *findOldMatch(const std::vector<FunctionComplexity> &olds, const FunctionComplexity &fn,
                                       bool &lowConfidence)
{
  const FunctionComplexity *best = nullptr;
  int count = 0;
  int bestDist = INT_MAX;
  for (const FunctionComplexity &old : olds)
  {
    if (old.qualifiedName != fn.qualifiedName || old.paramArity != fn.paramArity)
      continue;
    ++count;
    const int dist = std::abs(old.startLine - fn.startLine);
    if (dist < bestDist)
    {
      bestDist = dist;
      best = &old;
    }
  }
  lowConfidence = count > 1;
  return best;
}

// LCX hierarchy, strongest first. Low-confidence matches qualify only for the
// soft LCX.3 signal. nullopt => no finding; the value is the message suffix.
std::optional<std::string> reasonFor(int oldScore, int newScore, bool lowConfidence)
{
  const int delta = newScore - oldScore;
  if (!lowConfidence && oldScore < kCognitiveThreshold && newScore >= kCognitiveThreshold)
    return ", crossed " + std::to_string(kCognitiveThreshold);
  if (!lowConfidence && oldScore >= kCognitiveThreshold && delta >= kAboveFloor)
    return ", already above " + std::to_string(kCognitiveThreshold);
  if (delta >= kDeltaK)
    return std::string{};
  return std::nullopt;
}

std::string growthMessage(const FunctionComplexity &fn, int oldScore, const std::string &reasonSuffix)
{
  return "function '" + fn.qualifiedName + "' grew local complexity from " + std::to_string(oldScore) + " to " +
         std::to_string(fn.score) + " (+" + std::to_string(fn.score - oldScore) + reasonSuffix + ")";
}

} // namespace

ComplexityDriftResult compareLocalComplexity(const std::string &oldSource, const std::string &newSource,
                                             const std::string &file)
{
  const auto olds = authoredFunctions(oldSource);
  const auto news = authoredFunctions(newSource);
  ComplexityDriftResult result;
  for (const FunctionComplexity &fn : news)
  {
    bool lowConfidence = false;
    const FunctionComplexity *old = findOldMatch(olds, fn, lowConfidence);
    const int oldScore = old != nullptr ? old->score : 0;
    const int delta = fn.score - oldScore;
    (delta > 0 ? result.positiveDelta : result.negativeDelta) += delta;
    if (old == nullptr)
    {
      // New functions: only the corpus-validated LCX.1 signal — flagging every
      // new function with score >= K would be noise (#101 start decision).
      if (fn.score >= kCognitiveThreshold)
        result.violations.push_back({kRuleId, file, fn.startLine,
                                     "new function '" + fn.qualifiedName + "' has local complexity " +
                                         std::to_string(fn.score) + " (threshold " +
                                         std::to_string(kCognitiveThreshold) + ")"});
      continue;
    }
    const auto reason = reasonFor(oldScore, fn.score, lowConfidence);
    if (reason.has_value())
      result.violations.push_back({kRuleId, file, fn.startLine, growthMessage(fn, oldScore, *reason)});
  }
  return result;
}

ComplexityDriftResult detectLocalComplexityDrift(FileSource &oldSource, FileSource &newSource,
                                                 const std::vector<std::filesystem::path> &changedFiles)
{
  ComplexityDriftResult total;
  for (const std::filesystem::path &p : changedFiles)
  {
    const std::string path = p.generic_string();
    if (pathHasVendoredDir(path) || pathHasTestDir(path) || isTestBasename(baseName(path)))
      continue;
    const std::string oldText = oldSource.read(path);
    const std::string newText = newSource.read(path);
    if (isVendoredFile(baseName(path), oldText) || isVendoredFile(baseName(path), newText))
      continue;
    ComplexityDriftResult fileResult = compareLocalComplexity(oldText, newText, path);
    total.positiveDelta += fileResult.positiveDelta;
    total.negativeDelta += fileResult.negativeDelta;
    std::move(fileResult.violations.begin(), fileResult.violations.end(), std::back_inserter(total.violations));
  }
  return total;
}

} // namespace archcheck::scan
