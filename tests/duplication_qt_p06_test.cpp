#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "archcheck/scan/duplication/duplication_scanner.h"

using namespace archcheck::scan::duplication;

TEST_CASE("P0.6 verification: does it actually filter low line-overlap pairs?", "[duplication][p0.6][guards]")
{

  // Create TWO IDENTICAL blocks (so they definitely pass similarity threshold)
  // Then check if P0.6 filters based on line_overlap

  std::string block1 = R"(
void processData(const std::vector<int>& data) {
    for (int i = 0; i < data.size(); i++) {
        int value = data[i];
        if (value > threshold) {
            results.push_back(value);
            count++;
        }
    }
    return count;
}
)";

  // EXACT COPY - should have similarity 1.0
  std::string block2 = block1;

  // Create 5+ DIFFERENT dummy blocks so IDF weights are non-zero
  // Each must be > 30 tokens to create fragments
  std::vector<std::string> dummyBlocks = {
      R"(
void dummy1() {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            int x = i * j;
            int y = x + 1;
            int z = y + 2;
            process(x, y, z);
        }
    }
}
)",
      R"(
void dummy2() {
    while (true) {
        int a = getData();
        if (a < 0) break;
        int b = a * 2;
        int c = b + 3;
        int d = c * 4;
        process(a, b, c, d);
        update();
    }
}
)",
      R"(
void dummy3() {
    for (const auto& item : items) {
        int p = item.value;
        int q = p * 3;
        int r = q + 5;
        if (r > threshold) {
            process(p, q, r);
            log(r);
        }
    }
}
)",
      R"(
void dummy4() {
    for (int i = 0; i < 100; i++) {
        int m = i + 1;
        int n = m * 2;
        if (n % 2 == 0) {
            process(n);
            count++;
        }
        if (count > 50) break;
    }
}
)",
      R"(
