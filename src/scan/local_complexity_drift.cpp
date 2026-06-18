#include "archcheck/scan/local_complexity_drift.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "archcheck/scan/file_classification.h"
#include "archcheck/scan/local_complexity_metrics.h"
#include "archcheck/scan/source_snapshot.h"

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

std::string shortNameOf(const std::string &qualifiedName)
{
  const std::size_t pos = qualifiedName.rfind("::");
  return pos == std::string::npos ? qualifiedName : qualifiedName.substr(pos + 2);
}

// TEST_F(Suite, Name) bodies all share the macro symbol, so matching would
// pair unrelated tests (#102 review D6).
bool isTestMacroSymbol(const std::string &name)
{
  static constexpr std::array<const char *, 6> kTestSymbols = {"TEST",       "TEST_F",    "TEST_P",
                                                               "TYPED_TEST", "BENCHMARK", "MOCK_METHOD"};
  return std::find(kTestSymbols.begin(), kTestSymbols.end(), shortNameOf(name)) != kTestSymbols.end();
}

std::vector<FunctionComplexity> authoredFunctions(const std::string &source)
{
  auto fns = computeFileComplexity(source);
  fns.erase(std::remove_if(fns.begin(), fns.end(),
                           [](const FunctionComplexity &f) { return isTestMacroSymbol(f.qualifiedName); }),
            fns.end());
  return fns;
}

