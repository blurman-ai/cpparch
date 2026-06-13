// #116: verify §9.1 of duplication_architecture.md — thin delegates are not reported.
// Thin delegate = 1-2 line method body forwarding to a backend; bodies stay below
// minTokens=30 and never fragment, so they cannot form clone pairs regardless of how
// many implementations share the shape. The TP control and boundary scenarios carry
// substantial DISTINCT background functions so the IDF denominator is non-degenerate
// (2 identical fragments alone give idf=log(2/2)=0 → weighted=0; the task brief warns
// about exactly this).
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/scan/duplication/duplication_scanner.h"

using namespace archcheck::scan::duplication;

namespace
{

// Distinct ≥30-token background functions, unique vocabulary per file, so rare tokens
// exist and IDF weighting is meaningful for the fragments under test.
const std::string kBackgroundA = R"cpp(
int alphaRoutine(int seed, int scale)
{
  int acc = seed;
  for (int k = 0; k < scale; ++k) { acc = acc * 7 + k - seed; acc = acc ^ (k << 2); }
  return acc + scale;
}
int betaRoutine(int base, int span)
{
  int total = base;
  for (int n = 0; n < span; ++n) { total = total + n * base - span; total = total | (n >> 1); }
  return total - base;
}
)cpp";

const std::string kBackgroundB = R"cpp(
int gammaRoutine(int origin, int width)
{
  int sum = origin;
  for (int p = 0; p < width; ++p) { sum = sum * 5 + p - origin; sum = sum & (p << 3); }
  return sum + width;
}
int deltaRoutine(int start, int reach)
{
  int agg = start;
  for (int q = 0; q < reach; ++q) { agg = agg + q * start - reach; agg = agg ^ (q >> 2); }
  return agg - start;
}
)cpp";

// Three implementations delegating to *different* backends (BackendX/Y/Z).
// Methods arity 0..4 — bodies well below minTokens=30.
const std::string kImplA_DiffBackend = R"cpp(
class ImplA {
public:
  void m0() { bx_.op0(); }
  int m1(int a) { return bx_.op1(a); }
  void m2(int a, int b) { bx_.op2(a, b); }
  int m3(int a, int b, int c) { return bx_.op3(a, b, c); }
  void m4(int a, int b, int c, int d) { bx_.op4(a, b, c, d); }
private:
  BackendX bx_;
};
)cpp";

const std::string kImplB_DiffBackend = R"cpp(
class ImplB {
public:
  void m0() { by_.op0(); }
  int m1(int a) { return by_.op1(a); }
  void m2(int a, int b) { by_.op2(a, b); }
  int m3(int a, int b, int c) { return by_.op3(a, b, c); }
  void m4(int a, int b, int c, int d) { by_.op4(a, b, c, d); }
private:
  BackendY by_;
};
)cpp";

const std::string kImplC_DiffBackend = R"cpp(
class ImplC {
public:
  void m0() { bz_.op0(); }
  int m1(int a) { return bz_.op1(a); }
  void m2(int a, int b) { bz_.op2(a, b); }
  int m3(int a, int b, int c) { return bz_.op3(a, b, c); }
  void m4(int a, int b, int c, int d) { bz_.op4(a, b, c, d); }
private:
  BackendZ bz_;
};
)cpp";

// Same shape, all delegating to the SAME backend — only class name differs.
const std::string kImplA_SameBackend = R"cpp(
class ImplA {
public:
  void m0() { bx_.op0(); }
  int m1(int a) { return bx_.op1(a); }
  void m2(int a, int b) { bx_.op2(a, b); }
  int m3(int a, int b, int c) { return bx_.op3(a, b, c); }
  void m4(int a, int b, int c, int d) { bx_.op4(a, b, c, d); }
private:
  BackendX bx_;
};
)cpp";

const std::string kImplB_SameBackend = R"cpp(
class ImplB {
public:
  void m0() { bx_.op0(); }
  int m1(int a) { return bx_.op1(a); }
  void m2(int a, int b) { bx_.op2(a, b); }
  int m3(int a, int b, int c) { return bx_.op3(a, b, c); }
  void m4(int a, int b, int c, int d) { bx_.op4(a, b, c, d); }
private:
  BackendX bx_;
};
)cpp";

