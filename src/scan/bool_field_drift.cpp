#include "archcheck/scan/bool_field_drift.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "archcheck/scan/source_snapshot.h"

namespace archcheck::scan
{
namespace
{

constexpr const char *kRuleId = "DRIFT.BOOL_FIELD_ACCRETION";

// Ported from the validated research parser (experiments/boolean_state/perstruct_drift.py),
// then hardened by the #161 manual audit (#164): qualified struct names (Type::Function),
// const-qualified fields, brace-init fields. `static` members stay excluded (class
// constants, not instance state); `constexpr` never matches `const\s`.
const std::regex kStructRe(R"((?:struct|class)\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*(?:final\s*)?(?::[^{;]*)?\{)");
const std::regex kBoolRe(
    R"(^\s*(?:(?:mutable|const)\s+)?bool\s+([A-Za-z_]\w*)\s*(?::\s*\d+)?(?:\s*=\s*[^;]+|\s*\{[^{};]*\})?;\s*(?://.*)?$)");

struct BoolField
{
  std::string structName;
  std::string fieldName;
  int line = 0;
};

enum class Lex
{
  Code,
  Str,
  Chr,
  Line,
  Block
};

Lex openFromCode(char c, char n)
{
  if (c == '"')
    return Lex::Str;
  if (c == '\'')
    return Lex::Chr;
  if (c == '/' && n == '/')
    return Lex::Line;
  if (c == '/' && n == '*')
    return Lex::Block;
  return Lex::Code;
}

// True if the char at `i` ends state `st`; a two-char close ('*/') advances `i` past it.
bool closesState(Lex st, char c, char n, std::size_t &i)
{
  switch (st)
  {
  case Lex::Str:
    return c == '"';
  case Lex::Chr:
    return c == '\'';
  case Lex::Line:
    return c == '\n';
  case Lex::Block:
    if (c != '*' || n != '/')
      return false;
    ++i;
    return true;
  default:
    return false;
  }
}

// Copy of `text` with every '{' / '}' inside a string/char literal or a line/block comment
// blanked to a space. Length, newlines and all other bytes are preserved, so offsets and
// line numbers map 1:1 onto the original. #136: a brace in a literal (`ss << "x={ ";`) must
// not corrupt brace matching — neither the body scan nor the depth tracking.
std::string neutralizeBraces(const std::string &text)
{
  std::string out = text;
  Lex st = Lex::Code;
  for (std::size_t i = 0; i < out.size(); ++i)
  {
    const char c = out[i];
    const char n = i + 1 < out.size() ? out[i + 1] : '\0';
    if (st == Lex::Code)
      st = openFromCode(c, n);
    else if ((st == Lex::Str || st == Lex::Chr) && c == '\\')
      ++i; // escaped char: skip the next byte
    else if (closesState(st, c, n, i))
      st = Lex::Code;
    else if (c == '{' || c == '}')
      out[i] = ' ';
  }
  return out;
}

int newlinesBefore(const std::string &text, std::size_t end)
{
  return static_cast<int>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(end), '\n'));
}

int braceDelta(const std::string &neut, std::size_t from, std::size_t to)
{
  const auto b = neut.begin();
  return static_cast<int>(std::count(b + static_cast<std::ptrdiff_t>(from), b + static_cast<std::ptrdiff_t>(to), '{')) -
         static_cast<int>(std::count(b + static_cast<std::ptrdiff_t>(from), b + static_cast<std::ptrdiff_t>(to), '}'));
}

// Char-level '{'..'}' match from `start` over BRACE-NEUTRALIZED text; returns [bodyStart,
// bodyEnd) or an npos pair.
std::pair<std::size_t, std::size_t> findBody(const std::string &neut, std::size_t start)
{
  int depth = 0;
  std::size_t bodyStart = 0;
  bool inBody = false;
  for (std::size_t i = start; i < neut.size(); ++i)
  {
    if (neut[i] == '{')
    {
      if (!inBody)
      {
        inBody = true;
        bodyStart = i + 1;
      }
      ++depth;
    }
    else if (neut[i] == '}' && --depth == 0 && inBody)
    {
      return {bodyStart, i};
    }
  }
  return {std::string::npos, std::string::npos};
}

