#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "archcheck/scan/include_directive.h"
#include "archcheck/scan/include_resolver.h"
#include "archcheck/scan/project_files.h"
#include "archcheck/scan/resolved_include.h"

using archcheck::scan::buildProjectIndex;
using archcheck::scan::IncludeDirective;
using archcheck::scan::IncludeKind;
using archcheck::scan::NodeId;
using archcheck::scan::ProjectFile;
using archcheck::scan::ProjectIndex;
using archcheck::scan::Resolution;
using archcheck::scan::ResolvedInclude;
using archcheck::scan::resolveInclude;
using archcheck::scan::resolveIncludes;

namespace
{

std::vector<ProjectFile> files_of(std::initializer_list<std::string_view> paths)
{
  std::vector<ProjectFile> out;
  out.reserve(paths.size());
  for (std::string_view p : paths)
  {
    out.push_back(ProjectFile{std::string{p}});
  }
  return out;
}

IncludeDirective quote(std::string token, int line = 1)
{
  return IncludeDirective{IncludeKind::Quote, std::move(token), line};
}

IncludeDirective angle(std::string token, int line = 1)
{
  return IncludeDirective{IncludeKind::Angle, std::move(token), line};
}

} // namespace

TEST_CASE("resolve_include quote: directory-relative hit -> Project", "[scan][resolver][quote]")
{
  const auto files = files_of({"src/foo/a.cpp", "src/foo/a.h", "include/a.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(quote("a.h"), "src/foo/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{1});
  REQUIRE(r.sourceFile == "src/foo/a.cpp");
}

TEST_CASE("resolve_include quote: exact repo-relative match -> Project", "[scan][resolver][quote]")
{
  const auto files = files_of({"src/a.cpp", "include/lib/b.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(quote("include/lib/b.h"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{1});
}

TEST_CASE("resolve_include quote: unique suffix match -> Project", "[scan][resolver][quote]")
{
  const auto files = files_of({"src/a.cpp", "include/lib/widget.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(quote("lib/widget.h"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{1});
}

TEST_CASE("resolve_include quote: multiple suffix candidates -> Ambiguous", "[scan][resolver][quote]")
{
  const auto files = files_of({"src/a.cpp", "moduleA/util.h", "moduleB/util.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(quote("util.h"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Ambiguous);
  REQUIRE(r.candidates.size() == 2);
}

TEST_CASE("resolve_include quote: no match -> Unresolved", "[scan][resolver][quote]")
{
  const auto files = files_of({"src/a.cpp"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(quote("nowhere.h"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Unresolved);
}

TEST_CASE("resolve_include angle: exact repo-relative match -> Project", "[scan][resolver][angle]")
{
  const auto files = files_of({"src/a.cpp", "include/proj/api.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(angle("include/proj/api.h"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{1});
}

TEST_CASE("resolve_include angle: unique suffix match -> Project", "[scan][resolver][angle]")
{
  const auto files = files_of({"src/a.cpp", "include/proj/api.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(angle("proj/api.h"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{1});
}

TEST_CASE("resolve_include angle: multiple suffix candidates -> Ambiguous", "[scan][resolver][angle]")
{
  const auto files = files_of({"src/a.cpp", "moduleA/util.h", "moduleB/util.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(angle("util.h"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Ambiguous);
  REQUIRE(r.candidates.size() == 2);
}

TEST_CASE("resolve_include angle: no match -> External", "[scan][resolver][angle]")
{
  const auto files = files_of({"src/a.cpp"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(angle("vector"), "src/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::External);
}

TEST_CASE("resolve_include quote: dir-relative wins over suffix collision", "[scan][resolver][quote]")
{
  const auto files = files_of({"moduleA/a.cpp", "moduleA/util.h", "moduleB/util.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(quote("util.h"), "moduleA/a.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{1});
}

TEST_CASE("resolve_include quote: source at repo root, dir-relative degenerate", "[scan][resolver][quote]")
{
  const auto files = files_of({"main.cpp", "a.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(quote("a.h"), "main.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{1});
}

TEST_CASE("resolve_include angle: single_include candidate is skipped", "[scan][resolver][mirror]")
{
  const auto files = files_of({"src/nlohmann/json.hpp", "single_include/nlohmann/json.hpp"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(angle("nlohmann/json.hpp"), "src/main.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Project);
  REQUIRE(r.target == NodeId{0});
}

TEST_CASE("resolve_include angle: both mirror candidates -> still Ambiguous", "[scan][resolver][mirror]")
{
  const auto files = files_of({"single_include/a/foo.h", "amalgamated/a/foo.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const ResolvedInclude r = resolveInclude(angle("a/foo.h"), "src/main.cpp", files, index);
  REQUIRE(r.resolution == Resolution::Ambiguous);
  REQUIRE(r.candidates.size() == 2);
}

TEST_CASE("resolve_includes batch preserves order and per-directive verdicts", "[scan][resolver][batch]")
{
  const auto files = files_of({"src/a.cpp", "src/a.h", "include/lib/b.h"});
  const ProjectIndex index = buildProjectIndex(files);
  const std::vector<IncludeDirective> ds = {
      quote("a.h", 1),
      angle("lib/b.h", 2),
      angle("vector", 3),
      quote("missing.h", 4),
  };
  const auto results = resolveIncludes(ds, "src/a.cpp", files, index);
  REQUIRE(results.size() == 4);
  REQUIRE(results[0].resolution == Resolution::Project);
  REQUIRE(results[0].target == NodeId{1});
  REQUIRE(results[1].resolution == Resolution::Project);
  REQUIRE(results[1].target == NodeId{2});
  REQUIRE(results[2].resolution == Resolution::External);
  REQUIRE(results[3].resolution == Resolution::Unresolved);
}
