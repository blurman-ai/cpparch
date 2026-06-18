#include <catch2/catch_test_macros.hpp>
#include <map>
#include <string>
#include <vector>

#include "archcheck/scan/new_clone_drift.h"
#include "archcheck/scan/source_snapshot.h"

namespace
{

using archcheck::scan::detectNewClones;
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

// A function well over the 30-token fragment floor, so the scanner emits it as
// one fragment. `prefix` makes the enclosing file unique (avoids the whole-file
// clone guard suppressing the pair).
std::string fileWithSharedFunc(const std::string &uniqueFn)
{
  return "int " + uniqueFn + "() { return 0; }\n" +
         "int shared(int x, int y)\n{\n"
         "  int a = x + y;\n"
         "  int b = a * x - y;\n"
         "  int c = b + a * 2;\n"
         "  int d = c - b + x;\n"
         "  int e = d * y + a;\n"
         "  int f = e - c + b;\n"
         "  int g = f + d - e;\n"
         "  return a + b + c + d + e + f + g;\n"
         "}\n";
}

// A distinct (non-clone) filler function. Several of these give the token
// corpus enough variety that IDF weighting is not degenerate — with only the
// two cloned files, every shared token has document-frequency 1.0 and IDF zeroes
// the weighted score, suppressing a real clone (a small-corpus artefact, not how
// the scanner behaves on a real repo).
std::string fillerFunc(const std::string &name, const std::string &op)
{
  return "double " + name +
         "(double p, double q)\n{\n"
         "  double r = p " +
         op +
         " q;\n"
         "  double s = r " +
         op +
         " p;\n"
         "  double t = s " +
         op +
         " q;\n"
         "  double u = t " +
         op +
         " r;\n"
         "  double v = u " +
         op +
         " s;\n"
         "  return v " +
         op +
         " t;\n"
         "}\n";
}

// Two files sharing one cloned function, plus filler files for a healthy IDF.
MapFileSource cloneCorpus()
{
  MapFileSource src;
  src.files["orig.cpp"] = fileWithSharedFunc("uniqOne");
  src.files["copy.cpp"] = fileWithSharedFunc("uniqTwo");
  src.files["m1.cpp"] = fillerFunc("alpha", "+");
  src.files["m2.cpp"] = fillerFunc("beta", "-");
  src.files["m3.cpp"] = fillerFunc("gamma", "*");
  src.files["m4.cpp"] = fillerFunc("delta", "/");
  return src;
}

archcheck::scan::AddedLineMap allLinesOf(const std::string &file, int upTo)
{
  archcheck::scan::AddedLineMap m;
  for (int ln = 1; ln <= upTo; ++ln)
    m[file].insert(ln);
  return m;
}

} // namespace

TEST_CASE("new_clone_drift: clone introduced in added lines fires", "[scan][newclone]")
{
  MapFileSource src = cloneCorpus();
  // Parent holds only the original — the copy is what this diff adds, so the
  // parent has no pair to suppress against.
  MapFileSource parent = cloneCorpus();
  parent.files.erase("copy.cpp");

  const auto res = detectNewClones(SourceSnapshot::read(src), SourceSnapshot::read(parent), allLinesOf("copy.cpp", 14));
  REQUIRE(res.violations.size() >= 1);
  REQUIRE(res.violations[0].ruleId == "DRIFT.NEW_CLONE");
  REQUIRE(res.violations[0].file == "copy.cpp");
  REQUIRE(res.violations[0].message.find("clone of orig.cpp") != std::string::npos);
}

TEST_CASE("new_clone_drift: pre-existing clone merely touched is silent", "[scan][newclone]")
{
  MapFileSource src = cloneCorpus();
  // The clone pair already exists in the parent tree; the diff merely touched
  // the copy (e.g. a reformat). The parent-guard must drop it.
  MapFileSource parent = cloneCorpus();

  const auto res = detectNewClones(SourceSnapshot::read(src), SourceSnapshot::read(parent), allLinesOf("copy.cpp", 14));
  REQUIRE(res.violations.empty());
}

TEST_CASE("new_clone_drift: clone outside added lines is silent", "[scan][newclone]")
{
  MapFileSource src = cloneCorpus();
  MapFileSource parent; // irrelevant here — added touches an unrelated file

  // Diff touched only an unrelated file — the clone pair must not report.
  const auto res =
      detectNewClones(SourceSnapshot::read(src), SourceSnapshot::read(parent), allLinesOf("unrelated.cpp", 5));
  REQUIRE(res.violations.empty());
}

TEST_CASE("new_clone_drift: empty added map yields nothing", "[scan][newclone]")
{
  MapFileSource src = cloneCorpus();
  MapFileSource parent;

  const auto res = detectNewClones(SourceSnapshot::read(src), SourceSnapshot::read(parent), {});
  REQUIRE(res.violations.empty());
}
