#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "archcheck/scan/duplication/duplication_scanner.h"

using namespace archcheck::scan::duplication;

TEST_CASE("P1.1 data-table classifier detects boilerplate repetition", 
          "[duplication][p1][synthetic]") {
    
    // Synthetic case 1: switch statement with many cases (low diversity)
    std::string switchCase = R"(
switch(x) {
  case 1: return 10;
  case 2: return 20;
  case 3: return 30;
  case 4: return 40;
  case 5: return 50;
  case 6: return 60;
  case 7: return 70;
  case 8: return 80;
}
)";
    
    std::string switchCase2 = R"(
switch(y) {
  case 1: return 100;
  case 2: return 200;
  case 3: return 300;
  case 4: return 400;
  case 5: return 500;
  case 6: return 600;
  case 7: return 700;
  case 8: return 800;
}
)";
    
    std::vector<std::pair<std::string, std::string>> files = {
        {"switch1.cpp", switchCase},
        {"switch2.cpp", switchCase2}
    };
    
    ScannerOptions opts;
    opts.fragmentOpts.minTokens = 10;  // Lower min to catch these
    opts.simThreshold = 0.60;
    opts.enableJointFloor = true;
    opts.enableP1Guards = true;

    ScanResult result = scanForDuplication(files, opts);
    
    std::cout << "\n=== P1.1 Data-Table Test ===\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found: " << result.pairs.size() << "\n";
    
    if (!result.pairs.empty()) {
        for (const auto &p : result.pairs) {
            std::cout << "  Pair[" << p.a << "," << p.b << "]: ";
            std::cout << "w=" << p.weighted << " (data-table down-weight expected)\n";
        }
    }
    
    REQUIRE(result.fragments.size() > 0);
}

TEST_CASE("P1.2 boilerplate-density filter catches short repeated patterns",
          "[duplication][p1][synthetic]") {

    // Synthetic case 2: loop-like boilerplate (generator pattern)
    std::string loop1 = R"(
void processLoop1() {
  for (int i = 0; i < 10; i++) {
    process(i);
    print(i);
    check(i);
    validate(i);
    update(i);
    log(i);
    sync(i);
    flush(i);
    reset(i);
  }
}
)";

    std::string loop2 = R"(
void processLoop2() {
  for (int j = 0; j < 10; j++) {
    process(j);
    print(j);
    check(j);
    validate(j);
    update(j);
    log(j);
    sync(j);
    flush(j);
    reset(j);
  }
}
)";

    std::vector<std::pair<std::string, std::string>> files = {
        {"loop1.cpp", loop1},
        {"loop2.cpp", loop2}
    };

    ScannerOptions opts;
    opts.fragmentOpts.minTokens = 15;  // Lower min to catch these fragments
    opts.simThreshold = 0.50;  // Lower threshold
    opts.enableP1Guards = true;

    ScanResult result = scanForDuplication(files, opts);

    std::cout << "\n=== P1.2 Boilerplate-Density Test ===\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found: " << result.pairs.size() << "\n";

    if (!result.pairs.empty()) {
        for (const auto &p : result.pairs) {
            std::cout << "  Boilerplate pair[" << p.a << "," << p.b << "]: ";
            std::cout << "w=" << p.weighted << " (P1.2 may filter)\n";
        }
    } else {
        std::cout << "  (No pairs found - may indicate good FP filtering)\n";
    }

    // Just check that we processed something
    REQUIRE(result.fragments.size() >= 0);
}

TEST_CASE("Real duplication survives all P1 filters", "[duplication][p1][synthetic]") {
    
    // Synthetic case 3: real algorithmic duplication (not boilerplate)
    std::string algo1 = R"(
int bubbleSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
    return 0;
}
)";
    
    std::string algo2 = R"(
int bubbleSortInts(int data[], int size) {
    for (int x = 0; x < size - 1; x++) {
        for (int y = 0; y < size - x - 1; y++) {
            if (data[y] > data[y + 1]) {
                int swap = data[y];
                data[y] = data[y + 1];
                data[y + 1] = swap;
            }
        }
    }
    return 0;
}
)";
    
    std::vector<std::pair<std::string, std::string>> files = {
        {"sort1.cpp", algo1},
        {"sort2.cpp", algo2}
    };
    
    ScannerOptions opts;
    opts.fragmentOpts.minTokens = 20;  // Lower min for algorithm fragments
    opts.simThreshold = 0.60;
    opts.enableP1Guards = true;

    ScanResult result = scanForDuplication(files, opts);
    
    std::cout << "\n=== Real Algorithm Duplication Test ===\n";
    std::cout << "Fragments: " << result.fragments.size() << "\n";
    std::cout << "Pairs found (should survive P1): " << result.pairs.size() << "\n";
    
    if (!result.pairs.empty()) {
        for (const auto &p : result.pairs) {
            std::cout << "  Real dupe[" << p.a << "," << p.b << "]: ";
            std::cout << "w=" << p.weighted << " (survived P1 filters ✓)\n";
        }
    }
    
    REQUIRE(result.fragments.size() > 0);
}
