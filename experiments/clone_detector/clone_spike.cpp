// clone_spike.cpp
// Spike: can clang::CloneDetector find a clone whose two members live in
// DIFFERENT translation units (= different "modules"), and does
// MatchingVariablePatternConstraint survive being run across two ASTContexts?
//
// We DON'T use the alpha.clone.CloneChecker (it is per-TU by construction).
// We drive the underlying clang::CloneDetector ourselves: build two separate
// ASTUnits, feed every function body from both into ONE detector, then
// findClones. The two ASTUnits are kept alive for the whole run because the
// StmtSequences point into their ASTContexts.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/Analysis/CloneDetection.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

// --- Two "modules". module_b::total_above_zero is a Type-2 clone of
//     module_a::sum_positive (same structure, consistently renamed vars).
//     module_b::product is a control: different structure, must NOT group.
static const char *kModuleA = R"cpp(
namespace module_a {
int sum_positive(const int *data, int n) {
  int acc = 0;
  for (int i = 0; i < n; ++i) {
    if (data[i] > 0) {
      acc += data[i];
    }
  }
  return acc;
}
}
)cpp";

static const char *kModuleB = R"cpp(
namespace module_b {
int total_above_zero(const int *arr, int count) {
  int result = 0;
  for (int k = 0; k < count; ++k) {
    if (arr[k] > 0) {
      result += arr[k];
    }
  }
  return result;
}
int product(const int *xs, int m) {
  int p = 1;
  for (int j = 0; j < m; ++j) {
    p *= xs[j];
  }
  return p;
}
}
)cpp";

// Recursively feed every function-with-body into the detector.
static void feedBodies(const DeclContext *DC, CloneDetector &Det)
{
  for (const Decl *D : DC->decls())
  {
    if (const auto *FD = dyn_cast<FunctionDecl>(D))
      if (FD->doesThisDeclarationHaveABody())
        Det.analyzeCodeBody(FD);
    if (const auto *Inner = dyn_cast<DeclContext>(D))
      feedBodies(Inner, Det);
  }
}

static std::string loc(const StmtSequence &Seq)
{
  const Decl *D = Seq.getContainingDecl();
  SourceManager &SM = D->getASTContext().getSourceManager();
  PresumedLoc PL = SM.getPresumedLoc(Seq.getBeginLoc());
  if (!PL.isValid())
    return "<invalid>";
  return std::string(PL.getFilename()) + ":" + std::to_string(PL.getLine());
}

static std::string fileOf(const StmtSequence &Seq)
{
  const Decl *D = Seq.getContainingDecl();
  SourceManager &SM = D->getASTContext().getSourceManager();
  PresumedLoc PL = SM.getPresumedLoc(Seq.getBeginLoc());
  return PL.isValid() ? std::string(PL.getFilename()) : "<invalid>";
}

static void report(const char *label, std::vector<CloneDetector::CloneGroup> &Groups)
{
  llvm::outs() << "\n=== " << label << " ===\n";
  llvm::outs() << "clone groups: " << Groups.size() << "\n";
  unsigned crossTU = 0;
  for (size_t g = 0; g < Groups.size(); ++g)
  {
    auto &Grp = Groups[g];
    std::set<std::string> files;
    for (auto &S : Grp)
      files.insert(fileOf(S));
    bool cross = files.size() > 1;
    if (cross)
      ++crossTU;
    llvm::outs() << "  group #" << g << " size=" << Grp.size() << (cross ? "  [CROSS-TU]" : "  [same-TU]") << "\n";
    for (auto &S : Grp)
      llvm::outs() << "      - " << loc(S) << "\n";
  }
  llvm::outs() << "cross-TU groups: " << crossTU << "\n";
}

int main()
{
  std::vector<std::string> args = {"-std=c++17"};

  std::vector<std::unique_ptr<ASTUnit>> units; // kept alive for whole run
  units.push_back(tooling::buildASTFromCodeWithArgs(kModuleA, args, "module_a.cpp"));
  units.push_back(tooling::buildASTFromCodeWithArgs(kModuleB, args, "module_b.cpp"));

  for (auto &U : units)
    if (!U)
    {
      llvm::errs() << "FATAL: failed to build an ASTUnit\n";
      return 1;
    }

  // One detector, fed from BOTH translation units.
  CloneDetector detector;
  for (auto &U : units)
    feedBodies(U->getASTContext().getTranslationUnitDecl(), detector);

  // Pass 1: the "normal clone" pipeline CloneChecker uses, WITHOUT the
  // variable-pattern constraint. Answers Q1 (cross-TU detection at all).
  // Swept over MinComplexity to see how the noise (sub-statement matches)
  // reacts vs the genuine function-level clone.
  for (unsigned cx : {5u, 15u, 30u})
  {
    std::vector<CloneDetector::CloneGroup> groups;
    detector.findClones(groups, RecursiveCloneTypeIIHashConstraint(), MinComplexityConstraint(cx),
                        MinGroupSizeConstraint(2), RecursiveCloneTypeIIVerifyConstraint(),
                        OnlyLargestCloneConstraint());
    report((std::string("Q1: hash+verify, MinComplexity=") + std::to_string(cx)).c_str(), groups);
  }

  // Pass 2: add MatchingVariablePatternConstraint at the end. Answers Q2
  // (does it run across two ASTContexts, and does the cross-TU group survive).
  {
    std::vector<CloneDetector::CloneGroup> groups;
    detector.findClones(groups, RecursiveCloneTypeIIHashConstraint(), MinComplexityConstraint(5),
                        MinGroupSizeConstraint(2), RecursiveCloneTypeIIVerifyConstraint(),
                        MatchingVariablePatternConstraint(), OnlyLargestCloneConstraint());
    report("Q2: + MatchingVariablePatternConstraint", groups);
  }

  llvm::outs() << "\n(done)\n";
  return 0;
}
