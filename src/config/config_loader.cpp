#include "archcheck/config/config_loader.h"

#include <algorithm>
#include <fstream>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <sstream>
#include <unordered_set>

namespace archcheck::config
{

ConfigError::ConfigError(std::string file, int line, int column, const std::string &message)
    : std::runtime_error(file + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + message),
      file_(std::move(file)), line_(line), column_(column)
{
}

namespace
{

std::string read_file(const std::filesystem::path &path)
{
  std::ifstream in(path);
  if (!in)
  {
    throw ConfigError(path.string(), 0, 0, "cannot open config file");
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

int parse_version(const ryml::ConstNodeRef &root, const std::string &file)
{
  if (!root.is_map() || !root.has_child("version"))
  {
    throw ConfigError(file, 0, 0, "top-level must be a map with a 'version' key");
  }
  int v = 0;
  root["version"] >> v;
  if (v != 1)
  {
    throw ConfigError(file, 0, 0, "unsupported schema version " + std::to_string(v) + ", this binary supports v1");
  }
  return v;
}

std::string to_string(ryml::csubstr s) { return std::string(s.data(), s.size()); }

bool is_valid_module_char(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }

void validate_module_name(const std::string &name, const std::string &file)
{
  if (name.empty() || !std::all_of(name.begin(), name.end(), is_valid_module_char))
  {
    throw ConfigError(file, 0, 0, "invalid module name '" + name + "' (expected [a-z0-9_-]+)");
  }
}

ModuleDef parse_module_def(const ryml::ConstNodeRef &node, const std::string &file)
{
  ModuleDef def;
  def.name = to_string(node.key());
  validate_module_name(def.name, file);
  if (!node.has_child("paths"))
  {
    throw ConfigError(file, 0, 0, "module '" + def.name + "' must have a 'paths' list");
  }
  const auto paths = node["paths"];
  if (!paths.is_seq() || paths.num_children() == 0)
  {
    throw ConfigError(file, 0, 0, "module '" + def.name + "' has empty or non-list 'paths'");
  }
  for (const auto &p : paths.children())
  {
    std::string pat = to_string(p.val());
    if (pat.empty())
    {
      throw ConfigError(file, 0, 0, "module '" + def.name + "' has an empty path entry");
    }
    def.paths.push_back(std::move(pat));
  }
  return def;
}

void parse_modules(const ryml::ConstNodeRef &root, const std::string &file, Config &config)
{
  const auto modules_node = root["modules"];
  if (!modules_node.is_map() || modules_node.num_children() == 0)
  {
    throw ConfigError(file, 0, 0, "'modules' must be a non-empty map");
  }
  std::unordered_set<std::string> seen;
  for (const auto &mod : modules_node.children())
  {
    ModuleDef def = parse_module_def(mod, file);
    if (!seen.insert(def.name).second)
    {
      throw ConfigError(file, 0, 0, "duplicate module name '" + def.name + "'");
    }
    config.modules.push_back(std::move(def));
  }
}

void validate_top_keys(const ryml::ConstNodeRef &root, const std::string &file)
{
  for (const auto &child : root.children())
  {
    const ryml::csubstr key = child.key();
    const std::string k(key.data(), key.size());
    if (k != "version" && k != "modules" && k != "rules")
    {
      throw ConfigError(file, 0, 0, "unknown top-level key '" + k + "' (expected: version, modules, rules)");
    }
  }
  if (!root.has_child("modules"))
  {
    throw ConfigError(file, 0, 0, "missing required top-level key 'modules'");
  }
  if (!root.has_child("rules"))
  {
    throw ConfigError(file, 0, 0, "missing required top-level key 'rules'");
  }
}

} // namespace

Config load(const std::filesystem::path &path)
{
  const std::string source = read_file(path);
  ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(source));
  const ryml::ConstNodeRef root = tree.crootref();

  Config config;
  config.version = parse_version(root, path.string());
  validate_top_keys(root, path.string());
  parse_modules(root, path.string(), config);
  return config;
}

} // namespace archcheck::config
