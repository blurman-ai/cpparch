#include "archcheck/scan/duplication/token_normalizer.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace archcheck::scan::duplication
{

namespace
{

const std::unordered_set<std::string> &keywords()
{
  static const std::unordered_set<std::string> kw = {
      "alignas",     "alignof",   "and",        "and_eq",    "asm",      "auto",         "bitand",
      "bitor",       "bool",      "break",      "case",      "catch",    "char",         "char8_t",
      "char16_t",    "char32_t",  "class",      "compl",     "concept",  "const",        "consteval",
      "constexpr",   "constinit", "const_cast", "continue",  "co_await", "co_return",    "co_yield",
      "decltype",    "default",   "delete",     "do",        "double",   "dynamic_cast", "else",
      "enum",        "explicit",  "export",     "extern",    "false",    "float",        "for",
      "friend",      "goto",      "if",         "inline",    "int",      "long",         "mutable",
      "namespace",   "new",       "noexcept",   "not",       "not_eq",   "nullptr",      "operator",
      "or",          "or_eq",     "private",    "protected", "public",   "register",     "reinterpret_cast",
      "requires",    "return",    "short",      "signed",    "sizeof",   "static",       "static_assert",
      "static_cast", "struct",    "switch",     "template",  "this",     "thread_local", "throw",
      "true",        "try",       "typedef",    "typeid",    "typename", "union",        "unsigned",
      "using",       "virtual",   "void",       "volatile",  "wchar_t",  "while",        "xor",
      "xor_eq",      "override",  "final"};
  return kw;
}

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_'; }

bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }

const std::vector<std::string> &multiOps()
{
  static const std::vector<std::string> ops = {"<<=", ">>=", "->*", "...", "<=>", "::", "->", "++", "--",
                                               "<<",  ">>",  "<=",  ">=",  "==",  "!=", "&&", "||", "+=",
                                               "-=",  "*=",  "/=",  "%=",  "&=",  "|=", "^=", ".*"};
  return ops;
}

std::size_t rawStringPrefixLen(const std::string &src, std::size_t i)
{
  const std::size_t n = src.size();
  std::size_t j = i;
  if (src.compare(j, 2, "u8") == 0)
  {
    j += 2;
  }
  else if (j < n && (src[j] == 'L' || src[j] == 'u' || src[j] == 'U'))
  {
    j += 1;
  }
  if (j >= n || src[j] != 'R')
  {
    return 0;
  }
  ++j;
  if (j + 1 < n && src[j] == '"' && src[j + 1] == '(')
  {
    return (j + 2) - i;
  }
  return 0;
}

bool isDeadIfOpener(const std::string &src, std::size_t i)
{
  const std::size_t n = src.size();
  std::size_t j = i + 1; // past '#'
  while (j < n && (src[j] == ' ' || src[j] == '\t'))
  {
    ++j;
  }
  if (src.compare(j, 2, "if") != 0)
  {
    return false;
  }
  j += 2;
  if (j < n && isIdentChar(src[j]))
  {
    return false; // reject #ifdef / #ifndef
  }
  while (j < n && (src[j] == ' ' || src[j] == '\t'))
  {
    ++j;
  }
  if (src.compare(j, 5, "false") == 0)
  {
    return true;
  }
  if (j < n && src[j] == '0')
  {
    const char d = (j + 1 < n) ? src[j + 1] : '\n';
    return d == '\n' || d == '\r' || d == ' ' || d == '\t' || d == '/';
  }
  return false;
}

void skipDeadIfBlock(const std::string &src, std::size_t &i, int &line)
{
  const std::size_t n = src.size();
  int depth = 0;
  while (i < n)
  {
    if (src[i] == '#')
    {
      std::size_t j = i + 1;
      while (j < n && (src[j] == ' ' || src[j] == '\t'))
      {
        ++j;
      }
      if (src.compare(j, 2, "if") == 0)
      {
        ++depth;
      }
      else if (src.compare(j, 5, "endif") == 0)
      {
        --depth;
      }
    }
    while (i < n && src[i] != '\n')
    {
      ++i;
    }
    if (i < n)
    {
      ++i;
      ++line;
    }
    if (depth == 0)
    {
      break;
    }
  }
}

} // namespace

