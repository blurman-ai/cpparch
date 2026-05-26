#include <string>

#include <catch2/catch_test_macros.hpp>

#include "archcheck/version.h"

TEST_CASE("version string is non-empty", "[smoke]")
{
   REQUIRE(archcheck::kVersionString != nullptr);
   REQUIRE(std::string{archcheck::kVersionString}.size() > 0);
}

TEST_CASE("version components are non-negative", "[smoke]")
{
   STATIC_REQUIRE(archcheck::kVersionMajor >= 0);
   STATIC_REQUIRE(archcheck::kVersionMinor >= 0);
   STATIC_REQUIRE(archcheck::kVersionPatch >= 0);
}
