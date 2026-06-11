#include "archcheck/config/config_loader.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <sstream>
#include <unordered_set>

namespace archcheck::config
{

ConfigError::ConfigError(const std::string &file, int line, int column, const std::string &message)
    : std::runtime_error(file + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + message)
{
}

namespace
{

// ryml's default error handler calls abort(), which would kill the process on
// a malformed .archcheck.yml instead of honouring the exit-code contract (2).
// Same pattern as graph/baseline.cpp: install a throwing handler for the whole
// parse+walk, restore the defaults on scope exit, translate into ConfigError.
struct RymlParseException
{
  std::string message;
};

[[noreturn]] void on_ryml_error(const char *msg, std::size_t msg_len, ryml::Location /*loc*/, void * /*user_data*/)
{
  throw RymlParseException{std::string{msg, msg_len}};
}

struct RymlErrorGuard
{
  ryml::Callbacks defaults = ryml::get_callbacks();
  RymlErrorGuard() { ryml::set_callbacks(ryml::Callbacks(nullptr, nullptr, nullptr, &on_ryml_error)); }
  ~RymlErrorGuard() { ryml::set_callbacks(defaults); }
};

struct LoaderCtx
{
  const ryml::Parser *parser;
  std::string file;
};

void throw_at(const LoaderCtx &ctx, const ryml::ConstNodeRef &node, const std::string &msg)
{
  const ryml::Location loc = ctx.parser->location(node);
  throw ConfigError(ctx.file, static_cast<int>(loc.line), static_cast<int>(loc.col), msg);
}

void throw_top(const LoaderCtx &ctx, const std::string &msg) { throw ConfigError(ctx.file, 1, 1, msg); }

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

int parse_version(const ryml::ConstNodeRef &root, const LoaderCtx &ctx)
{
  if (!root.is_map() || !root.has_child("version"))
  {
    throw_top(ctx, "top-level must be a map with a 'version' key");
  }
  int v = 0;
  root["version"] >> v;
  if (v != 1)
  {
    throw_at(ctx, root["version"], "unsupported schema version " + std::to_string(v) + ", this binary supports v1");
  }
  return v;
}

std::string to_string(ryml::csubstr s) { return std::string(s.data(), s.size()); }

bool is_valid_module_char(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }

void validate_module_name(const ryml::ConstNodeRef &node, const std::string &name, const LoaderCtx &ctx)
{
  if (name.empty() || !std::all_of(name.begin(), name.end(), is_valid_module_char))
  {
    throw_at(ctx, node, "invalid module name '" + name + "' (expected [a-z0-9_-]+)");
  }
}

ModuleDef parse_module_def(const ryml::ConstNodeRef &node, const LoaderCtx &ctx)
{
  ModuleDef def;
  def.name = to_string(node.key());
  validate_module_name(node, def.name, ctx);
  if (!node.has_child("paths"))
  {
    throw_at(ctx, node, "module '" + def.name + "' must have a 'paths' list");
  }
  const auto paths = node["paths"];
  if (!paths.is_seq() || paths.num_children() == 0)
  {
    throw_at(ctx, paths, "module '" + def.name + "' has empty or non-list 'paths'");
  }
  for (const auto &p : paths.children())
  {
    std::string pat = to_string(p.val());
    if (pat.empty())
    {
      throw_at(ctx, p, "module '" + def.name + "' has an empty path entry");
    }
    def.paths.push_back(std::move(pat));
  }
  return def;
}

void parse_modules(const ryml::ConstNodeRef &root, const LoaderCtx &ctx, Config &config)
{
  const auto modules_node = root["modules"];
  if (!modules_node.is_map() || modules_node.num_children() == 0)
  {
    throw_at(ctx, modules_node, "'modules' must be a non-empty map");
  }
  std::unordered_set<std::string> seen;
  for (const auto &mod : modules_node.children())
  {
    ModuleDef def = parse_module_def(mod, ctx);
    if (!seen.insert(def.name).second)
    {
      throw_at(ctx, mod, "duplicate module name '" + def.name + "'");
    }
    config.modules.push_back(std::move(def));
  }
}

std::vector<std::string> parse_string_list(const ryml::ConstNodeRef &node, const std::string &field,
                                           const std::string &rule_name, const LoaderCtx &ctx)
{
  if (!node.is_seq() || node.num_children() == 0)
  {
    throw_at(ctx, node, "rule '" + rule_name + "': '" + field + "' must be a non-empty list");
  }
  std::vector<std::string> out;
  for (const auto &item : node.children())
  {
    out.push_back(to_string(item.val()));
  }
  return out;
}

LayersRule parse_layers_rule(const ryml::ConstNodeRef &node, std::string name, const LoaderCtx &ctx)
{
  if (!node.has_child("layers"))
  {
    throw_at(ctx, node, "rule '" + name + "' of type 'layers' requires a 'layers' list");
  }
  LayersRule rule;
  rule.name = std::move(name);
  rule.layers = parse_string_list(node["layers"], "layers", rule.name, ctx);
  return rule;
}

IndependenceRule parse_independence_rule(const ryml::ConstNodeRef &node, std::string name, const LoaderCtx &ctx)
{
  if (!node.has_child("modules"))
  {
    throw_at(ctx, node, "rule '" + name + "' of type 'independence' requires a 'modules' list");
  }
  IndependenceRule rule;
  rule.name = std::move(name);
  rule.modules = parse_string_list(node["modules"], "modules", rule.name, ctx);
  return rule;
}

ForbiddenRule parse_forbidden_rule(const ryml::ConstNodeRef &node, std::string name, const LoaderCtx &ctx)
{
  if (!node.has_child("from") || !node.has_child("to"))
  {
    throw_at(ctx, node, "rule '" + name + "' of type 'forbidden' requires both 'from' and 'to' lists");
  }
  ForbiddenRule rule;
  rule.name = std::move(name);
  rule.from = parse_string_list(node["from"], "from", rule.name, ctx);
  rule.to = parse_string_list(node["to"], "to", rule.name, ctx);
  return rule;
}

Rule parse_rule(const ryml::ConstNodeRef &node, const LoaderCtx &ctx)
{
  if (!node.is_map() || !node.has_child("type") || !node.has_child("name"))
  {
    throw_at(ctx, node, "each rule must be a map with 'type' and 'name'");
  }
  std::string name = to_string(node["name"].val());
  std::string type = to_string(node["type"].val());
  if (type == "layers")
  {
    return parse_layers_rule(node, std::move(name), ctx);
  }
  if (type == "independence")
  {
    return parse_independence_rule(node, std::move(name), ctx);
  }
  if (type == "forbidden")
  {
    return parse_forbidden_rule(node, std::move(name), ctx);
  }
  throw_at(ctx, node["type"],
           "rule '" + name + "' has unknown type '" + type + "' (expected: layers, independence, forbidden)");
  return Rule{}; // unreachable
}

void validate_member_list(const std::vector<std::string> &items, const std::string &field, const std::string &rule_name,
                          const std::unordered_set<std::string> &known_modules, const LoaderCtx &ctx)
{
  std::unordered_set<std::string> seen;
  for (const auto &name : items)
  {
    if (known_modules.count(name) == 0)
    {
      throw_top(ctx, "rule '" + rule_name + "' references unknown module '" + name + "' in '" + field + "'");
    }
    if (!seen.insert(name).second)
    {
      throw_top(ctx, "rule '" + rule_name + "' has duplicate '" + name + "' in '" + field + "'");
    }
  }
}

void validate_forbidden_disjoint(const ForbiddenRule &rule, const LoaderCtx &ctx)
{
  std::unordered_set<std::string> from_set(rule.from.begin(), rule.from.end());
  for (const auto &t : rule.to)
  {
    if (from_set.count(t) > 0)
    {
      throw_top(ctx, "rule '" + rule.name + "' has module '" + t + "' in both 'from' and 'to'");
    }
  }
}

struct RuleValidator
{
  const std::unordered_set<std::string> &modules;
  const LoaderCtx &ctx;

