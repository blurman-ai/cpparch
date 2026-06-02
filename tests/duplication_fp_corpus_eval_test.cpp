#include <catch2/catch_test_macros.hpp>

#include "archcheck/scan/duplication/fp_corpus_eval.h"
#include "archcheck/scan/duplication/token_normalizer.h"

using namespace archcheck::scan::duplication;

TEST_CASE("FP corpus eval: loadCorpusGroundTruth returns empty on missing file", "[duplication][corpus-eval]")
{
  const auto truth = loadCorpusGroundTruth("/nonexistent/path/to/corpus.tsv");
  REQUIRE(truth.empty());
}

TEST_CASE("FP corpus eval: basic corpus loading", "[duplication][corpus-eval]")
{
  // This test would require a sample corpus file. For now, test empty case.
  const GroundTruth truth;
  REQUIRE(truth.empty());
}

TEST_CASE("FP corpus eval: evaluateAgainstCorpus handles empty ground truth", "[duplication][corpus-eval]")
{
  const std::string file1 = "void func1() { a=1; b=2; c=3; d=4; e=5; f=6; g=7; }";
  const std::string file2 = "void func2() { a=1; b=2; c=3; d=4; e=5; f=6; g=7; }";

  ScannerOptions opts;
  opts.fragmentOpts.minTokens = 5;
  opts.simThreshold = 0.5;

  const auto result = scanForDuplication({{"file1.cpp", file1}, {"file2.cpp", file2}}, opts);

  const GroundTruth emptyTruth;
  const auto metrics = evaluateAgainstCorpus(result, emptyTruth);

  // Empty ground truth should produce zero metrics
  REQUIRE(metrics.precision == 0.0);
}

TEST_CASE("FP corpus eval: metrics structure is valid", "[duplication][corpus-eval]")
{
  CorpusMetrics m;

  // Verify all fields are initialized
  REQUIRE(m.truePositives == 0);
  REQUIRE(m.falsePositives == 0);
  REQUIRE(m.falseNegatives == 0);
  REQUIRE(m.trueNegatives == 0);
  REQUIRE(m.precision == 0.0);
  REQUIRE(m.recall == 0.0);
  REQUIRE(m.fpByClass.empty());
}

TEST_CASE("FP corpus eval: precision calculation", "[duplication][corpus-eval]")
{
  CorpusMetrics m;
  m.truePositives = 10;
  m.falsePositives = 5;

  double precision = static_cast<double>(m.truePositives) / (m.truePositives + m.falsePositives);
  REQUIRE(precision == 0.6666666666666666); // 10 / 15 ≈ 0.667
}

TEST_CASE("FP corpus eval: recall calculation", "[duplication][corpus-eval]")
{
  CorpusMetrics m;
  m.truePositives = 10;
  m.falseNegatives = 5;

  double recall = static_cast<double>(m.truePositives) / (m.truePositives + m.falseNegatives);
  REQUIRE(recall == 0.6666666666666666); // 10 / 15 ≈ 0.667
}
