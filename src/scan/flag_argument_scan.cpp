#include "archcheck/scan/flag_argument_scan.h"

#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>

#include "archcheck/scan/function_body_scan.h"

namespace archcheck::scan
{

namespace
{

using duplication::Token;

// A bool-typed parameter whose name reads like a flag. Prefix match on a word
// boundary so `node`/`normal` (start "no") do NOT trip, but `enableX`/`skip_y` do.
bool flagLikeName(const std::string &name)
{
  static constexpr std::array<std::string_view, 10> kPrefixes = {"enable", "disable", "use",   "force", "skip",
                                                                 "with",   "without", "allow", "is",    "no"};
  std::string low;
  for (char c : name)
    low.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  for (std::string_view p : kPrefixes)
  {
    if (low.rfind(p, 0) != 0)
      continue;
    if (low.size() == p.size())
      return true; // exactly the prefix word
    const char next = name[p.size()];
    if (next == '_' || (std::isupper(static_cast<unsigned char>(next)) != 0))
      return true; // boundary: snake_ or camelCase
  }
  return false;
}

// +1 for an opening bracket, -1 for a closing one, 0 otherwise. Lets the param
// walk skip anything nested (template args, default-arg parens, init lists).
int bracketDelta(const std::string &s)
{
  if (s == "(" || s == "[" || s == "{" || s == "<")
    return 1;
  if (s == ")" || s == "]" || s == "}" || s == ">")
    return -1;
  return 0;
}

// One parameter segment [lo, hi): a by-value bool parameter (not bool* / bool&,
// which are out-parameters). Sets `name` to its last identifier.
bool segmentIsBoolFlag(const std::vector<Token> &t, std::size_t lo, std::size_t hi, std::string &name)
{
  bool hasBool = false;
  bool hasPtrRef = false;
  int depth = 0;
  for (std::size_t k = lo; k < hi && k < t.size(); ++k)
  {
    const std::string &s = t[k].sym;
    const int d = bracketDelta(s);
    if (d != 0)
      depth += d;
    else if (depth > 0)
      continue;
    else if (s == "bool")
      hasBool = true;
    else if (s == "*" || s == "&")
      hasPtrRef = true;
    else if (s == "id")
      name = t[k].raw;
  }
  return hasBool && !hasPtrRef;
}

// Count top-level by-value bool parameters in [open+1, close). lastName receives
// the last such parameter's identifier (for the single-flag name check).
int countBoolParams(const std::vector<Token> &t, std::size_t open, std::size_t close, std::string &lastName)
{
  int count = 0;
  int depth = 0;
  std::size_t segStart = open + 1;
  for (std::size_t k = open + 1; k <= close && k < t.size(); ++k)
  {
    const int d = (k == close) ? 0 : bracketDelta(t[k].sym);
    if (d != 0)
    {
      depth += d;
      continue;
    }
    if (k != close && !(depth == 0 && t[k].sym == ","))
      continue;
    std::string name;
    if (segmentIsBoolFlag(t, segStart, k, name))
    {
      ++count;
      lastName = name;
    }
    segStart = k + 1;
  }
  return count;
}

} // namespace

rules::ViolationList detectFlagArguments(const std::vector<Token> &tokens, const std::string &file)
{
  rules::ViolationList out;
  std::unordered_set<std::string> seen; // dedup declaration vs definition
  for (const auto &fn : discoverFunctions(tokens))
  {
    std::string lastName;
    const int boolParams = countBoolParams(tokens, fn.paramOpen, fn.paramClose, lastName);
    const bool many = boolParams >= 2;
    const bool flagged = many || (boolParams == 1 && flagLikeName(lastName));
    if (!flagged)
      continue;
    const std::string key = fn.qualifiedName + "/" + std::to_string(fn.paramArity);
    if (!seen.insert(key).second)
      continue;
    std::string msg =
        many ? ("function '" + fn.qualifiedName + "' takes " + std::to_string(boolParams) + " boolean flag parameters")
             : ("function '" + fn.qualifiedName + "' takes a boolean flag parameter '" + lastName + "'");
    out.push_back({"ARG.1.flag_argument_signature", file, fn.startLine, std::move(msg)});
  }
  return out;
}

} // namespace archcheck::scan