  void operator()(const LayersRule &r) const { validate_member_list(r.layers, "layers", r.name, modules, ctx); }
  void operator()(const IndependenceRule &r) const { validate_member_list(r.modules, "modules", r.name, modules, ctx); }
  void operator()(const ForbiddenRule &r) const
  {
    validate_member_list(r.from, "from", r.name, modules, ctx);
    validate_member_list(r.to, "to", r.name, modules, ctx);
    validate_forbidden_disjoint(r, ctx);
  }
};

void cross_validate(const Config &config, const LoaderCtx &ctx)
{
  std::unordered_set<std::string> modules;
  for (const auto &m : config.modules)
  {
    modules.insert(m.name);
  }
  const RuleValidator visitor{modules, ctx};
  for (const auto &rule : config.rules)
  {
    std::visit(visitor, rule);
  }
}

void parse_rules(const ryml::ConstNodeRef &root, const LoaderCtx &ctx, Config &config)
{
  const auto rules_node = root["rules"];
  if (!rules_node.is_seq())
  {
    throw_at(ctx, rules_node, "'rules' must be a list");
  }
  std::unordered_set<std::string> names;
  for (const auto &rule_node : rules_node.children())
  {
    Rule rule = parse_rule(rule_node, ctx);
    std::string name = std::visit([](const auto &r) { return r.name; }, rule);
    if (!names.insert(name).second)
    {
      throw_at(ctx, rule_node, "duplicate rule name '" + name + "'");
    }
    config.rules.push_back(std::move(rule));
  }
}

std::size_t parse_positive_int(const ryml::ConstNodeRef &node, const std::string &key, const LoaderCtx &ctx)
{
  const ryml::csubstr s = node.val();
  unsigned long long value = 0;
  const auto res = std::from_chars(s.data(), s.data() + s.size(), value);
  if (res.ec != std::errc{} || res.ptr != s.data() + s.size() || value == 0)
  {
    throw_at(ctx, node, "threshold '" + key + "' must be a positive integer");
  }
  return static_cast<std::size_t>(value);
}

// Optional phase-2 block; absent keys keep the embedded defaults in config.thresholds.
void parse_thresholds(const ryml::ConstNodeRef &root, const LoaderCtx &ctx, Config &config)
{
  if (!root.has_child("thresholds"))
  {
    return;
  }
  const auto node = root["thresholds"];
  if (!node.is_map())
  {
    throw_at(ctx, node, "'thresholds' must be a map");
  }
  for (const auto &child : node.children())
  {
    const std::string key = to_string(child.key());
    if (key == "chain_length")
    {
      config.thresholds.chainLength = parse_positive_int(child, key, ctx);
    }
    else if (key == "god_header_fan_in")
    {
      config.thresholds.godHeaderFanIn = parse_positive_int(child, key, ctx);
    }
    else
    {
      throw_at(ctx, child, "unknown threshold key '" + key + "' (expected: chain_length, god_header_fan_in)");
    }
  }
}

void validate_top_keys(const ryml::ConstNodeRef &root, const LoaderCtx &ctx)
{
  for (const auto &child : root.children())
  {
    const ryml::csubstr key = child.key();
    const std::string k(key.data(), key.size());
    if (k != "version" && k != "modules" && k != "rules" && k != "thresholds")
    {
      throw_at(ctx, child, "unknown top-level key '" + k + "' (expected: version, modules, rules, thresholds)");
    }
  }
  if (!root.has_child("modules"))
  {
    throw_top(ctx, "missing required top-level key 'modules'");
  }
  if (!root.has_child("rules"))
  {
    throw_top(ctx, "missing required top-level key 'rules'");
  }
}

} // namespace

Config load(const std::filesystem::path &path)
{
  std::string source = read_file(path);
  const RymlErrorGuard guard;
  try
  {
    ryml::EventHandlerTree evt_handler;
    ryml::Parser parser(&evt_handler, ryml::ParserOptions().locations(true));
    ryml::Tree tree = ryml::parse_in_arena(&parser, ryml::to_csubstr(path.string()), ryml::to_csubstr(source));
    const ryml::ConstNodeRef root = tree.crootref();
    const LoaderCtx ctx{&parser, path.string()};

    Config config;
    config.version = parse_version(root, ctx);
    validate_top_keys(root, ctx);
    parse_modules(root, ctx, config);
    parse_rules(root, ctx, config);
    parse_thresholds(root, ctx, config);
    cross_validate(config, ctx);
    return config;
  }
  catch (const RymlParseException &e)
  {
    throw ConfigError(path.string(), 0, 0, "YAML error: " + e.message);
  }
}

std::optional<std::filesystem::path> findConfig(const std::filesystem::path &start)
{
  std::error_code ec;
  std::filesystem::path dir = std::filesystem::absolute(start, ec);
  if (ec)
  {
    dir = start;
  }
  for (;;)
  {
    const std::filesystem::path candidate = dir / ".archcheck.yml";
    if (std::filesystem::is_regular_file(candidate, ec))
    {
      return candidate;
    }
    const std::filesystem::path parent = dir.parent_path();
    if (parent == dir)
    {
      return std::nullopt;
    }
    dir = parent;
  }
}

Config discover(const std::filesystem::path &root)
{
  const auto found = findConfig(root);
  return found ? load(*found) : Config{};
}

} // namespace archcheck::config
