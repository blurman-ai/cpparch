#pragma once

#include <string>
#include <variant>
#include <vector>

namespace archcheck::config
{

struct ModuleDef
{
  std::string name;
  std::vector<std::string> paths;
};

struct LayersRule
{
  std::string name;
  std::vector<std::string> layers;
};

struct IndependenceRule
{
  std::string name;
  std::vector<std::string> modules;
};

struct ForbiddenRule
{
  std::string name;
  std::vector<std::string> from;
  std::vector<std::string> to;
};

using Rule = std::variant<LayersRule, IndependenceRule, ForbiddenRule>;

struct Config
{
  int version = 0;
  std::vector<ModuleDef> modules;
  std::vector<Rule> rules;
};

} // namespace archcheck::config
