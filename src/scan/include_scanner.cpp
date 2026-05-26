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

std::size_t consume_line_comment(std::string_view src, std::size_t i, std::string& out)
{
   while (i < src.size() && src[i] != '\n')
   {
      out.push_back(' ');
      ++i;
   }
   return i;
}

std::size_t consume_block_comment(std::string_view src, std::size_t i, std::string& out)
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

std::size_t consume_raw_string(std::string_view src, std::size_t i, std::string& out)
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

bool is_ident_start(char c)
{
   return (c == '_') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_ident_cont(char c)
{
   return is_ident_start(c) || (c >= '0' && c <= '9');
}

void emit_directive(std::string_view line, int line_no, std::size_t i, ScanResult& out)
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
   });
}

void emit_macro_include(std::string_view line, int line_no, std::size_t i, ScanResult& out)
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

void try_extract(std::string_view line, int line_no, ScanResult& out)
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
      emit_directive(line, line_no, i, out);
   }
   else if (is_ident_start(c))
   {
      emit_macro_include(line, line_no, i, out);
   }
}

int line_at(const Joined& joined, std::size_t offset)
{
   if (joined.line_of_offset.empty())
   {
      return 1;
   }
   const std::size_t last = joined.line_of_offset.size() - 1;
   return joined.line_of_offset[std::min(offset, last)];
}

} // namespace

ScanResult scan_includes(std::string_view source)
{
   const Joined joined = join_continuations(source);
   const std::string cleaned = preprocess(joined.text);
   const std::string_view view{cleaned};
   ScanResult out;
   std::size_t line_start = 0;
   for (std::size_t i = 0; i <= view.size(); ++i)
   {
      if (i == view.size() || view[i] == '\n')
      {
         try_extract(view.substr(line_start, i - line_start), line_at(joined, line_start), out);
         line_start = i + 1;
      }
   }
   return out;
}

} // namespace archcheck::scan
