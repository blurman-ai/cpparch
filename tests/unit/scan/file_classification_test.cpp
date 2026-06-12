#include <catch2/catch_test_macros.hpp>
#include <string>

#include "archcheck/scan/file_classification.h"

using archcheck::scan::baseName;
using archcheck::scan::hasVendorLicenseHeader;
using archcheck::scan::isTestBasename;
using archcheck::scan::isTestDirName;
using archcheck::scan::isVendoredBasename;
using archcheck::scan::isVendoredDirName;
using archcheck::scan::isVendoredFile;
using archcheck::scan::pathHasTestDir;
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
  REQUIRE(hasVendorLicenseHeader("/* Permission is hereby granted, free of charge ... */"));
  REQUIRE(hasVendorLicenseHeader("// Redistribution and use in source and binary forms"));
  REQUIRE(hasVendorLicenseHeader("// This is released into the public domain."));
  REQUIRE(hasVendorLicenseHeader("// Licensed under the Apache License, Version 2.0"));
}

TEST_CASE("vendored license header: plain author code does not fire", "[scan][vendor]")
{
  REQUIRE_FALSE(hasVendorLicenseHeader("#include <vector>\nint main() { return 0; }"));
  REQUIRE_FALSE(hasVendorLicenseHeader("// Copyright header without a license phrase"));
}

// #081: a bare SPDX-License-Identifier tag is now standard on *authored* code
// (KDE etc.), not a vendor signal. Only full verbatim license texts fire layer 2.
TEST_CASE("vendored license header: bare SPDX tag is not a vendor signal", "[scan][vendor]")
{
  REQUIRE_FALSE(hasVendorLicenseHeader("// SPDX-License-Identifier: GPL-3.0-or-later\nint x;"));
  REQUIRE_FALSE(hasVendorLicenseHeader("/* SPDX-License-Identifier: MIT */\nclass Widget {};"));
}

TEST_CASE("isVendoredFile combines name and license layers", "[scan][vendor]")
{
  REQUIRE(isVendoredFile("qcustomplot.cpp", "no license here"));                           // layer 1
  REQUIRE(isVendoredFile("renamed_lib.cpp", "// Permission is hereby granted, free ...")); // layer 2
  REQUIRE_FALSE(isVendoredFile("widget.cpp", "#include <vector>\n"));                      // neither
  REQUIRE_FALSE(isVendoredFile("widget.cpp", "// SPDX-License-Identifier: GPL-3.0\n"));    // #081: SPDX-only
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

TEST_CASE("vendored dir name: dotted version suffix maps to the lib name", "[scan][vendor]")
{
  // #109 corpus: `src/zlib-1.3.2/` escaped the plain `zlib` entry.
  REQUIRE(pathHasVendoredDir("src/zlib-1.3.2/zutil.c"));
  REQUIRE(pathHasVendoredDir("libs/freetype-2.13.0/src/base/ftbase.c"));
  REQUIRE_FALSE(pathHasVendoredDir("src/engine-1.3.2/core.cpp")); // unknown stem stays in
  REQUIRE_FALSE(pathHasVendoredDir("src/zlib2/x.c"));             // undotted tail is a name, not a version
}

TEST_CASE("vendored dir name: in-tree bundled libraries (not under third_party/)", "[scan][vendor]")
{
  // Multi-file libs dropped under their own dir, no third_party/ wrapper.
  for (const auto *n : {"qhull", "jpeglib", "agg", "hidapi", "libigl", "glu-libtess", "bzip2", "libpng"})
  {
    REQUIRE(isVendoredDirName(n));
  }
  REQUIRE(pathHasVendoredDir("src/qhull/src/libqhull/rboxlib.c"));
  REQUIRE(pathHasVendoredDir("source/Irrlicht/jpeglib/jdhuff.c"));
  REQUIRE(pathHasVendoredDir("src/agg/agg_renderer_base.h"));
  REQUIRE_FALSE(pathHasVendoredDir("src/slic3r/GUI/Gizmos/GLGizmoColorCut.cpp")); // authored stays in
}

TEST_CASE("baseName extracts the final path segment", "[scan][vendor]")
{
  REQUIRE(baseName("a/b/c.cpp") == "c.cpp");
  REQUIRE(baseName("c.cpp") == "c.cpp");
}

// === Unit/integration test exclusion (#070) ==================================

TEST_CASE("test dir name: every spelling collapses", "[scan][test]")
{
  for (const auto *n : {"test", "tests", "unit_test", "unit-tests", "UnitTests"})
  {
    REQUIRE(isTestDirName(n));
  }
  REQUIRE_FALSE(isTestDirName("src"));
  REQUIRE_FALSE(isTestDirName("latest")); // whole-segment match, not substring
}

TEST_CASE("pathHasTestDir tests directory segments only", "[scan][test]")
{
  REQUIRE(pathHasTestDir("tests/unit/foo.cpp"));
  REQUIRE(pathHasTestDir("src/test/bar.cpp"));
  REQUIRE(pathHasTestDir("be/src/testutil/scoped-flag-setter.h"));
  REQUIRE(pathHasTestDir("be/src/test_utils/foo.h")); // normalizes to testutils
  REQUIRE_FALSE(pathHasTestDir("src/graph/graph_builder.cpp"));
  REQUIRE_FALSE(pathHasTestDir("latest/x.cpp")); // segment is "latest", not "test"
  REQUIRE_FALSE(pathHasTestDir("be/src/observe/span-manager.cc"));
  REQUIRE_FALSE(pathHasTestDir("src/testutility/foo.h")); // segment != testutil/testutils
}

TEST_CASE("isTestBasename matches test_/_test/_tests/_spec stems", "[scan][test]")
{
  REQUIRE(isTestBasename("graph_test.cpp"));
  REQUIRE(isTestBasename("graph_tests.cpp"));
  REQUIRE(isTestBasename("test_helpers.cpp"));
  REQUIRE(isTestBasename("widget_spec.cpp"));
  REQUIRE(isTestBasename("Foo_Test.CPP")); // case-insensitive
  REQUIRE(isTestBasename("otel-test.cc"));
  REQUIRE(isTestBasename("unit-tests.cpp"));
  REQUIRE(isTestBasename("test-main.cc"));
  REQUIRE(isTestBasename("widget-spec.cpp"));
  REQUIRE_FALSE(isTestBasename("graph_builder.cpp"));
  REQUIRE_FALSE(isTestBasename("contest.cpp")); // not a _test suffix
  REQUIRE_FALSE(isTestBasename("contest.cc"));  // no separator before 'test'
  REQUIRE_FALSE(isTestBasename("attest.h"));    // ditto
  REQUIRE_FALSE(isTestBasename("latest.h"));    // ditto
}
