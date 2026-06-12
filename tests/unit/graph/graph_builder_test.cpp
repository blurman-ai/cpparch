#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>
#include <vector>

#include "archcheck/graph/graph_builder.h"
#include "archcheck/scan/file_source.h"
#include "archcheck/scan/project_files.h"

namespace
{

struct FakeSource : archcheck::scan::FileSource
{
  std::vector<archcheck::scan::ProjectFile> files;
  std::vector<std::pair<std::string, std::string>> blobs;
  std::vector<archcheck::scan::ProjectFile> list() override { return files; }
  std::string read(const std::string &path) override
  {
    for (const auto &b : blobs)
    {
      if (b.first == path)
      {
        return b.second;
      }
    }
    return "";
  }
};

} // namespace

// Case 1: all files carry the Apache banner → dominant (100% > 50%) → banner
// layer disabled → all 4 nodes kept, edge d.cpp→a.h preserved.
TEST_CASE("filterVendored: dominant Apache banner keeps all project nodes", "[graph][vendor][apache]")
{
  constexpr auto kApache = "// Licensed under the Apache License, Version 2.0\n";
  FakeSource src;
  src.files = {{"src/a.h"}, {"src/b.h"}, {"src/c.cpp"}, {"src/d.cpp"}};
  src.blobs = {
      {"src/a.h", std::string(kApache) + "#pragma once\n"},
      {"src/b.h", std::string(kApache) + "#pragma once\n"},
      {"src/c.cpp", std::string(kApache)},
      {"src/d.cpp", std::string(kApache) + "#include \"a.h\"\n"},
  };
  const auto r = archcheck::graph::buildGraphForSource(src);
  REQUIRE(r.graph.nodeCount() == 4);
  REQUIRE(r.counters.edges == 1);
}

// Case 2: minority-banner file (1/5 = 20%) is a real vendor → excluded, edge
// from clean file to it goes unresolved.
TEST_CASE("filterVendored: minority MIT banner is excluded as real vendor", "[graph][vendor][real_vendor]")
{
  FakeSource src;
  src.files = {{"src/a.h"}, {"src/b.h"}, {"src/c.cpp"}, {"src/d.cpp"}, {"src/mini_lib.h"}};
  src.blobs = {
      {"src/a.h", "// project code\n"},
      {"src/b.h", "// project code\n"},
      {"src/c.cpp", "#include \"mini_lib.h\"\n"},
      {"src/d.cpp", "// project code\n"},
      {"src/mini_lib.h", "/* Permission is hereby granted, free of charge */\n"},
  };
  const auto r = archcheck::graph::buildGraphForSource(src);
  REQUIRE(r.graph.nodeCount() == 4);
  REQUIRE(r.counters.edges == 0);
  REQUIRE(r.counters.external + r.counters.unresolved == 1);
}

// Case 3: exactly 50% banner (2/4) — NOT strictly > 50% → banner layer active
// → banner files excluded → only 2 clean nodes remain.
TEST_CASE("filterVendored: exactly-50pct banner is not dominant, banner files excluded", "[graph][vendor][threshold]")
{
  constexpr auto kApache = "// Licensed under the Apache License, Version 2.0\n";
  FakeSource src;
  src.files = {{"src/a.h"}, {"src/b.h"}, {"src/c.cpp"}, {"src/d.cpp"}};
  src.blobs = {
      {"src/a.h", std::string(kApache)},
      {"src/b.h", std::string(kApache)},
      {"src/c.cpp", "// project code\n"},
      {"src/d.cpp", "// project code\n"},
  };
  const auto r = archcheck::graph::buildGraphForSource(src);
  REQUIRE(r.graph.nodeCount() == 2);
}

// Case 4: dominant Apache banner (3/3 candidates), but qcustomplot.cpp is
// excluded by name-layer before banner counting → still 3 clean nodes.
TEST_CASE("filterVendored: name-layer qcustomplot excluded even when banner is dominant", "[graph][vendor][name_layer]")
{
  constexpr auto kApache = "// Licensed under the Apache License, Version 2.0\n";
  FakeSource src;
  src.files = {{"src/a.h"}, {"src/b.h"}, {"src/c.cpp"}, {"src/qcustomplot.cpp"}};
  src.blobs = {
      {"src/a.h", std::string(kApache)},
      {"src/b.h", std::string(kApache)},
      {"src/c.cpp", std::string(kApache)},
      {"src/qcustomplot.cpp", std::string(kApache)},
  };
  const auto r = archcheck::graph::buildGraphForSource(src);
  REQUIRE(r.graph.nodeCount() == 3);
}
