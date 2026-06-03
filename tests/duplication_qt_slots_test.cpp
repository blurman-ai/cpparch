#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "archcheck/scan/duplication/duplication_scanner.h"

using namespace archcheck::scan::duplication;

TEST_CASE("P0.6 joint floor should filter Qt dispatcher slots (33% line overlap)",
          "[duplication][p0][guards][qt-slots]")
{

  // РЕАЛЬНЫЙ СЛУЧАЙ ИЗ CORPUS:
  // 3 Qt slot-а в AlchemyViewer
  // Каждый имеет ОДИНАКОВЫЙ guard (2 строки) но РАЗНУЮ операцию (1 строка)
  // Line overlap = 2/6 = 33% < 50% threshold
  // P0.6 должен отфильтровать, но ловит как дубликат!

  // EXPANDED: More code to exceed minTokens=30
  std::string slot1 = R"(
void onSearchClick() {
    LLTextEditor* pEditor = getEditor();
    if (pEditor) {
        mCurrentPos = 0;
        QString searchTerm = getSearchString();
        int foundPos = pEditor->text().indexOf(searchTerm, mCurrentPos);
        if (foundPos != -1) {
            selectText(foundPos, searchTerm.length());
            mCurrentPos = foundPos + 1;
        }
    }
}
)";

  std::string slot2 = R"(
void onReplaceClick() {
    LLTextEditor* pEditor = getEditor();
    if (pEditor) {
        mCurrentPos = 0;
        QString replaceTerm = getReplaceString();
        QTextCursor cursor = pEditor->textCursor();
        if (cursor.hasSelection()) {
            cursor.insertText(replaceTerm);
            pEditor->setTextCursor(cursor);
        }
    }
}
)";

  std::string slot3 = R"(
void onReplaceAllClick() {
    LLTextEditor* pEditor = getEditor();
    if (pEditor) {
        mCurrentPos = 0;
        QString searchTerm = getSearchString();
        QString replaceTerm = getReplaceString();
        QString text = pEditor->toPlainText();
        text.replace(searchTerm, replaceTerm);
        pEditor->setPlainText(text);
    }
}
)";

  // Try 1: Different files (cross-file duplication)
  std::vector<std::pair<std::string, std::string>> filesXY = {
      {"slot1.cpp", slot1}, {"slot2.cpp", slot2}, {"slot3.cpp", slot3}};

  // Try 2: Same file (within-file duplication) - as in the corpus
  std::string slotsInSameFile = slot1 + "\n\n" + slot2 + "\n\n" + slot3;
  std::vector<std::pair<std::string, std::string>> filesSameFile = {{"slots.cpp", slotsInSameFile}};

  // Use same-file version since that's what's in the corpus
  auto &files = filesSameFile;

  SECTION("Without P0.6 guard (enableJointFloor=false)")
  {
    ScannerOptions opts;
    opts.simThreshold = 0.60;
    opts.enableJointFloor = false; // P0.6 disabled
    opts.enableP1Guards = false;

    ScanResult result = scanForDuplication(files, opts);

    std::cout << "\n=== WITHOUT P0.6 ===\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found: " << result.pairs.size() << "\n";

    if (!result.pairs.empty())
    {
      for (const auto &p : result.pairs)
      {
        std::cout << "  Pair[" << p.a << "," << p.b << "]: "
                  << "w=" << p.weighted << ", line=" << p.line << "\n";
      }
    }

    // Without P0.6, expect pairs to be kept (high weighted similarity)
    REQUIRE(result.fragments.size() > 0);
  }

  SECTION("With P0.6 guard (enableJointFloor=true)")
  {
    ScannerOptions opts;
    opts.simThreshold = 0.60;
    opts.enableJointFloor = true; // P0.6 enabled
    opts.jointWeightedThreshold = 0.75;
    opts.jointLineThreshold = 0.50;
    opts.enableP1Guards = false;

    ScanResult result = scanForDuplication(files, opts);

    std::cout << "\n=== WITH P0.6 (threshold: w>=0.75 AND line>=0.50) ===\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found: " << result.pairs.size() << "\n";

    if (!result.pairs.empty())
    {
      for (const auto &p : result.pairs)
      {
        std::cout << "  Pair[" << p.a << "," << p.b << "]: "
                  << "w=" << p.weighted << ", line=" << p.line << "\n";

        // P0.6 REQUIREMENT: BOTH metrics must pass
        if (!((p.weighted >= 0.75 && p.line >= 0.50)))
        {
          std::cout << "❌ P0.6 FAILED! Pair violates joint floor:\n";
          std::cout << "   weighted=" << p.weighted << " (need >=0.75)\n";
          std::cout << "   line=" << p.line << " (need >=0.50)\n";
          REQUIRE(false);
        }
      }
    }

    // ⚠️ KEY ASSERTION:
    // If line_overlap is really 33% (2 common lines / 6 total),
    // then P0.6 should REJECT this pair (33% < 50%).
    //
    // If this assertion FAILS, it means:
    // 1. Line overlap is calculated differently than expected
    // 2. P0.6 thresholds are different than defaults
    // 3. There's a bug in P0.6 implementation

    if (result.pairs.size() > 0)
    {
      std::cout << "❌ P0.6 FAILED TO FILTER!\n";
      std::cout << "These pairs should be rejected because line_overlap < 50%.\n";
      std::cout << "Instead, found " << result.pairs.size() << " pairs.\n";
    }
    REQUIRE(result.pairs.size() == 0);
  }

  SECTION("Check metrics with lower threshold")
  {
    std::cout << "\n=== LOWERING THRESHOLD TO 0.40 TO CAPTURE PAIRS ===\n";

    ScannerOptions opts;
    opts.simThreshold = 0.40; // Much lower threshold
    opts.enableJointFloor = false;
    opts.enableP1Guards = false;

    // NOTE: P0.1 (same-function filter) is applied BEFORE similarity check
    // Since slots are in same file, P0.1 might be filtering them!

    std::cout << "⚠️  Note: Same file = P0.1 same-function filter may reject them\n";

    ScanResult result = scanForDuplication(files, opts);

    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found: " << result.pairs.size() << "\n";

    if (!result.pairs.empty())
    {
      for (const auto &p : result.pairs)
      {
        std::cout << "\nPair[" << p.a << "," << p.b << "] metrics:\n";
        std::cout << "  weighted: " << p.weighted << " ← weighted Jaccard\n";
        std::cout << "  plain: " << p.plain << " ← plain Jaccard\n";
        std::cout << "  line: " << p.line << " ← LINE OVERLAP (this is what P0.6 checks!)\n";
        std::cout << "  lcs: " << p.lcs << " ← token-LCS\n\n";

        std::cout << "P0.6 would filter this pair if:\n";
        std::cout << "  weighted < 0.75: " << (p.weighted < 0.75 ? "YES ✓" : "NO") << "\n";
        std::cout << "  line < 0.50: " << (p.line < 0.50 ? "YES ✓ (this is why!)" : "NO") << "\n";
      }
    }
    else
    {
      std::cout << "⚠️  Still no pairs even at 0.40 threshold!\n";
      std::cout << "This means similarity is < 0.40 for all pairs.\n";
    }
  }
}
