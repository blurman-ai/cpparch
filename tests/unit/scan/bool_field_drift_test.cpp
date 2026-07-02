#include <catch2/catch_test_macros.hpp>
#include <map>
#include <string>

#include "archcheck/scan/bool_field_drift.h"
#include "archcheck/scan/source_snapshot.h"

namespace
{

using archcheck::scan::compareBoolFields;
using archcheck::scan::detectBoolFieldDrift;
using archcheck::scan::SourceSnapshot;

struct MapFileSource final : archcheck::scan::FileSource
{
  std::map<std::string, std::string> files;

  std::vector<archcheck::scan::ProjectFile> list() override
  {
    std::vector<archcheck::scan::ProjectFile> out;
    for (const auto &[path, content] : files)
      out.push_back({path});
    return out;
  }
  std::string read(const std::string &path) override
  {
    const auto it = files.find(path);
    return it == files.end() ? std::string{} : it->second;
  }
};

} // namespace

TEST_CASE("bool_drift: accretion into a pre-existing struct fires", "[scan][bool_drift]")
{
  const std::string oldSrc = "struct S\n{\n  bool a;\n};\n";
  const std::string newSrc = "struct S\n{\n  bool a;\n  bool b;\n  bool c;\n};\n";
  const auto res = compareBoolFields(oldSrc, newSrc, "s.h");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].ruleId == "DRIFT.BOOL_FIELD_ACCRETION");
  REQUIRE(res.violations[0].message.find("accreted 2") != std::string::npos);
  REQUIRE(res.violations[0].message.find("b, c") != std::string::npos);
}

TEST_CASE("bool_drift: new struct (greenfield) is not drift", "[scan][bool_drift]")
{
  const std::string newSrc = "struct New\n{\n  bool a;\n  bool b;\n};\n";
  REQUIRE(compareBoolFields("", newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: struct that had 0 bools and gains some fires", "[scan][bool_drift]")
{
  const std::string oldSrc = "struct S\n{\n  int x;\n};\n";
  const std::string newSrc = "struct S\n{\n  int x;\n  bool ready;\n  bool done;\n};\n";
  const auto res = compareBoolFields(oldSrc, newSrc, "s.h");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("accreted 2") != std::string::npos);
}

TEST_CASE("bool_drift: rename keeps the count, no finding", "[scan][bool_drift]")
{
  const std::string oldSrc = "struct S\n{\n  bool oldName;\n};\n";
  const std::string newSrc = "struct S\n{\n  bool newName;\n};\n";
  REQUIRE(compareBoolFields(oldSrc, newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: reduction is silent", "[scan][bool_drift]")
{
  const std::string oldSrc = "struct S\n{\n  bool a;\n  bool b;\n};\n";
  const std::string newSrc = "struct S\n{\n  bool a;\n};\n";
  REQUIRE(compareBoolFields(oldSrc, newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: method/param bools are not fields", "[scan][bool_drift]")
{
  const std::string oldSrc = "struct S\n{\n  bool a;\n};\n";
  const std::string newSrc = "struct S\n{\n  bool a;\n  void set(bool x);\n  bool isOk() const;\n};\n";
  REQUIRE(compareBoolFields(oldSrc, newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: brace inside a string literal does not fake accretion (#136)", "[scan][bool_drift]")
{
  // The bool field exists in BOTH versions; a naive brace counter would miss it in the
  // version whose method body contains `"x={ "` and fake a +1. With literal stripping the
  // count is stable -> no finding.
  const std::string oldSrc = "class S\n{\n  void f()\n  {\n    ss << \"x={ \";\n  }\n  bool flag_;\n};\n";
  const std::string newSrc = "class S\n{\n  void f()\n  {\n    log(flag_);\n  }\n  bool flag_;\n};\n";
  REQUIRE(compareBoolFields(oldSrc, newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: const drop / brace-init rewrite are not accretion (#164)", "[scan][bool_drift]")
{
  // btop/fmt: `const bool x;` -> `bool x;` and mvfst: `bool x{false};` -> `bool x : 1;`
  const std::string oldSrc = "struct S\n{\n  const bool a;\n  bool b{false};\n};\n";
  const std::string newSrc = "struct S\n{\n  bool a;\n  bool b : 1;\n};\n";
  REQUIRE(compareBoolFields(oldSrc, newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: parens inside a trailing comment do not hide a field (#164)", "[scan][bool_drift]")
{
  // NanaBox reproducer: the baseline field carries a `// TODO(Phase4): ...` comment;
  // dropping the comment must not read as a new field.
  const std::string oldSrc = "struct S\n{\n  bool a = false; // TODO(Phase4): remove\n};\n";
  const std::string newSrc = "struct S\n{\n  bool a = false;\n};\n";
  REQUIRE(compareBoolFields(oldSrc, newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: message counts the listed gained names, gate stays net (#164)", "[scan][bool_drift]")
{
  // rename a->b plus one genuinely new field: net +1 fires, message lists both gained
  // names with a matching count.
  const std::string oldSrc = "struct S\n{\n  bool a;\n};\n";
  const std::string newSrc = "struct S\n{\n  bool b;\n  bool c;\n};\n";
  const auto res = compareBoolFields(oldSrc, newSrc, "s.h");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("accreted 2 bool field(s): b, c") != std::string::npos);
}

TEST_CASE("bool_drift: out-of-line nested class keeps its qualified name (#164)", "[scan][bool_drift]")
{
  const std::string oldSrc = "class Type::Function : public Base\n{\n  bool a;\n};\n";
  const std::string newSrc = "class Type::Function : public Base\n{\n  bool a;\n  bool b;\n};\n";
  const auto res = compareBoolFields(oldSrc, newSrc, "t.h");
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].message.find("struct 'Type::Function'") != std::string::npos);
}

TEST_CASE("bool_drift: static bools are class constants, not state", "[scan][bool_drift]")
{
  const std::string oldSrc = "struct S\n{\n  bool a;\n};\n";
  const std::string newSrc = "struct S\n{\n  bool a;\n  static bool s;\n  static const bool k = true;\n};\n";
  REQUIRE(compareBoolFields(oldSrc, newSrc, "s.h").violations.empty());
}

TEST_CASE("bool_drift: detect honours the authored filter over changed files", "[scan][bool_drift]")
{
  MapFileSource oldSrc;
  oldSrc.files["src/a.h"] = "struct S\n{\n  bool a;\n};\n";
  MapFileSource newSrc;
  newSrc.files["src/a.h"] = "struct S\n{\n  bool a;\n  bool b;\n};\n";
  const SourceSnapshot oldSnap = SourceSnapshot::read(oldSrc);
  const SourceSnapshot newSnap = SourceSnapshot::read(newSrc);
  const auto res = detectBoolFieldDrift(oldSnap, newSnap, {std::filesystem::path{"src/a.h"}});
  REQUIRE(res.violations.size() == 1);
  REQUIRE(res.violations[0].file == "src/a.h");
}
