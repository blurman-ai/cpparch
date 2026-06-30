#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "archcheck/scan/duplication/clone_index.h"
#include "archcheck/scan/duplication/fragmenter.h"
#include "archcheck/scan/duplication/similarity.h"

namespace archcheck::scan::duplication
{

struct Pair
{
  std::size_t a = 0;     // first fragment index
  std::size_t b = 0;     // second fragment index
  double weighted = 0.0; // weighted Jaccard similarity
  double plain = 0.0;    // plain Jaccard similarity
  double line = 0.0;     // line-based overlap (union Jaccard)
  std::size_t sharedLines = 0; // distinct substantive verbatim lines shared (absolute run)
  std::size_t sharedRare = 0;  // shared rare (project-specific) tokens — anchors a real copy
  double lcs = 0.0;      // token-LCS Dice ratio
  std::string type;      // clone type: EXACT/RENAMED/LITERAL/MIXED/STRUCTURAL (see clone_classifier)
};

struct ScannerOptions
{
  FragmentOptions fragmentOpts;
  IndexOptions indexOpts;
  std::string metric = "weighted";      // "weighted" or "plain" — which metric gates similarity
  bool precise = false;                 // if true, use token-LCS as gate; else use metric
  double simThreshold = 0.60;           // similarity gate threshold
  bool enableJointFloor = true;         // P0.6: require BOTH token AND line metrics to pass
  double jointWeightedThreshold = 0.75; // P0.6: minimum weighted similarity when joint floor enabled
  double jointLineThreshold = 0.50;     // P0.6: minimum line overlap when joint floor enabled
  // Copy-paste is a substantive block, not a dense one/two-liner. A 30-token fragment can sit
  // on 1-3 physical lines (`for(int i=0;i<4;i++) buf[i]=(v>>(8*i))&0xFF;` = 31 tokens, one
  // line), and the classic ratio path has no absolute substance floor, so it passes at
  // lineOverlap=1.0. Gate on STATEMENT count (top-level `;`), style-robust where lines are
  // not (7 calls on 2 lines = 7 statements). Set to 2: a 1-statement fragment is a dense
  // one-liner that copy-paste detection must not flag. NOT higher — a real 3-statement
  // function (loop + two guarded returns) is indistinguishable by any size axis from a
  // 3-statement near-idiom (the Part D wall). Both fragments must clear this.
  std::size_t jointMinClassicStatements = 2;
  // P0.6b: a pair below the line-ratio still passes if it shares this many distinct
  // verbatim lines AND >= jointMinSharedRare rare anchors AND is not a low-diversity
  // table — recovers Type-3 edited copies (insert/delete deflates the ratio but not
  // the absolute run) without admitting framework idioms (no rare anchor). 0 disables.
  std::size_t jointMinSharedLines = 6;
  std::size_t jointMinSharedRare = 0;        // run-path needs this many shared anchors (0 disables)
  std::size_t jointAnchorDfCap = 12;         // a token shared by both AND in <= this many fragments is a
                                             // project anchor; framework idioms (Qt/STL) exceed it -> no anchor
  double jointRunWeightedThreshold = 0.60;   // run-path weighted floor: edits dilute the bag below
                                             // jointWeightedThreshold, so a strong line-run uses a lower bound
  bool enableP1Guards = true;           // P1: enable classifier filters (data-table, boilerplate, header-impl, IDF)
  bool enablePathGuards = true;         // P0.9: suppress generated-file pairs (.pb.cc, moc_, flex/bison)
  bool enableWholeFileGuard = true;     // P0.2: count whole-file clones separately, drop their pairs
  bool enableDataTableDrop = true;      // P1.1: make data-table guard a real DROP (not just down-weight)
};

struct ScanResult
{
  std::vector<Fragment> fragments;
  std::vector<Pair> pairs;
  CloneIndex index;
  std::size_t fileCount = 0;
  std::size_t candidateCount = 0;       // Raw candidates before scoring
  std::size_t scoredCandidateCount = 0; // After similarity gate
  std::size_t wholeFileClones = 0;      // P0.2: file-pairs suppressed as whole-file clones (counted, not reported)
  std::size_t totalLoc = 0;
};

// Scan C++ source files and detect duplication: lex → fragment → index → candidate pairs → score.
// Returns all fragments, candidate pairs passing the similarity gate, and index metadata.
ScanResult scanForDuplication(const std::vector<std::pair<std::string, std::string>> &files,
                              const ScannerOptions &opts = ScannerOptions{});

} // namespace archcheck::scan::duplication