std::vector<Token> lex(const std::string &source, bool keepCalls)
{
  std::vector<Token> out;
  const std::size_t n = source.size();
  std::size_t i = 0;
  int line = 1;
  bool atLineStart = true;

  while (i < n)
  {
    const char c = source[i];

    if (c == '\n')
    {
      ++line;
      ++i;
      atLineStart = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c)) != 0)
    {
      ++i;
      continue;
    }
    // line comment
    if (c == '/' && i + 1 < n && source[i + 1] == '/')
    {
      while (i < n && source[i] != '\n')
      {
        ++i;
      }
      continue;
    }
    // block comment
    if (c == '/' && i + 1 < n && source[i + 1] == '*')
    {
      i += 2;
      while (i + 1 < n && !(source[i] == '*' && source[i + 1] == '/'))
      {
        if (source[i] == '\n')
        {
          ++line;
        }
        ++i;
      }
      i += 2;
      continue;
    }
    // dead preprocessor block: #if 0 / #if false ... #endif
    if (atLineStart && c == '#' && isDeadIfOpener(source, i))
    {
      skipDeadIfBlock(source, i, line);
      atLineStart = true;
      continue;
    }
    atLineStart = false;
    // raw string literal R"( ... )" — collapse body to "lit"
    if (const std::size_t pre = rawStringPrefixLen(source, i); pre != 0)
    {
      const std::size_t litStart = i;
      const int startLine = line;
      i += pre;
      while (i + 1 < n && !(source[i] == ')' && source[i + 1] == '"'))
      {
        if (source[i] == '\n')
        {
          ++line;
        }
        ++i;
      }
      i += 2;
      out.push_back({"lit", startLine, source.substr(litStart, i - litStart)});
      continue;
    }
    // string literal
    if (c == '"')
    {
      const std::size_t litStart = i;
      const int startLine = line;
      ++i;
      while (i < n && source[i] != '"')
      {
        if (source[i] == '\\' && i + 1 < n)
        {
          i += 2;
          continue;
        }
        if (source[i] == '\n')
        {
          ++line;
        }
        ++i;
      }
      ++i;
      out.push_back({"lit", startLine, source.substr(litStart, i - litStart)});
      continue;
    }
    // char literal
    if (c == '\'')
    {
      const std::size_t litStart = i;
      ++i;
      while (i < n && source[i] != '\'')
      {
        if (source[i] == '\\' && i + 1 < n)
        {
          i += 2;
          continue;
        }
        ++i;
      }
      ++i;
      out.push_back({"lit", line, source.substr(litStart, i - litStart)});
      continue;
    }
    // number
    if (std::isdigit(static_cast<unsigned char>(c)) != 0 ||
        (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(source[i + 1])) != 0))
    {
      const std::size_t litStart = i;
      ++i;
      while (i < n)
      {
        const char d = source[i];
        if (isIdentChar(d) || d == '.' || d == '\'')
        {
          ++i;
        }
        else if ((d == '+' || d == '-') && i > 0 &&
                 (source[i - 1] == 'e' || source[i - 1] == 'E' || source[i - 1] == 'p' || source[i - 1] == 'P'))
        {
          ++i;
        }
        else
        {
          break;
        }
      }
      out.push_back({"lit", line, source.substr(litStart, i - litStart)});
      continue;
    }
    // identifier / keyword
    if (isIdentStart(c))
    {
      const std::size_t start = i;
      while (i < n && isIdentChar(source[i]))
      {
        ++i;
      }
      std::string word = source.substr(start, i - start);
      if (keywords().count(word) != 0)
      {
        out.push_back({word, line, std::string()});
      }
      else if (keepCalls)
      {
        std::size_t j = i;
        while (j < n && (source[j] == ' ' || source[j] == '\t' || source[j] == '\n' || source[j] == '\r'))
        {
          ++j;
        }
        const bool callee = (j < n && source[j] == '(');
        const bool caseLabel = (!out.empty() && out.back().sym == "case");
        const bool keep = callee || caseLabel;
        out.push_back({keep ? word : std::string("id"), line, keep ? std::string() : word});
      }
      else
      {
        out.push_back({std::string("id"), line, word});
      }
      continue;
    }
    // operator / punctuation (longest match)
    bool matched = false;
    for (const std::string &op : multiOps())
    {
      if (source.compare(i, op.size(), op) == 0)
      {
        out.push_back({op, line, std::string()});
        i += op.size();
        matched = true;
        break;
      }
    }
    if (!matched)
    {
      out.push_back({std::string(1, c), line, std::string()});
      ++i;
    }
  }

  return out;
}

} // namespace archcheck::scan::duplication
