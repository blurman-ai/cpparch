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

bool isValidZeroTerminator(char d) { return d == '\n' || d == '\r' || d == ' ' || d == '\t' || d == '/'; }

bool isDeadIfOpener(const std::string &src, std::size_t i)
{
  const std::size_t n = src.size();
  std::size_t j = i + 1;
  while (j < n && (src[j] == ' ' || src[j] == '\t'))
    ++j;
  if (src.compare(j, 2, "if") != 0)
    return false;
  j += 2;
  if (j < n && isIdentChar(src[j]))
    return false;
  while (j < n && (src[j] == ' ' || src[j] == '\t'))
    ++j;
  if (src.compare(j, 5, "false") == 0)
    return true;
  if (j >= n || src[j] != '0')
    return false;
  return isValidZeroTerminator(j + 1 < n ? src[j + 1] : '\n');
}

namespace
{
int updatePreprocessorDepth(const std::string &src, std::size_t i)
{
  const std::size_t n = src.size();
  if (src[i] != '#')
  {
    return 0;
  }
  std::size_t j = i + 1;
  while (j < n && (src[j] == ' ' || src[j] == '\t'))
  {
    ++j;
  }
  if (src.compare(j, 2, "if") == 0)
  {
    return 1;
  }
  if (src.compare(j, 5, "endif") == 0)
  {
    return -1;
  }
  return 0;
}
} // namespace