// Append depth-0 bool members of the struct whose body is [body.first, body.second) to
// `out`. Field text comes from `orig`; brace depth from `neut` (same offsets) so literals
// never shift depth.
void appendStructBools(const std::string &orig, const std::string &neut, const std::string &name,
                       std::pair<std::size_t, std::size_t> body, std::vector<BoolField> &out)
{
  const int baseLine = newlinesBefore(orig, body.first) + 1;
  int depth = 0;
  int off = 0;
  for (std::size_t pos = body.first; pos < body.second; ++off)
  {
    const std::size_t nl = orig.find('\n', pos);
    const std::size_t lineEnd = (nl == std::string::npos || nl > body.second) ? body.second : nl;
    const std::string line = orig.substr(pos, lineEnd - pos);
    // The paren guard skips method declarations, but must ignore parens inside a trailing
    // comment (#164 A.3: `bool x = false; // TODO(Phase4)` was invisible to the baseline).
    const std::string code = line.substr(0, line.find("//"));
    std::smatch m;
    if (depth == 0 && code.find('(') == std::string::npos && code.find(')') == std::string::npos &&
        std::regex_match(line, m, kBoolRe))
      out.push_back({name, m[1].str(), baseLine + off});
    depth += braceDelta(neut, pos, lineEnd);
    if (depth < 0)
      depth = 0;
    pos = lineEnd + 1;
  }
}

std::vector<BoolField> boolFields(const std::string &text)
{
  std::vector<BoolField> out;
  const std::string neut = neutralizeBraces(text);
  for (auto it = std::sregex_iterator(text.begin(), text.end(), kStructRe); it != std::sregex_iterator(); ++it)
  {
    const auto body = findBody(neut, static_cast<std::size_t>(it->position()));
    if (body.first != std::string::npos)
      appendStructBools(text, neut, (*it)[1].str(), body, out);
  }
  return out;
}

// Names of every struct/class DEFINED in `text` (existence test, independent of whether
// it has bool fields — a struct that had 0 bools and gains some is still drift, #135).
std::set<std::string> structNames(const std::string &text)
{
  std::set<std::string> names;
  for (auto it = std::sregex_iterator(text.begin(), text.end(), kStructRe); it != std::sregex_iterator(); ++it)
    names.insert((*it)[1].str());
  return names;
}

// Emit one finding when struct `name` has MORE bool fields than the baseline. Net count
// (rename / reformat keep the count); the message lists the gained names (new \ old).
void pushIfAccreted(const std::string &name, const std::vector<BoolField> &fields,
                    const std::set<std::string> &oldNames, const std::string &file, rules::ViolationList &out)
{
  std::set<std::string> newNames;
  for (const auto &f : fields)
    newNames.insert(f.fieldName);
  if (newNames.size() <= oldNames.size())
    return;
  int line = 0;
  int gainedCount = 0;
  std::string gained;
  std::set<std::string> seen;
  for (const auto &f : fields)
  {
    if (oldNames.count(f.fieldName) != 0 || !seen.insert(f.fieldName).second)
      continue;
    line = line == 0 ? f.line : std::min(line, f.line);
    ++gainedCount; // gross adds, matching the listed names (#164 A.4; the gate stays net)
    gained += (gained.empty() ? "" : ", ") + f.fieldName;
  }
  out.push_back({kRuleId, file, line,
                 "struct '" + name + "' accreted " + std::to_string(gainedCount) + " bool field(s): " + gained});
}

} // namespace

BoolFieldDriftResult compareBoolFields(const std::string &oldSource, const std::string &newSource,
                                       const std::string &file)
{
  BoolFieldDriftResult result;
  const std::set<std::string> oldStructs = structNames(oldSource);
  std::map<std::string, std::set<std::string>> oldBools;
  for (const auto &f : boolFields(oldSource))
    oldBools[f.structName].insert(f.fieldName);
  std::map<std::string, std::vector<BoolField>> newByStruct;
  for (const auto &f : boolFields(newSource))
    newByStruct[f.structName].push_back(f);

  static const std::set<std::string> kNone;
  for (const auto &[name, fields] : newByStruct)
  {
    if (oldStructs.count(name) == 0) // struct absent from baseline -> greenfield, not drift
      continue;
    const auto oldIt = oldBools.find(name);
    pushIfAccreted(name, fields, oldIt == oldBools.end() ? kNone : oldIt->second, file, result.violations);
  }
  return result;
}

BoolFieldDriftResult detectBoolFieldDrift(const SourceSnapshot &oldSnapshot, const SourceSnapshot &newSnapshot,
                                          const std::vector<std::filesystem::path> &changedFiles)
{
  BoolFieldDriftResult result;
  static const std::string kEmpty;
  for (const auto &p : changedFiles)
  {
    const std::string path = p.generic_string();
    const SnapshotFile *newFile = newSnapshot.findFile(path);
    const SnapshotFile *oldFile = oldSnapshot.findFile(path);
    const bool authored = newFile != nullptr ? newFile->authored : (oldFile != nullptr && oldFile->authored);
    if (!authored)
      continue;
    auto one = compareBoolFields(oldFile != nullptr ? oldFile->content : kEmpty,
                                 newFile != nullptr ? newFile->content : kEmpty, path);
    for (auto &v : one.violations)
      result.violations.push_back(std::move(v));
  }
  return result;
}

} // namespace archcheck::scan
