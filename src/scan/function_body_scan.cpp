#include "archcheck/scan/function_body_scan.h"

#include <algorithm>
#include <array>

namespace archcheck::scan
{
namespace
{

using duplication::Token;

constexpr std::size_t kNpos = static_cast<std::size_t>(-1);

std::string spellingOf(const Token &t) { return t.raw.empty() ? t.sym : t.raw; }

std::size_t findMatching(const std::vector<Token> &toks, std::size_t open, const char *openSym, const char *closeSym)
{
  int depth = 0;
  for (std::size_t k = open; k < toks.size(); ++k)
  {
    if (toks[k].sym == openSym)
      ++depth;
    else if (toks[k].sym == closeSym && --depth == 0)
      return k;
  }
  return kNpos;
}

struct NameInfo
{
  std::string name;
  int line = 0;
};

// Walk the `id (:: id)*` chain leftwards; `~Name` keeps the tilde.
NameInfo extractIdentifierChain(const std::vector<Token> &toks, std::size_t k)
{
  NameInfo info{spellingOf(toks[k]), toks[k].line};
  if (k >= 1 && toks[k - 1].sym == "~")
  {
    info.name = "~" + info.name;
    --k;
  }
  while (k >= 2 && toks[k - 1].sym == "::" && toks[k - 2].sym == "id")
  {
    info.name = spellingOf(toks[k - 2]) + "::" + info.name;
    k -= 2;
  }
  return info;
}

// Name of the candidate whose parameter list opens at toks[parenIdx].
// Empty name => not a function candidate (control header, lambda, expression).
NameInfo extractName(const std::vector<Token> &toks, std::size_t parenIdx)
{
  if (parenIdx == 0)
    return {};
  const std::size_t k = parenIdx - 1;
  // cppcheck-suppress negativeContainerIndex
  if (toks[k].sym == "id")
    return extractIdentifierChain(toks, k);
  // `operator@ (` — one- or two-token operator spelling after the keyword.
  if (k >= 1 && toks[k - 1].sym == "operator")
    return {"operator" + spellingOf(toks[k]), toks[k - 1].line};
  if (k >= 2 && toks[k - 2].sym == "operator")
    return {"operator" + spellingOf(toks[k - 1]) + spellingOf(toks[k]), toks[k - 2].line};
  return {};
}

int bracketDelta(const std::string &s)
{
  if (s == "(" || s == "[" || s == "{")
    return 1;
  if (s == ")" || s == "]" || s == "}")
    return -1;
  return 0;
}

// Top-level parameter count: commas at paren depth 0 outside template angles.
int countArity(const std::vector<Token> &toks, std::size_t open, std::size_t close)
{
  if (close == open + 1 || (close == open + 2 && toks[open + 1].sym == "void"))
    return 0;
  int depth = 0, angle = 0, commas = 0;
  for (std::size_t k = open + 1; k < close; ++k)
  {
    const std::string &s = toks[k].sym;
    const int d = bracketDelta(s);
    depth += d;
    if (d != 0 || depth != 0)
      continue;
    if (s == "<")
      ++angle;
    else if (s == ">" || s == ">>")
      angle = std::max(0, angle - (s == ">>" ? 2 : 1));
    else if (angle == 0 && s == ",")
      ++commas;
  }
  return commas + 1;
}

// Constructor init-list: skip `: member(init), member{init}, ...` up to the
// body brace. An initializer group's bracket follows an id or `>`.
std::size_t skipInitList(const std::vector<Token> &toks, std::size_t k)
{
  while (k < toks.size())
  {
    const std::string &s = toks[k].sym;
    const bool afterName = k > 0 && (toks[k - 1].sym == "id" || toks[k - 1].sym == ">");
    if ((s == "(" || s == "{") && afterName)
    {
      k = findMatching(toks, k, s == "(" ? "(" : "{", s == "(" ? ")" : "}");
      if (k == kNpos)
        return kNpos;
    }
    else if (s == "{")
      return k;
    else if (s == ";")
      return kNpos;
    ++k;
  }
  return kNpos;
}

// Trailing return type: skip tokens after `->` until the body brace.
std::size_t skipTrailingReturn(const std::vector<Token> &toks, std::size_t k)
{
  while (k < toks.size())
  {
    const std::string &s = toks[k].sym;
    if (s == "{")
      return k;
    if (s == ";" || s == "=" || s == "}")
      return kNpos;
    if (s == "(")
    {
      k = findMatching(toks, k, "(", ")");
      if (k == kNpos)
        return kNpos;
    }
    ++k;
  }
  return kNpos;
}

bool isPlainQualifier(const std::string &s)
{
  static constexpr std::array<const char *, 7> kQualifiers = {"const", "noexcept", "override", "final",
                                                              "&",     "&&",       "mutable"};
  return std::find(kQualifiers.begin(), kQualifiers.end(), s) != kQualifiers.end();
}

// After the parameter list's `)`: accept qualifiers / trailing return /
// ctor init-list, and land on the body `{`. kNpos => declaration or call.
std::size_t findBodyBrace(const std::vector<Token> &toks, std::size_t closeParen)
{
  std::size_t k = closeParen + 1;
  while (k < toks.size())
  {
    const std::string &s = toks[k].sym;
    if (s == "{")
      return k;
    if (s == "->")
      return skipTrailingReturn(toks, k + 1);
    if (s == ":")
      return skipInitList(toks, k + 1);
    if ((s == "noexcept" || s == "throw") && k + 1 < toks.size() && toks[k + 1].sym == "(")
    {
      k = findMatching(toks, k + 1, "(", ")");
      if (k == kNpos)
        return kNpos;
    }
    else if (!isPlainQualifier(s))
      return kNpos;
    ++k;
  }
  return kNpos;
}

struct Candidate
{
  NameInfo name;
  std::size_t paramOpen = 0;
};

// Candidate at the '(' token: `operator()` has its name parens first, the
// parameter list follows; everything else resolves leftwards from the '('.
Candidate candidateAt(const std::vector<Token> &tokens, std::size_t i)
{
  if (i >= 1 && tokens[i - 1].sym == "operator" && i + 2 < tokens.size() && tokens[i + 1].sym == ")" &&
      tokens[i + 2].sym == "(")
    return {{"operator()", tokens[i - 1].line}, i + 2};
  return {extractName(tokens, i), i};
}

// Appends a span when the candidate at the '(' token is a definition.
// Returns the index to resume scanning from.
std::size_t scanCandidate(const std::vector<Token> &tokens, std::size_t i, std::vector<FunctionSpan> &out)
{
  const Candidate cand = candidateAt(tokens, i);
  const std::size_t paramClose = cand.name.name.empty() ? kNpos : findMatching(tokens, cand.paramOpen, "(", ")");
  if (paramClose == kNpos)
    return i + 1;
  const std::size_t bodyOpen = findBodyBrace(tokens, paramClose);
  const std::size_t bodyClose = bodyOpen == kNpos ? kNpos : findMatching(tokens, bodyOpen, "{", "}");
  if (bodyClose == kNpos)
    return paramClose + 1;
  out.push_back({cand.name.name, countArity(tokens, cand.paramOpen, paramClose), cand.name.line, tokens[bodyClose].line,
                 bodyOpen, bodyClose});
  return bodyClose + 1;
}

} // namespace

std::vector<FunctionSpan> discoverFunctions(const std::vector<Token> &tokens)
{
  std::vector<FunctionSpan> out;
  std::size_t i = 0;
  while (i < tokens.size())
  {
    if (tokens[i].sym == "(")
      i = scanCandidate(tokens, i, out);
    else
      ++i;
  }
  return out;
}

} // namespace archcheck::scan
