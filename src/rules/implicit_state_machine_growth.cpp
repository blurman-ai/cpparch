#include "archcheck/rules/implicit_state_machine_growth.h"

#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>

#include "archcheck/scan/project_files.h"

namespace archcheck::rules
{

namespace
{

// State-pattern names indicating state machines
const std::unordered_set<std::string> kStatePatterns = {
  "started",
  "running",
  "paused",
  "paused",
  "failed",
  "completed",
  "cancelled",
  "ready",
  "active",
  "enabled",
  "loaded",
  "connected",
  "initialized",
  "is_running",
  "is_active",
  "is_ready",
};

// Config-pattern names indicating configuration flags (not state)
const std::unordered_set<std::string> kConfigPatterns = {
  "enable_",
  "use_",
  "allow_",
  "verbose",
  "debug",
  "trace",
  "force_",
  "skip_",
  "ignore_",
};

// Struct names to exclude (known false positives)
const std::unordered_set<std::string> kExcludePatterns = {
  "Options",
  "Config",
  "Settings",
  "Flags",
  "Parameters",
  "Chord",
  "FlatC",
};

bool matchesPattern(std::string_view name, const std::unordered_set<std::string> &patterns)
{
  for (const auto &pattern : patterns)
  {
    if (pattern.back() == '_')
    {
      // Prefix pattern: enable_, use_, etc.
      if (name.find(pattern) == 0)
        return true;
    }
    else
    {
      // Exact match
      if (name == pattern)
        return true;
    }
  }
  return false;
}

} // namespace

ImplicitStateMachineGrowth::ImplicitStateMachineGrowth(std::size_t minBoolFields, double statePatternRatio)
  : minBoolFields_(minBoolFields), statePatternRatio_(statePatternRatio)
{
}

bool ImplicitStateMachineGrowth::isStatePatternName(std::string_view name)
{
  return matchesPattern(name, kStatePatterns);
}

bool ImplicitStateMachineGrowth::isConfigPatternName(std::string_view name)
{
  return matchesPattern(name, kConfigPatterns);
}

bool ImplicitStateMachineGrowth::shouldExclude(std::string_view structName)
{
  for (const auto &pattern : kExcludePatterns)
  {
    if (structName.find(pattern) != std::string::npos)
      return true;
  }
  return false;
}

std::vector<ImplicitStateMachineGrowth::BoolStruct> ImplicitStateMachineGrowth::scanSourceFile(
  std::string_view path, const std::string &source) const
{
  std::vector<BoolStruct> results;

  // Regex to find struct/class definitions: (struct|class) Name {
  std::regex structPattern(R"((?:struct|class)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*[^{]*)?\s*\{)");

  // Regex to find bool fields: bool fieldname; (with optional bitfield, init, etc.)
  std::regex boolFieldPattern(R"(\bbool\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*\d+)?(?:\s*=\s*[^;]+)?;)");

  std::string::const_iterator searchStart(source.cbegin());
  std::smatch structMatch;

  // Find each struct/class definition
  while (std::regex_search(searchStart, source.cend(), structMatch, structPattern))
  {
    const auto structName = structMatch[1].str();
    const auto structBegin = structMatch.position(0) + (searchStart - source.cbegin());

    // Find matching closing brace to extract body
    int braceCount = 0;
    bool inBody = false;
    std::size_t bodyStart = 0;
    std::size_t bodyEnd = 0;

    for (std::size_t i = structBegin; i < source.size(); ++i)
    {
      if (source[i] == '{')
      {
        if (!inBody)
        {
          inBody = true;
          bodyStart = i + 1;
        }
        braceCount++;
      }
      else if (source[i] == '}')
      {
        braceCount--;
        if (braceCount == 0 && inBody)
        {
          bodyEnd = i;
          break;
        }
      }
    }

    if (inBody && bodyEnd > bodyStart)
    {
      const auto structBody = source.substr(bodyStart, bodyEnd - bodyStart);

      // Count bool fields in this struct
      std::size_t numBools = 0;
      std::size_t stateMatches = 0;
      std::vector<std::string> boolNames;

      std::string::const_iterator fieldSearchStart(structBody.cbegin());
      std::smatch fieldMatch;

      while (std::regex_search(fieldSearchStart, structBody.cend(), fieldMatch, boolFieldPattern))
      {
        const auto fieldName = fieldMatch[1].str();

        // Skip if this looks like a method parameter (on same line with parentheses)
        const auto fieldPos = fieldMatch.position(0);
        const auto lineStart = structBody.rfind('\n', fieldPos);
        const auto lineEnd = structBody.find('\n', fieldPos);
        const auto line = structBody.substr((lineStart == std::string::npos) ? 0 : lineStart + 1,
                                           ((lineEnd == std::string::npos) ? structBody.size() : lineEnd) - ((lineStart == std::string::npos) ? 0 : lineStart + 1));

        // Skip if line contains parentheses (method signature)
        if (line.find('(') == std::string::npos && line.find(')') == std::string::npos)
        {
          numBools++;
          boolNames.push_back(fieldName);

          if (isStatePatternName(fieldName) && !isConfigPatternName(fieldName))
          {
            stateMatches++;
          }
        }

        fieldSearchStart = fieldMatch.suffix().first;
      }

      // Only report if meets threshold
      if (numBools >= minBoolFields_ && !shouldExclude(structName))
      {
        const auto stateRatio = numBools > 0 ? static_cast<double>(stateMatches) / numBools : 0.0;
        if (stateRatio >= statePatternRatio_)
        {
          results.push_back({structName, std::string(path), 0, numBools, boolNames, stateMatches});
        }
      }
    }

    // Move search forward
    searchStart = structMatch.suffix().first;
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

    // Only check header files
    if (!archcheck::scan::isHeaderFile(std::filesystem::path(path)))
      continue;

    const auto source = readFile(path);
    const auto structs = scanSourceFile(path, source);

    for (const auto &boolStruct : structs)
    {
      const auto stateRatio = boolStruct.numBoolFields > 0
                                ? static_cast<double>(boolStruct.statePatternMatches) / boolStruct.numBoolFields
                                : 0.0;

      // Format field names for message
      std::string fieldList;
      for (std::size_t j = 0; j < boolStruct.boolFieldNames.size() && j < 5; ++j)
      {
        if (j > 0)
          fieldList += ", ";
        fieldList += boolStruct.boolFieldNames[j];
      }
      if (boolStruct.boolFieldNames.size() > 5)
      {
        fieldList += ", ...";
      }

      const auto message = std::string("implicit state machine detected (") + std::to_string(boolStruct.numBoolFields) +
                           " bool fields: " + fieldList + "; state-pattern ratio: " + std::to_string(static_cast<int>(stateRatio * 100)) + "%). " +
                           "Consider State Pattern or enum class instead of boolean flags.";

      result.push_back({"implicit_state_machine_growth", std::string(path), 1, message});
    }
  }

  return result;
}

} // namespace archcheck::rules