// Substantial method body (>30 tokens), identical across two impls = the TP.
const std::string kSubstantialCompute = R"cpp(
class %CLASS% {
public:
  int compute(int a, int b, int c) {
    int tmp1 = a * b + c;
    int tmp2 = tmp1 - a + b;
    int tmp3 = tmp2 * c - b;
    int tmp4 = tmp3 + tmp1 - tmp2;
    return backend_.process(tmp1, tmp2, tmp3, tmp4);
  }
private:
  Backend backend_;
};
)cpp";

// 10-param delegate (arity 9). The fragmenter counts BODY tokens (the delegating
// call `backend_.op9(a..j);` ≈ 25 tokens), NOT the signature — so even a 10-param
// delegate stays below minTokens=30 and never fragments.
const std::string kTenParamDelegate = R"cpp(
class %CLASS% {
public:
  void m9(int a, int b, int c, int d, int e,
          int f, int g, int h, int i, int j) {
    backend_.op9(a, b, c, d, e, f, g, h, i, j);
  }
private:
  Backend backend_;
};
)cpp";

std::string withClass(std::string tmpl, const std::string &name)
{
  const auto pos = tmpl.find("%CLASS%");
  tmpl.replace(pos, 7, name);
  return tmpl;
}

} // namespace

// Scenario 1: thin delegates to *different* backends → 0 pairs.
// Bodies are 1-liners (arity ≤ 4); all below minTokens=30, never fragmented.
TEST_CASE("thin delegates to different backends produce no pairs", "[duplication][thin-delegates]")
{
  const auto result = scanForDuplication({
      {"implA.h", kBackgroundA + kImplA_DiffBackend},
      {"implB.h", kBackgroundB + kImplB_DiffBackend},
      {"implC.h", kImplC_DiffBackend},
  });
  REQUIRE(result.pairs.empty());
}

// Scenario 2: thin delegates to the *same* backend (only class name differs) → 0 pairs.
// Bodies are token-identical but below minTokens=30 — not fragmented.
TEST_CASE("thin delegates to same backend (only class name differs) produce no pairs", "[duplication][thin-delegates]")
{
  const auto result = scanForDuplication({
      {"implA.h", kBackgroundA + kImplA_SameBackend},
      {"implB.h", kBackgroundB + kImplB_SameBackend},
  });
  REQUIRE(result.pairs.empty());
}

// Scenario 3: control TP — substantial identical bodies (>30 tokens) are reported.
// Background functions supply IDF context so the shared compute tokens carry weight.
TEST_CASE("substantial identical compute bodies are reported as clone (TP control)", "[duplication][thin-delegates]")
{
  const auto result = scanForDuplication({
      {"implA.cpp", kBackgroundA + withClass(kSubstantialCompute, "ImplA")},
      {"implB.cpp", kBackgroundB + withClass(kSubstantialCompute, "ImplB")},
  });
  REQUIRE(!result.pairs.empty());
}

// Scenario 4: 10-param thin delegate (arity 9) — DOCUMENTS actual behavior for §9.1.
// The fragmenter sizes the BODY, not the signature. A 10-arg delegating call
// (`backend_.op9(a..j);`) is ≈25 tokens — still below minTokens=30 — so the method
// never fragments and produces 0 pairs. Conclusion for §9.1: inflating a delegate's
// parameter list does NOT push it across the fragmentation floor; only a delegate whose
// BODY exceeds ~30 tokens (multi-statement, not a single forwarding call) can surface.
TEST_CASE("10-param thin delegate body stays below the fragmentation floor — no pairs", "[duplication][thin-delegates]")
{
  const auto result = scanForDuplication({
      {"tenA.h", kBackgroundA + withClass(kTenParamDelegate, "TenA")},
      {"tenB.h", kBackgroundB + withClass(kTenParamDelegate, "TenB")},
  });
  REQUIRE(result.pairs.empty());
}
