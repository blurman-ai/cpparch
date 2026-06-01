#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/scan/file_classification.h"

using archcheck::scan::baseName;
using archcheck::scan::hasVendorLicenseHeader;
using archcheck::scan::isVendoredBasename;
using archcheck::scan::isVendoredDirName;
using archcheck::scan::isVendoredFile;
using archcheck::scan::pathHasVendoredDir;

// === Layer 1: curated single-file-lib basenames (#069) =======================

TEST_CASE("vendored basename: curated stems match any extension", "[scan][vendor]")
{
  REQUIRE(isVendoredBasename("qcustomplot.cpp"));
  REQUIRE(isVendoredBasename("qcustomplot.h"));
  REQUIRE(isVendoredBasename("sqlite3.c"));
  REQUIRE(isVendoredBasename("sqlite3.h"));
  REQUIRE(isVendoredBasename("tinyxml2.cpp"));
  REQUIRE(isVendoredBasename("lodepng.cpp"));
  REQUIRE(isVendoredBasename("cJSON.c")); // case-insensitive
}

TEST_CASE("vendored basename: full-name pins and prefix families", "[scan][vendor]")
{
  REQUIRE(isVendoredBasename("json.hpp"));
  REQUIRE(isVendoredBasename("httplib.h"));
  REQUIRE(isVendoredBasename("doctest.h"));
  REQUIRE(isVendoredBasename("catch.hpp"));
  REQUIRE(isVendoredBasename("stb_image.h")); // stb_* prefix
  REQUIRE(isVendoredBasename("stb_truetype.h"));
  REQUIRE(isVendoredBasename("imgui.cpp")); // imgui* prefix
  REQUIRE(isVendoredBasename("imgui_draw.cpp"));
}

TEST_CASE("vendored basename: author files are not vendored", "[scan][vendor]")
{
  REQUIRE_FALSE(isVendoredBasename("main.cpp"));
  REQUIRE_FALSE(isVendoredBasename("graph_builder.cpp"));
  REQUIRE_FALSE(isVendoredBasename("json_reporter.cpp")); // not "json.hpp"
  REQUIRE_FALSE(isVendoredBasename("mywidget.h"));
}

// === Layer 2: permissive-license header (#069) ===============================

TEST_CASE("vendored license header: catches renamed/unknown vendors", "[scan][vendor]")
{
  REQUIRE(hasVendorLicenseHeader("// SPDX-License-Identifier: MIT\nint x;"));
  REQUIRE(hasVendorLicenseHeader("/* Permission is hereby granted, free of charge ... */"));
  REQUIRE(hasVendorLicenseHeader("// Redistribution and use in source and binary forms"));
  REQUIRE(hasVendorLicenseHeader("// This is released into the public domain."));
}

TEST_CASE("vendored license header: plain author code does not fire", "[scan][vendor]")
{
  REQUIRE_FALSE(hasVendorLicenseHeader("#include <vector>\nint main() { return 0; }"));
  REQUIRE_FALSE(hasVendorLicenseHeader("// Copyright header without a license phrase"));
}

TEST_CASE("isVendoredFile combines name and license layers", "[scan][vendor]")
{
  REQUIRE(isVendoredFile("qcustomplot.cpp", "no license here"));                  // layer 1
  REQUIRE(isVendoredFile("renamed_lib.cpp", "// SPDX-License-Identifier: Zlib")); // layer 2
  REQUIRE_FALSE(isVendoredFile("widget.cpp", "#include <vector>\n"));             // neither
}

// === Vendored directories (#068, folded into #069) ===========================

TEST_CASE("vendored dir name: every spelling collapses", "[scan][vendor]")
{
  for (const auto *n :
       {"third_party", "ThirdParty", "3rd-party", "vendor", "node_modules", "extern", "deps", "contrib"})
  {
    REQUIRE(isVendoredDirName(n));
  }
  REQUIRE_FALSE(isVendoredDirName("src"));
  REQUIRE_FALSE(isVendoredDirName("include"));
}

TEST_CASE("pathHasVendoredDir tests directory segments only", "[scan][vendor]")
{
  REQUIRE(pathHasVendoredDir("third_party/fmt/format.h"));
  REQUIRE(pathHasVendoredDir("src/extern/lib/x.cpp"));
  REQUIRE_FALSE(pathHasVendoredDir("src/graph/graph_builder.cpp"));
  REQUIRE_FALSE(pathHasVendoredDir("vendor.cpp")); // file named like a dir, not a segment
}

TEST_CASE("baseName extracts the final path segment", "[scan][vendor]")
{
  REQUIRE(baseName("a/b/c.cpp") == "c.cpp");
  REQUIRE(baseName("c.cpp") == "c.cpp");
}