// Match on (qualifiedName, paramArity), preferring an exact parameter-list
// fingerprint — equal-arity overloads (#109 deletion-shift FP class) must not
// cross-match on line proximity. Remaining ambiguity (several candidates with
// the same signature) degrades to nearest start line with low confidence.
const FunctionComplexity *findOldMatch(const std::vector<FunctionComplexity> &olds, const FunctionComplexity &fn,
                                       bool &lowConfidence)
{
  const auto matches = [&fn](const FunctionComplexity &old, bool withParams)
  {
    return old.qualifiedName == fn.qualifiedName && old.paramArity == fn.paramArity &&
           (!withParams || old.paramFingerprint == fn.paramFingerprint);
  };
  const bool exact =
      std::any_of(olds.begin(), olds.end(), [&matches](const FunctionComplexity &o) { return matches(o, true); });
  const FunctionComplexity *best = nullptr;
  int count = 0;
  int bestDist = INT_MAX;
  for (const FunctionComplexity &old : olds)
  {
    if (!matches(old, exact))
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

// Same name, different arity, nearest line: a signature change, not a new
// function (#109 arity-change FP class — the body usually carries over).
const FunctionComplexity *findArityChangedMatch(const std::vector<FunctionComplexity> &olds,
                                                const FunctionComplexity &fn)
{
  const FunctionComplexity *best = nullptr;
  int bestDist = INT_MAX;
  for (const FunctionComplexity &old : olds)
  {
    const int dist = std::abs(old.startLine - fn.startLine);
    if (old.qualifiedName == fn.qualifiedName && dist < bestDist)
    {
      bestDist = dist;
      best = &old;
    }
  }
  return best;
}

// Same short name and arity, but the old function disappeared from the new
// side: a namespace/class move (#109 gromacs "into gmx namespace" gave 29
// phantom "new function" findings), not a new function. The disappeared check
// keeps genuinely new same-named overloads reportable; parameter spellings are
// not compared — a move often re-spells qualifiers (gmx::MDLogger -> MDLogger).
const FunctionComplexity *findRescopedMatch(const std::vector<FunctionComplexity> &olds,
                                            const std::vector<FunctionComplexity> &news, const FunctionComplexity &fn)
{
  const std::string shortName = shortNameOf(fn.qualifiedName);
  const auto stillPresent = [&news](const FunctionComplexity &old)
  {
    return std::any_of(news.begin(), news.end(), [&old](const FunctionComplexity &n)
                       { return n.qualifiedName == old.qualifiedName && n.paramArity == old.paramArity; });
  };
  const FunctionComplexity *best = nullptr;
  int bestDist = INT_MAX;
  for (const FunctionComplexity &old : olds)
  {
    if (old.paramArity != fn.paramArity || shortNameOf(old.qualifiedName) != shortName || stillPresent(old))
      continue;
    const int dist = std::abs(old.startLine - fn.startLine);
    if (dist < bestDist)
    {
      bestDist = dist;
      best = &old;
    }
  }
  return best;
}

// An unclosed brace (broken commit) swallows every following function into one
// garbage mega-span; #109 corpus showed phantom "new function" findings from
// such parents. Stray extra `}` is tolerated: forward discovery self-heals at
// the next definition. Skip the file pair only on unclosed opens.
bool hasUnclosedBrace(const std::string &source)
{
  int depth = 0;
  for (const duplication::Token &t : stripDirectiveTokens(duplication::lex(source)))
  {
    if (t.sym == "{")
      ++depth;
    else if (t.sym == "}")
      --depth;
  }
  return depth > 0;
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

// New functions: only the corpus-validated LCX.1 signal — flagging every
// new function with score >= K would be noise (#101 start decision).
void addNewFunctionFinding(ComplexityDriftResult &result, const FunctionComplexity &fn, const std::string &file)
{
  if (fn.score >= kCognitiveThreshold)
    result.violations.push_back({kRuleId, file, fn.startLine,
                                 "new function '" + fn.qualifiedName + "' has local complexity " +
                                     std::to_string(fn.score) + " (threshold " + std::to_string(kCognitiveThreshold) +
                                     ")"});
}

// Cross-file move pool: profiles of functions that vanished from (or collapsed
// to a delegate stub in) some changed file of the same diff. A from-zero /
// new-function finding elsewhere with a matching profile is a relocation, not
// growth (#109 foundationdb: an 861-line move gave 9 phantom "grew 0->N").
struct MovedFunction
{
  std::string shortName;
  int arity = 0;
  int score = 0;
};

void addDisappearedFunctions(std::vector<MovedFunction> &pool, const std::vector<FunctionComplexity> &olds,
                             const std::vector<FunctionComplexity> &news)
{
  for (const FunctionComplexity &old : olds)
  {
    if (old.score < kDeltaK)
      continue;
    // Survival is checked per exact signature (fingerprint): an equal-arity
    // sibling overload must not mask this overload's disappearance (#109
    // minsky counter-example — implement-shim-and-drop-old fired +17).
    // A survivor shrunk to a delegate stub (score < K) still counts as gone.
    const std::string shortName = shortNameOf(old.qualifiedName);
    int bestScore = -1;
    for (const FunctionComplexity &n : news)
      if (n.paramArity == old.paramArity && n.paramFingerprint == old.paramFingerprint &&
          shortNameOf(n.qualifiedName) == shortName)
        bestScore = std::max(bestScore, n.score);
    if (bestScore < kDeltaK)
      pool.push_back({shortName, old.paramArity, old.score});
  }
}

// Consumes a pool entry so one disappeared body can mute only one finding.
bool consumeMovedMatch(std::vector<MovedFunction> &pool, const FunctionComplexity &fn)
{
  const std::string shortName = shortNameOf(fn.qualifiedName);
  const int tolerance = std::max(2, fn.score / 10); // a moved body is often lightly edited
  for (auto it = pool.begin(); it != pool.end(); ++it)
  {
    if (it->arity == fn.paramArity && it->shortName == shortName && std::abs(it->score - fn.score) <= tolerance)
    {
      pool.erase(it);
      return true;
    }
  }
  return false;
}

ComplexityDriftResult compareFunctions(const std::vector<FunctionComplexity> &olds,
                                       const std::vector<FunctionComplexity> &news, const std::string &file,
                                       std::vector<MovedFunction> *movedPool)
{
  ComplexityDriftResult result;
  for (const FunctionComplexity &fn : news)
  {
    bool lowConfidence = false;
    const FunctionComplexity *old = findOldMatch(olds, fn, lowConfidence);
    if (old == nullptr && (old = findArityChangedMatch(olds, fn)) != nullptr)
      lowConfidence = true; // signature change: only the soft LCX.3 signal applies
    if (old == nullptr && (old = findRescopedMatch(olds, news, fn)) != nullptr)
      lowConfidence = true; // namespace/class move: same body under a new scope
    const int oldScore = old != nullptr ? old->score : 0;
    const int delta = fn.score - oldScore;
    (delta > 0 ? result.positiveDelta : result.negativeDelta) += delta;
    if (oldScore < kDeltaK && fn.score >= kDeltaK && movedPool != nullptr && consumeMovedMatch(*movedPool, fn))
      continue; // relocated body: the net delta already reflects both sides
    if (old == nullptr)
    {
      addNewFunctionFinding(result, fn, file);
      continue;
    }
    // GCC8-COMPAT: cppcheck 1.86 (Astra 1.7) can't parse init-statement in else-if.
    const auto reason = reasonFor(oldScore, fn.score, lowConfidence);
    if (reason.has_value())
      result.violations.push_back({kRuleId, file, fn.startLine, growthMessage(fn, oldScore, *reason)});
  }
  return result;
}

} // namespace

ComplexityDriftResult compareLocalComplexity(const std::string &oldSource, const std::string &newSource,
                                             const std::string &file)
{
  if (hasUnclosedBrace(oldSource) || hasUnclosedBrace(newSource))
    return {};
  return compareFunctions(authoredFunctions(oldSource), authoredFunctions(newSource), file, nullptr);
}

namespace
{

struct FilePair
{
  std::string path;
  std::vector<FunctionComplexity> olds, news;
};

// Pass 1: parse every analysable changed file and pool disappeared functions.
// #129: classification comes from the WHOLE current tree (newSnapshot), so the
// license-header layer has its dominant-banner ratio and complexity gets the same
// `authored` verdict as graph/clone — a vendored single-header is dropped here too
// (rmm false-complexity gone), while a self-licensed project stays analysed
// (its banner is dominant, #109 foundationdb).
std::vector<FilePair> collectFilePairs(const SourceSnapshot &oldSnapshot, const SourceSnapshot &newSnapshot,
                                       const std::vector<std::filesystem::path> &changedFiles,
                                       std::vector<MovedFunction> &movedPool)
{
  static const std::string kEmpty;
  std::vector<FilePair> pairs;
  for (const std::filesystem::path &p : changedFiles)
  {
    const std::string path = p.generic_string();
    const SnapshotFile *newFile = newSnapshot.findFile(path);
    const SnapshotFile *oldFile = oldSnapshot.findFile(path);
    if (newFile == nullptr && oldFile == nullptr)
      continue; // not in either tree
    // Authored verdict from the current tree (whole-tree banner ratio); for a
    // file deleted in the diff (old-only) fall back to the baseline classification.
    // A file present in old but not new must still be processed so the move pool
    // sees its disappeared functions (cross-file move detection).
    const bool authored = newFile != nullptr ? newFile->authored : oldFile->authored;
    if (!authored)
      continue; // vendored / test / generated / banner
    const std::string &oldText = oldFile != nullptr ? oldFile->content : kEmpty;
    const std::string &newText = newFile != nullptr ? newFile->content : kEmpty;
    if (hasUnclosedBrace(oldText) || hasUnclosedBrace(newText))
      continue;
    pairs.push_back({path, authoredFunctions(oldText), authoredFunctions(newText)});
    addDisappearedFunctions(movedPool, pairs.back().olds, pairs.back().news);
  }
  return pairs;
}

} // namespace

ComplexityDriftResult detectLocalComplexityDrift(const SourceSnapshot &oldSnapshot, const SourceSnapshot &newSnapshot,
                                                 const std::vector<std::filesystem::path> &changedFiles)
{
  std::vector<MovedFunction> movedPool;
  const std::vector<FilePair> pairs = collectFilePairs(oldSnapshot, newSnapshot, changedFiles, movedPool);
  // Pass 2: per-file comparison; the shared pool mutes relocation findings.
  ComplexityDriftResult total;
  for (const FilePair &pair : pairs)
  {
    ComplexityDriftResult fileResult = compareFunctions(pair.olds, pair.news, pair.path, &movedPool);
    total.positiveDelta += fileResult.positiveDelta;
    total.negativeDelta += fileResult.negativeDelta;
    std::move(fileResult.violations.begin(), fileResult.violations.end(), std::back_inserter(total.violations));
  }
  return total;
}

} // namespace archcheck::scan