void skipDeadIfBlock(const std::string &src, std::size_t &i, int &line)
{
  const std::size_t n = src.size();
  int depth = 0;
  while (i < n)
  {
    depth += updatePreprocessorDepth(src, i);
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

namespace
{
bool tryConsumeLine(const std::string &source, std::size_t &i, int &line)
{
  if (i < source.size() && source[i] == '\n')
  {
    ++line;
    ++i;
    return true;
  }
  return false;
}

bool tryConsumeWhitespace(const std::string &source, std::size_t &i)
{
  if (i < source.size() && std::isspace(static_cast<unsigned char>(source[i])) != 0)
  {
    ++i;
    return true;
  }
  return false;
}

bool tryConsumeLineComment(const std::string &source, std::size_t &i)
{
  if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/')
  {
    while (i < source.size() && source[i] != '\n')
    {
      ++i;
    }
    return true;
  }
  return false;
}

bool tryConsumeBlockComment(const std::string &source, std::size_t &i, int &line)
{
  if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*')
  {
    i += 2;
    while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/'))
    {
      if (source[i] == '\n')
      {
        ++line;
      }
      ++i;
    }
    if (i + 1 < source.size())
    {
      i += 2;
    }
    return true;
  }
  return false;
}

bool tryConsumeRawString(const std::string &source, std::size_t &i, int &line, std::vector<Token> &out)
{
  const std::size_t pre = rawStringPrefixLen(source, i);
  if (pre == 0)
  {
    return false;
  }
  const std::size_t litStart = i;
  const int startLine = line;
  i += pre;
  while (i + 1 < source.size() && !(source[i] == ')' && source[i + 1] == '"'))
  {
    if (source[i] == '\n')
    {
      ++line;
    }
    ++i;
  }
  if (i + 1 < source.size())
  {
    i += 2;
  }
  out.push_back({"lit", startLine, source.substr(litStart, i - litStart)});
  return true;
}

bool tryConsumeString(const std::string &source, std::size_t &i, int &line, std::vector<Token> &out)
{
  if (i >= source.size() || source[i] != '"')
  {
    return false;
  }
  const std::size_t litStart = i;
  const int startLine = line;
  ++i;
  while (i < source.size() && source[i] != '"')
  {
    if (source[i] == '\\' && i + 1 < source.size())
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
  if (i < source.size())
  {
    ++i;
  }
  out.push_back({"lit", startLine, source.substr(litStart, i - litStart)});
  return true;
}

bool tryConsumeChar(const std::string &source, std::size_t &i, int &line, std::vector<Token> &out)
{
  if (i >= source.size() || source[i] != '\'')
  {
    return false;
  }
  const std::size_t litStart = i;
  ++i;
  while (i < source.size() && source[i] != '\'')
  {
    if (source[i] == '\\' && i + 1 < source.size())
    {
      i += 2;
      continue;
    }
    ++i;
  }
  if (i < source.size())
  {
    ++i;
  }
  out.push_back({"lit", line, source.substr(litStart, i - litStart)});
  return true;
}

bool isExponentSign(const std::string &src, std::size_t i)
{
  if (i == 0)
    return false;
  const char d = src[i];
  const char prev = src[i - 1];
  return (d == '+' || d == '-') && (prev == 'e' || prev == 'E' || prev == 'p' || prev == 'P');
}

bool tryConsumeNumber(const std::string &source, std::size_t &i, int &line, std::vector<Token> &out)
{
  const char c = source[i];
  const bool isDigit = std::isdigit(static_cast<unsigned char>(c)) != 0;
  const bool isDotNumber =
      c == '.' && i + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[i + 1])) != 0;
  if (!isDigit && !isDotNumber)
    return false;
  const std::size_t litStart = i++;
  while (i < source.size())
  {
    const char d = source[i];
    if (isIdentChar(d) || d == '.' || d == '\'')
      ++i;
    else if (isExponentSign(source, i))
      ++i;
    else
      break;
  }
  out.push_back({"lit", line, source.substr(litStart, i - litStart)});
  return true;
}

void pushIdentifierToken(const std::string &word, int line, bool nextIsParen, bool keepCalls, std::vector<Token> &out)
{
  if (keywords().count(word) != 0)
  {
    out.push_back({word, line, ""});
    return;
  }
  if (!keepCalls)
  {
    out.push_back({"id", line, word});
    return;
  }
  const bool keep = nextIsParen || (!out.empty() && out.back().sym == "case");
  out.push_back({keep ? word : "id", line, keep ? "" : word});
}

bool tryConsumeIdentifier(const std::string &source, std::size_t &i, int line, std::vector<Token> &out, bool keepCalls)
{
  if (!isIdentStart(source[i]))
    return false;
  const std::size_t start = i;
  while (i < source.size() && isIdentChar(source[i]))
    ++i;
  std::size_t j = i;
  while (j < source.size() && (source[j] == ' ' || source[j] == '\t' || source[j] == '\n' || source[j] == '\r'))
    ++j;
  const bool nextIsParen = j < source.size() && source[j] == '(';
  pushIdentifierToken(source.substr(start, i - start), line, nextIsParen, keepCalls, out);
  return true;
}

bool tryConsumeOperator(const std::string &source, std::size_t &i, int line, std::vector<Token> &out)
{
  for (const std::string &op : multiOps())
  {
    if (source.compare(i, op.size(), op) == 0)
    {
      out.push_back({op, line, std::string()});
      i += op.size();
      return true;
    }
  }
  return false;
}

void consumeToken(const std::string &source, std::size_t &i, int line, std::vector<Token> &out, bool keepCalls)
{
  if (tryConsumeRawString(source, i, line, out))
  {
    return;
  }
  if (tryConsumeString(source, i, line, out))
  {
    return;
  }
  if (tryConsumeChar(source, i, line, out))
  {
    return;
  }
  if (tryConsumeNumber(source, i, line, out))
  {
    return;
  }
  if (tryConsumeIdentifier(source, i, line, out, keepCalls))
  {
    return;
  }
  if (tryConsumeOperator(source, i, line, out))
  {
    return;
  }
  out.push_back({std::string(1, source[i]), line, std::string()}); // fallback: single char
  ++i;
}

bool consumeTrivia(const std::string &source, std::size_t &i, int &line, bool &atLineStart)
{
  if (tryConsumeLine(source, i, line))
  {
    atLineStart = true;
    return true;
  }
  if (tryConsumeWhitespace(source, i))
  {
    return true;
  }
  if (tryConsumeLineComment(source, i))
  {
    return true;
  }
  if (tryConsumeBlockComment(source, i, line))
  {
    return true;
  }
  if (atLineStart && source[i] == '#' && isDeadIfOpener(source, i))
  {
    skipDeadIfBlock(source, i, line);
    atLineStart = true;
    return true;
  }
  return false;
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
    if (consumeTrivia(source, i, line, atLineStart))
    {
      continue;
    }
    atLineStart = false;
    const std::size_t tokenStart = i;
    consumeToken(source, i, line, out, keepCalls);
    out.back().off = static_cast<int>(tokenStart);
  }

  return out;
}

} // namespace archcheck::scan::duplication