void dummy5() {
    std::vector<int> vec;
    for (int u = 0; u < 20; u++) {
        int v = u * u;
        vec.push_back(v);
        if (v > 100) {
            process(u, v);
            cleanup();
        }
    }
}
)",
  };

  std::vector<std::pair<std::string, std::string>> filesSameFile = {{"data.cpp", block1 + "\n\n" + block2}};

  std::vector<std::pair<std::string, std::string>> filesDiffFiles = {
      {"data1.cpp", block1},          {"data2.cpp", block2},          {"dummy1.cpp", dummyBlocks[0]},
      {"dummy2.cpp", dummyBlocks[1]}, {"dummy3.cpp", dummyBlocks[2]}, {"dummy4.cpp", dummyBlocks[3]},
      {"dummy5.cpp", dummyBlocks[4]},
  };

  SECTION("P0.6 disabled - identical blocks in DIFFERENT FILES")
  {
    ScannerOptions opts;
    opts.simThreshold = 0.60;
    opts.enableJointFloor = false; // P0.6 disabled
    opts.enableP1Guards = false;   // Disable P1 to isolate

    ScanResult result = scanForDuplication(filesDiffFiles, opts);

    std::cout << "\n=== P0.6 DISABLED (identical blocks in different files) ===\n";
    std::cout << "Files: " << filesDiffFiles.size() << "\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";

    if (!result.fragments.empty())
    {
      std::cout << "Fragment details:\n";
      for (size_t i = 0; i < result.fragments.size(); i++)
      {
        const auto &f = result.fragments[i];
        std::cout << "  [" << i << "] tokens=" << f.tokenCount << " (line " << f.startLine << "-" << f.endLine << ")\n";
      }
    }

    std::cout << "Raw candidates: " << result.candidateCount << "\n";
    std::cout << "Scored candidates (passed similarity gate): " << result.scoredCandidateCount << "\n";
    std::cout << "Final pairs (after all guards): " << result.pairs.size() << "\n";

    if (!result.pairs.empty())
    {
      for (const auto &p : result.pairs)
      {
        std::cout << "Pair[" << p.a << "," << p.b << "]: " << "w=" << p.weighted << ", line=" << p.line << "\n";
      }
    }
    else
    {
      // Debug: show metrics for the scored candidates
      if (!result.fragments.empty() && result.fragments.size() >= 2)
      {
        std::cout << "\n⚠️  NO PAIRS FOUND even for identical blocks!\n";
        std::cout << "Fragment metrics:\n";
        const auto &f0 = result.fragments[0];
        const auto &f1 = result.fragments[1];
        std::cout << "  Fragment 0: tokens=" << f0.tokenCount << ", bag_size=" << f0.bag.size() << "\n";
        std::cout << "  Fragment 1: tokens=" << f1.tokenCount << ", bag_size=" << f1.bag.size() << "\n";
        std::cout << "\nBag contents (first 5 keys):\n";
        int count = 0;
        std::cout << "  F0: ";
        for (const auto &[k, v] : f0.bag)
        {
          if (count >= 5)
          {
            std::cout << "...";
            break;
          }
          std::cout << k << "(" << v << ") ";
          count++;
        }
        std::cout << "\n  F1: ";
        count = 0;
        for (const auto &[k, v] : f1.bag)
        {
          if (count >= 5)
          {
            std::cout << "...";
            break;
          }
          std::cout << k << "(" << v << ") ";
          count++;
        }
        std::cout << "\n  (If bags differ → tokenization/normalization issue)\n";
      }
    }

    REQUIRE(result.pairs.size() > 0); // Should find identical blocks
  }

  SECTION("P0.6 enabled - check if it filters correctly")
  {
    ScannerOptions opts;
    opts.simThreshold = 0.60;
    opts.enableJointFloor = true; // P0.6 enabled
    opts.jointWeightedThreshold = 0.75;
    opts.jointLineThreshold = 0.50;

    ScanResult result = scanForDuplication(filesDiffFiles, opts);

    std::cout << "\n=== P0.6 ENABLED (w>=0.75 AND line>=0.50) ===\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found: " << result.pairs.size() << "\n";

    if (!result.pairs.empty())
    {
      for (const auto &p : result.pairs)
      {
        std::cout << "Pair[" << p.a << "," << p.b << "]: " << "w=" << p.weighted << ", line=" << p.line << "\n";

        // For identical blocks, both metrics should be 1.0 or very high
        std::cout << "  ✓ P0.6 passed (both metrics high for identical blocks)\n";
      }
    }
    else
    {
      std::cout << "❌ P0.6 filtered all pairs (even identical blocks!)\n";
    }

    // Identical blocks MUST pass P0.6
    REQUIRE(result.pairs.size() > 0);
  }

  SECTION("NOW THE REAL TEST - qt slots with guard only in common")
  {
    // Create TWO blocks similar to Qt slots:
    // - Guard is IDENTICAL
    // - Operation is COMPLETELY DIFFERENT
    // This simulates: same guard + different operations

    std::string qtSlot1 = R"(
void onSearchClick() {
    LLTextEditor* pEditor = getEditor();
    if (pEditor) {
        QString text = pEditor->toPlainText();
        int pos = text.indexOf(searchTerm);
        selectNextOccurrence();
    }
}
)";

    std::string qtSlot2 = R"(
void onReplaceClick() {
    LLTextEditor* pEditor = getEditor();
    if (pEditor) {
        QString text = pEditor->toPlainText();
        text.replace(searchTerm, replaceTerm);
        applyReplacementToEditor();
    }
}
)";

    std::vector<std::pair<std::string, std::string>> qtFiles = {{"slots.cpp", qtSlot1 + "\n\n" + qtSlot2}};

    ScannerOptions opts;
    opts.simThreshold = 0.50; // Lower threshold to capture them
    opts.enableJointFloor = false;

    ScanResult result = scanForDuplication(qtFiles, opts);

    std::cout << "\n=== QT SLOTS (guard + different ops) ===\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found: " << result.pairs.size() << "\n";

    if (!result.pairs.empty())
    {
      for (const auto &p : result.pairs)
      {
        std::cout << "Pair[" << p.a << "," << p.b << "]:\n";
        std::cout << "  weighted=" << p.weighted << "\n";
        std::cout << "  line=" << p.line << " ← THIS DETERMINES IF P0.6 FILTERS IT\n";
      }
    }
    else
    {
      std::cout << "⚠️ No pairs even at 0.50 threshold - similarity too low\n";
    }
  }
}
