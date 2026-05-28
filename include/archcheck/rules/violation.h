#pragma once

#include <string>
#include <vector>

namespace archcheck::rules
{

struct Violation
{
  std::string ruleId;
  std::string file;
  int line; // 0 = not applicable (graph-level rules)
  std::string message;
};

using ViolationList = std::vector<Violation>;

} // namespace archcheck::rules
