#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archcheck/scan/authored_scope.h"
#include "archcheck/scan/file_classification.h"

using archcheck::scan::AuthoredScope;
namespace sc = archcheck::scan;

// Characterization test for the unified gate (#129). It replicates the THREE
// pre-#129 open-coded formulas and asserts AuthoredScope (a) agrees with them on
// the inputs where they already agreed, and (b) diverges ONLY on the two
// documented bug-fixes: SWIG generated files (none of the old set-filters caught
// them) and the dominant-banner clone over-exclusion.

namespace
{
constexpr std::string_view kApacheBanner = "// Licensed under the Apache License, Version 2.0\n";

// clone's collectNonVendoredSources: UNGUARDED banner via isVendoredFile.
bool oldClone(std::string_view path, std::string_view content)
{
  const std::string_view base = sc::baseName(path);
  return sc::pathHasVendoredDir(path) || sc::pathHasTestDir(path) || sc::isTestBasename(base) || content.empty() ||
         sc::isVendoredFile(base, content);
}
// complexity's collectFilePairs: path+basename only, NO banner, NO generated.
bool oldComplexity(std::string_view path)
{
  const std::string_view base = sc::baseName(path);
  return sc::pathHasVendoredDir(path) || sc::pathHasTestDir(path) || sc::isTestBasename(base) ||
         sc::isVendoredBasename(base);
}
} // namespace

TEST_CASE("AuthoredScope: vendored / test / curated-basename — all formulas agree", "[scan][authored_scope]")
{
  const auto scope = AuthoredScope::changedFilesMode();
  for (const auto &path : {std::string_view{"third_party/foo/bar.cpp"}, std::string_view{"tests/foo_test.cpp"},
                           std::string_view{"src/catch.hpp"}})
  {
    INFO(path);
    REQUIRE(scope.excluded(path, "x")); // unified gate excludes
    REQUIRE(oldClone(path, "x"));       // and so did clone
    REQUIRE(oldComplexity(path));       // and complexity
  }
}

TEST_CASE("AuthoredScope: plain authored source is never excluded", "[scan][authored_scope]")
{
  const auto scope = AuthoredScope::changedFilesMode();
  REQUIRE_FALSE(scope.excluded("src/graph/graph_builder.cpp", "int main(){}"));
  REQUIRE_FALSE(oldClone("src/graph/graph_builder.cpp", "int main(){}"));
  REQUIRE_FALSE(oldComplexity("src/graph/graph_builder.cpp"));
}

TEST_CASE("AuthoredScope: SWIG generated file — bug-fix, old set-filters missed it", "[scan][authored_scope]")
{
  const std::string_view swig = "source/cpp/objectmodel_wrap.cpp";
  // The fix: unified gate excludes generated SWIG output...
  REQUIRE(AuthoredScope::changedFilesMode().excluded(swig, "code"));
  // ...where the old clone + complexity SET filters did NOT (generated was only
  // suppressed later in the token phase, and never in complexity at all).
  REQUIRE_FALSE(oldComplexity(swig));
}

TEST_CASE("AuthoredScope: banner file in a NON-dominant set — vendored", "[scan][authored_scope]")
{
  // One banner-carrying file among mostly plain authored files: it is a vendored
  // drop-in, not the project's own license.
  std::vector<std::pair<std::string, std::string>> files = {
      {"src/a.cpp", "int a;"},
      {"src/b.cpp", "int b;"},
      {"src/c.cpp", "int c;"},
      {"src/vendored.hpp", std::string(kApacheBanner) + "int v;"}};
  const auto scope = AuthoredScope::fromFiles(files);
  REQUIRE(scope.excluded("src/vendored.hpp", std::string(kApacheBanner) + "int v;"));
  REQUIRE(oldClone("src/vendored.hpp", std::string(kApacheBanner) + "int v;")); // clone agreed
}

TEST_CASE("AuthoredScope: dominant banner (self-licensed project) — bug-fix vs clone over-exclusion",
          "[scan][authored_scope]")
{
  // foundationdb case: EVERY file carries the project's own Apache banner.
  const std::string body = std::string(kApacheBanner) + "int x;";
  std::vector<std::pair<std::string, std::string>> files = {
      {"src/a.cpp", body}, {"src/b.cpp", body}, {"src/c.cpp", body}};
  const auto scope = AuthoredScope::fromFiles(files);
  // The fix: the >50%-dominant guard recognizes it as the project's own license,
  // so the file is NOT excluded (matches graph; complexity also kept it).
  REQUIRE_FALSE(scope.excluded("src/a.cpp", body));
  REQUIRE_FALSE(oldComplexity("src/a.cpp"));
  // ...whereas clone's UNGUARDED banner would have wrongly over-excluded it:
  REQUIRE(oldClone("src/a.cpp", body));
}
