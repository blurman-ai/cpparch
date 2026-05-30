#pragma once

#include <string>

namespace archcheck::report
{

// Minimal JSON string escaping (handles the characters that appear in paths/messages):
// the double quote, the backslash, and the newline. Header-only: tiny pure function,
// the only dependency is <string>, so a separate TU would be pure overhead.
inline std::string jsonEscape(const std::string &s)
{
  std::string out;
  out.reserve(s.size());
  for (const char c : s)
  {
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else if (c == '\n')
      out += "\\n";
    else
      out += c;
  }
  return out;
}

} // namespace archcheck::report
