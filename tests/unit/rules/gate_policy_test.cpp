#include <catch2/catch_test_macros.hpp>

#include "archcheck/rules/gate_policy.h"

using archcheck::rules::classifyForGate;
using archcheck::rules::countGating;
using archcheck::rules::FindingDisposition;
using archcheck::rules::GateMode;
using archcheck::rules::isGating;

TEST_CASE("gate policy: check mode gates only SF.9", "[rules][gate-policy]")
{
  CHECK(classifyForGate("SF.9", GateMode::Check) == FindingDisposition::Gating);
  CHECK(classifyForGate("SF.7", GateMode::Check) == FindingDisposition::Advisory);
  CHECK(classifyForGate("Lakos.GodHeader", GateMode::Check) == FindingDisposition::Advisory);
}

TEST_CASE("gate policy: drift mode gates regression-grade drift rules", "[rules][gate-policy]")
{
  CHECK(isGating("DRIFT.1", GateMode::Drift));
  CHECK(isGating("DRIFT.2", GateMode::Drift));
  CHECK(isGating("DRIFT.4.CYCLE", GateMode::Drift));
  CHECK_FALSE(isGating("DRIFT.3", GateMode::Drift));
  CHECK_FALSE(isGating("DRIFT.4.NEW", GateMode::Drift));
  CHECK_FALSE(isGating("DRIFT.4.SDP", GateMode::Drift));
}

TEST_CASE("gate policy: counts gating violations", "[rules][gate-policy]")
{
  const archcheck::rules::ViolationList violations = {
      {"SF.9", "a.h", 0, "cycle"},
      {"SF.7", "b.h", 1, "using namespace"},
      {"Lakos.ChainLength", "c.h", 0, "chain"},
  };

  CHECK(countGating(violations, GateMode::Check) == 1);
}
