#include "archcheck/scan/include_scanner.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace archcheck::scan
{

namespace
{

constexpr std::string_view kIncludeKeyword = "#include";

struct Joined
{
  std::string text;
  std::vector<int> line_of_offset;
};

Joined join_continuations(std::string_view source)
{
  Joined j;
  j.text.reserve(source.size());
  j.line_of_offset.reserve(source.size());
  int line = 1;
  for (std::size_t i = 0; i < source.size(); ++i)
  {
    if (source[i] == '\\' && i + 1 < source.size() && source[i + 1] == '\n')
    {
      ++line;
      ++i;
      continue;
    }
    j.text.push_back(source[i]);
    j.line_of_offset.push_back(line);
    if (source[i] == '\n')
    {
      ++line;
    }
  }
  return j;
}

std::size_t skip_ws(std::string_view line, std::size_t i)
{
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
  {
    ++i;
  }
  return i;
}

std::size_t consume_line_comment(std::string_view src, std::size_t i, std::string &out)
{
  while (i < src.size() && src[i] != '\n')
  {
    out.push_back(' ');
    ++i;
  }
  return i;
}

std::size_t consume_block_comment(std::string_view src, std::size_t i, std::string &out)
{
  out.append("  ");
  i += 2;
  while (i < src.size())
  {
    if (i + 1 < src.size() && src[i] == '*' && src[i + 1] == '/')
    {
      out.append("  ");
      return i + 2;
    }
    out.push_back(src[i] == '\n' ? '\n' : ' ');
    ++i;
  }
  return i;
}

std::size_t consume_raw_string(std::string_view src, std::size_t i, std::string &out)
{
  out.push_back(' ');
  ++i;
  std::string delim;
  while (i < src.size() && src[i] != '(' && src[i] != '\n')
  {
    delim.push_back(src[i]);
    out.push_back(' ');
    ++i;
  }
  if (i >= src.size() || src[i] != '(')
  {
    return i;
  }
  out.push_back(' ');
  ++i;
  const std::string close = ")" + delim + "\"";
  while (i < src.size())
  {
    if (src.compare(i, close.size(), close) == 0)
    {
      out.append(close.size(), ' ');
      return i + close.size();
    }
    out.push_back(src[i] == '\n' ? '\n' : ' ');
    ++i;
  }
  return i;
}

std::string preprocess(std::string_view source)
{
  std::string out;
  out.reserve(source.size());
  for (std::size_t i = 0; i < source.size();)
  {
    const bool two = i + 1 < source.size();
    if (two && source[i] == '/' && source[i + 1] == '/')
    {
      i = consume_line_comment(source, i, out);
      continue;
    }
    if (two && source[i] == '/' && source[i + 1] == '*')
    {
      i = consume_block_comment(source, i, out);
      continue;
    }
    if (source[i] == '"' && i >= 1 && source[i - 1] == 'R')
    {
      i = consume_raw_string(source, i, out);
      continue;
    }
    out.push_back(source[i]);
    ++i;
  }
  return out;
}

bool is_ident_start(char c) { return (c == '_') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

bool is_ident_cont(char c) { return is_ident_start(c) || (c >= '0' && c <= '9'); }

bool is_pp_keyword(std::string_view line, std::string_view kw)
{
  std::size_t i = skip_ws(line, 0);
  if (i >= line.size() || line[i] != '#')
    return false;
  i = skip_ws(line, i + 1);
  if (line.compare(i, kw.size(), kw) != 0)
    return false;
  const std::size_t after = i + kw.size();
  return after >= line.size() || !is_ident_cont(line[after]);
}

bool opens_conditional(std::string_view line)
{
  return is_pp_keyword(line, "if") || is_pp_keyword(line, "ifdef") || is_pp_keyword(line, "ifndef");
}

bool closes_conditional(std::string_view line) { return is_pp_keyword(line, "endif"); }

std::string_view pp_argument(std::string_view line, std::string_view kw)
{
  std::size_t i = skip_ws(line, 0);
  ++i; // '#'
  i = skip_ws(line, i);
  i += kw.size();
  i = skip_ws(line, i);
  std::size_t e = i;
  while (e < line.size() && is_ident_cont(line[e]))
    ++e;
  return line.substr(i, e - i);
}

bool is_shouty_ident(std::string_view s)
{
  if (s.empty())
    return false;
  if (!(s[0] == '_' || (s[0] >= 'A' && s[0] <= 'Z')))
    return false;
  for (char c : s)
  {
    if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
      return false;
  }
  return true;
}

bool is_blank_line(std::string_view line) { return skip_ws(line, 0) >= line.size(); }

// Returns the next non-blank line starting at *cursor; advances *cursor past it.
// Sets line_offset to the starting offset of that line. Returns empty view at EOF.
std::string_view next_significant_line(std::string_view view, std::size_t &cursor, std::size_t &line_offset)
{
  while (cursor <= view.size())
  {
    const std::size_t start = cursor;
    while (cursor < view.size() && view[cursor] != '\n')
      ++cursor;
    const std::string_view ln = view.substr(start, cursor - start);
    if (cursor < view.size())
      ++cursor; // skip '\n'
    if (!is_blank_line(ln))
    {
      line_offset = start;
      return ln;
    }
    if (start == view.size())
      break;
  }
  return {};
}

// Detects a classical include-guard:
//   first non-blank directive is `#ifndef X`
//   immediately followed (after blanks/comments) by `#define X`
// Returns offset of the `#ifndef` line in `view`, or npos if not a guard.
// Comments are already replaced with whitespace by preprocess().
std::size_t detect_include_guard_offset(std::string_view view)
{
  std::size_t cursor = 0;
  std::size_t ifndef_offset = 0;
  const std::string_view first = next_significant_line(view, cursor, ifndef_offset);
  if (first.empty() || !is_pp_keyword(first, "ifndef"))
    return std::string_view::npos;
  const auto ident = pp_argument(first, "ifndef");
  if (!is_shouty_ident(ident))
    return std::string_view::npos;
  std::size_t define_offset = 0;
  const std::string_view second = next_significant_line(view, cursor, define_offset);
  if (second.empty() || !is_pp_keyword(second, "define"))
    return std::string_view::npos;
  if (pp_argument(second, "define") != ident)
    return std::string_view::npos;
  return ifndef_offset;
}

void emit_directive(std::string_view line, int line_no, std::size_t i, bool conditional, ScanResult &out)
{
  const char open = line[i];
  const char close = (open == '"') ? '"' : '>';
  const std::size_t start = i + 1;
  const std::size_t end = line.find(close, start);
  if (end == std::string_view::npos)
  {
    return;
  }
  out.directives.push_back(IncludeDirective{
      (open == '"') ? IncludeKind::Quote : IncludeKind::Angle,
      std::string{line.substr(start, end - start)},
      line_no,
      conditional,
  });
}

void emit_macro_include(std::string_view line, int line_no, std::size_t i, ScanResult &out)
{
  std::size_t end = i + 1;
  while (end < line.size() && is_ident_cont(line[end]))
  {
    ++end;
  }
  out.diagnostics.push_back(IncludeScanDiagnostic{
      DiagnosticKind::MacroInclude,
      std::string{line.substr(i, end - i)},
      line_no,
  });
}

void try_extract(std::string_view line, int line_no, bool conditional, ScanResult &out)
{
  std::size_t i = skip_ws(line, 0);
  if (line.compare(i, kIncludeKeyword.size(), kIncludeKeyword) != 0)
  {
    return;
  }
  i = skip_ws(line, i + kIncludeKeyword.size());
  if (i >= line.size())
  {
    return;
  }
  const char c = line[i];
  if (c == '"' || c == '<')
  {
    emit_directive(line, line_no, i, conditional, out);
  }
  else if (is_ident_start(c))
  {
    emit_macro_include(line, line_no, i, out);
  }
}

int line_at(const Joined &joined, std::size_t offset)
{
  if (joined.line_of_offset.empty())
  {
    return 1;
  }
  const std::size_t last = joined.line_of_offset.size() - 1;
  return joined.line_of_offset[std::min(offset, last)];
}

} // namespace

ScanResult scanIncludes(std::string_view source)
{
  constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
  // GCC8-COMPAT: libstdc++ 8 lacks std::string_view::starts_with; use compare().
  if (source.size() >= kUtf8Bom.size() && source.compare(0, kUtf8Bom.size(), kUtf8Bom) == 0)
    source.remove_prefix(kUtf8Bom.size());
  const Joined joined = join_continuations(source);
  const std::string cleaned = preprocess(joined.text);
  const std::string_view view{cleaned};
  ScanResult out;
  const std::size_t guard_offset = detect_include_guard_offset(view);
  std::size_t line_start = 0;
  int depth = 0;
  for (std::size_t i = 0; i <= view.size(); ++i)
  {
    if (i == view.size() || view[i] == '\n')
    {
      const std::string_view ln = view.substr(line_start, i - line_start);
      const bool is_guard_open = (line_start == guard_offset);
      if (opens_conditional(ln) && !is_guard_open)
        ++depth;
      else if (closes_conditional(ln) && depth > 0)
        --depth;
      try_extract(ln, line_at(joined, line_start), depth > 0, out);
      line_start = i + 1;
    }
  }
  return out;
}

} // namespace archcheck::scan
