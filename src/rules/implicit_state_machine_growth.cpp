#include "archcheck/rules/implicit_state_machine_growth.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>

#include "archcheck/scan/project_files.h"

namespace archcheck::rules
{

namespace
{

// Lifecycle words: a field whose core matches one of these is "state-like".
const std::unordered_set<std::string> kStateWords = {
  "started",  "starting",   "running",  "paused",     "stopped",     "stopping", "failed",
  "completed", "complete",  "cancelled", "canceled",  "ready",       "active",   "inactive",
  "idle",     "loaded",     "loading",  "connected",  "connecting",  "disconnected",
  "initialized", "pending", "done",     "finished",   "aborted",     "waiting",  "busy",
  "opened",   "closing",    "closed",   "enabled",
};

std::string toLower(std::string_view s)
{
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}

bool endsWith(const std::string &s, std::string_view suf)
{
  return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// Strip a Hungarian/member prefix (m_, _, is_, b) so "m_isReady" → "ready".
std::string_view stripMemberPrefix(std::string_view core)
{
  if (core.rfind("m_", 0) == 0)
    core.remove_prefix(2);
  else if (core.rfind("_", 0) == 0)
    core.remove_prefix(1);
  if (core.rfind("is_", 0) == 0)
    core.remove_prefix(3);
  else if (core.rfind("b", 0) == 0 && core.size() > 1 && std::isupper(static_cast<unsigned char>(core[1])))
    core.remove_prefix(1); // bTransferred → Transferred
  return core;
}

// Config / capability / presence flags — these are NOT state, even if they
// happen to contain a state-ish word (has_connected, use_running_mode).
bool isConfigOrCapability(std::string_view nameView)
{
  const std::string n = toLower(nameView);
  std::string_view core = n;
  if (core.rfind("m_", 0) == 0)
    core.remove_prefix(2);
  else if (core.rfind("_", 0) == 0)
    core.remove_prefix(1);

  static constexpr std::array kPrefixes = {"has", "can",   "use",   "enable", "disable", "allow", "force",
                                           "skip", "ignore", "dev",  "want",   "verbose", "debug", "trace"};
  for (const auto *p : kPrefixes)
    if (core.rfind(p, 0) == 0)
      return true;

  static constexpr std::array kSuffixes = {"_enabled", "_ok", "_provided", "_created",
                                           "_specified", "_set", "_printed"};
  for (const auto *s : kSuffixes)
    if (endsWith(n, s))
      return true;

  static constexpr std::array kContains = {"uptodate", "_has_", "_can_", "_use_"};
  for (const auto *c : kContains)
    if (n.find(c) != std::string::npos)
      return true;

  return false;
}

bool isStateName(std::string_view name)
{
  if (isConfigOrCapability(name))
    return false;
  const std::string lower = toLower(name);
  const std::string core(stripMemberPrefix(lower));
  if (kStateWords.count(core) != 0)
    return true;
  // Suffix form: m_jobFailed, downloadCompleted, ...
  for (const auto &w : kStateWords)
    if (endsWith(lower, "_" + w) || (lower.size() > w.size() && endsWith(lower, w) &&
                                     std::isupper(static_cast<unsigned char>(name[name.size() - w.size() - 1]))))
      return true;
  return false;
}

// Vendored / third-party code: never our state machine to fix.
bool isVendoredPath(std::string_view path)
{
  // Leading slash so a relative "third_party/..." matches "/third_party/" too.
  const std::string p = "/" + toLower(path);
  static constexpr std::array kDirs = {"/third_party/", "/thirdparty/", "/3rdparty/", "/vendor/",
                                       "/vendored/",    "/extern/",     "/external/", "/_deps/",
                                       "/deps/",        "/tclap/"};
  for (const auto *d : kDirs)
    if (p.find(d) != std::string::npos)
      return true;

  static constexpr std::array kFiles = {"catch.hpp", "catch_config.hpp", "cgltf.h",
                                        "peglib.h",  "rapidjson.h",      "json.hpp"};
  for (const auto *f : kFiles)
    if (endsWith(p, f))
      return true;
  return false;
}

// Struct names that are config bags, not state machines.
bool isExcludedStructName(std::string_view structName)
{
  const std::string n = toLower(structName);
  static constexpr std::array kSuffixes = {"options", "config",   "configdata", "settings", "cache",
                                           "preferences", "prefs", "arguments",  "args",     "flags",
                                           "parameters",  "params", "opts",      "info"};
  for (const auto *s : kSuffixes)
    if (endsWith(n, s))
      return true;
  static constexpr std::array kContains = {"chord", "flatc"};
  for (const auto *c : kContains)
    if (n.find(c) != std::string::npos)
      return true;
  return false;
}

// Keep only characters at brace-depth 0 of the struct body, dropping method
// bodies entirely so local `bool result;` are not counted as fields.
std::string depthZeroSlice(const std::string &body)
{
  std::string out;
  int depth = 0;
  for (char c : body)
  {
    if (c == '{')
      ++depth;
    else if (c == '}')
    {
      if (depth > 0)
        --depth;
    }
    else if (depth == 0)
      out.push_back(c);
  }
  return out;
}

// Direct-member slice (depth 0) of the struct whose definition starts at
// structBegin, or empty if the body is unbalanced.
std::string memberSlice(const std::string &source, std::size_t structBegin)
{
  int braceCount = 0;
  std::size_t bodyStart = 0, bodyEnd = 0;
  bool inBody = false;
  for (std::size_t i = structBegin; i < source.size(); ++i)
  {
    if (source[i] == '{')
    {
      if (!inBody)
      {
        inBody = true;
        bodyStart = i + 1;
      }
      ++braceCount;
    }
    else if (source[i] == '}' && --braceCount == 0 && inBody)
    {
      bodyEnd = i;
      break;
    }
  }
  if (!inBody || bodyEnd <= bodyStart)
    return {};
  return depthZeroSlice(source.substr(bodyStart, bodyEnd - bodyStart));
}

struct FieldStats
{
  std::size_t numBools = 0;
  std::size_t stateMatches = 0;
  std::vector<std::string> names;
};

// Count direct bool fields (skipping method declarations on the same line) and
// how many of them look like lifecycle state.
FieldStats countBoolFields(const std::string &members)
{
  static const std::regex boolFieldPattern(R"(\bbool\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*\d+)?(?:\s*=\s*[^;]+)?;)");
  FieldStats st;
  auto it = members.cbegin();
  std::smatch m;
  while (std::regex_search(it, members.cend(), m, boolFieldPattern))
  {
    const auto name = m[1].str();
    const auto absPos = static_cast<std::size_t>(m[0].first - members.cbegin());
    it = m.suffix().first;
    const auto ls = members.rfind('\n', absPos);
    const auto le = members.find('\n', absPos);
    const auto from = (ls == std::string::npos) ? 0 : ls + 1;
    const auto line = members.substr(from, (le == std::string::npos ? members.size() : le) - from);
    if (line.find('(') != std::string::npos || line.find(')') != std::string::npos)
      continue; // method declaration, not a field
    ++st.numBools;
    st.names.push_back(name);
    if (isStateName(name))
      ++st.stateMatches;
  }
  return st;
}

} // namespace

ImplicitStateMachineGrowth::ImplicitStateMachineGrowth(std::size_t minBoolFields, double statePatternRatio)
  : minBoolFields_(minBoolFields), statePatternRatio_(statePatternRatio)
{
}

bool ImplicitStateMachineGrowth::isStatePatternName(std::string_view name) { return isStateName(name); }

bool ImplicitStateMachineGrowth::isConfigPatternName(std::string_view name) { return isConfigOrCapability(name); }

bool ImplicitStateMachineGrowth::shouldExclude(std::string_view structName) { return isExcludedStructName(structName); }

std::string ImplicitStateMachineGrowth::describe(const BoolStruct &s)
{
  std::string fieldList;
  for (std::size_t j = 0; j < s.boolFieldNames.size() && j < 5; ++j)
    fieldList += (j > 0 ? ", " : "") + s.boolFieldNames[j];
  if (s.boolFieldNames.size() > 5)
    fieldList += ", ...";
  const auto ratio = static_cast<double>(s.statePatternMatches) / static_cast<double>(s.numBoolFields);
  return "implicit state machine detected (" + std::to_string(s.numBoolFields) + " bool fields: " + fieldList +
         "; state-pattern ratio: " + std::to_string(static_cast<int>(ratio * 100)) +
         "%). Consider State Pattern or enum class instead of boolean flags.";
}

std::vector<ImplicitStateMachineGrowth::BoolStruct> ImplicitStateMachineGrowth::scanSourceFile(
  std::string_view path, const std::string &source) const
{
  std::vector<BoolStruct> results;
  static const std::regex structPattern(R"((?:struct|class)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*[^{]*)?\s*\{)");

  std::string::const_iterator searchStart(source.cbegin());
  std::smatch structMatch;
  while (std::regex_search(searchStart, source.cend(), structMatch, structPattern))
  {
    const auto structName = structMatch[1].str();
    const auto structBegin = static_cast<std::size_t>(structMatch.position(0) + (searchStart - source.cbegin()));
    searchStart = structMatch.suffix().first;

    if (isExcludedStructName(structName))
      continue;
    const auto members = memberSlice(source, structBegin);
    if (members.empty())
      continue;

    const auto st = countBoolFields(members);
    if (st.numBools < minBoolFields_)
      continue;
    const auto ratio = static_cast<double>(st.stateMatches) / static_cast<double>(st.numBools);
    if (ratio >= statePatternRatio_)
      results.push_back({structName, std::string(path), 0, st.numBools, st.names, st.stateMatches});
  }
  return results;
}

ViolationList ImplicitStateMachineGrowth::check(const graph::DependencyGraph &graph,
                                                const std::function<std::string(std::string_view)> &readFile) const
{
  ViolationList result;
  const auto count = static_cast<std::uint32_t>(graph.nodeCount());
  for (std::uint32_t i = 0; i < count; ++i)
  {
    const graph::NodeId id{i};
    const auto path = graph.pathOf(id);
    if (!archcheck::scan::isHeaderFile(std::filesystem::path(path)) || isVendoredPath(path))
      continue;

    const auto source = readFile(path);
    for (const auto &boolStruct : scanSourceFile(path, source))
      result.push_back({"implicit_state_machine_growth", std::string(path), 1, describe(boolStruct)});
  }
  return result;
}

} // namespace archcheck::rules
