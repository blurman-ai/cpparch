#include "archcheck/config/config_loader.h"

#include <fstream>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <sstream>

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
  return config;
}

} // namespace archcheck::config
