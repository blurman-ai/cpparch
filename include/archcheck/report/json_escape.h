#pragma once

#include <string>

namespace archcheck::report
{

namespace detail
{

// Hex digits for \uXXXX encoding.
static constexpr char kHex[] = "0123456789abcdef";
// UTF-8 replacement character U+FFFD: EF BF BD.
static constexpr unsigned char kUtf8Fffd[] = {0xEF, 0xBF, 0xBD};

inline void appendControl(std::string &out, unsigned char b)
{
  if (b == '"')
    out += "\\\"";
  else if (b == '\\')
    out += "\\\\";
  else if (b == '\n')
    out += "\\n";
  else if (b == '\r')
    out += "\\r";
  else if (b == '\t')
    out += "\\t";
  else if (b == '\b')
    out += "\\b";
  else if (b == '\f')
    out += "\\f";
  else
  {
    out += "\\u00";
    out += kHex[(b >> 4) & 0xF];
    out += kHex[b & 0xF];
  }
}

// Returns the expected number of continuation bytes for a UTF-8 lead byte,
// or 0 if it is not a valid lead byte.
inline int utf8ExtraBytes(unsigned char b)
{
  if ((b & 0xE0) == 0xC0 && b >= 0xC2)
    return 1;
  if ((b & 0xF0) == 0xE0)
    return 2;
  if ((b & 0xF8) == 0xF0 && b <= 0xF4)
    return 3;
  return 0;
}

// Validates the next UTF-8 sequence starting at *p (lead byte already consumed),
// advances p past valid continuation bytes, and appends the sequence to out.
// On invalid sequence emits U+FFFD and leaves p unchanged.
inline void appendUtf8(std::string &out, const unsigned char *lead, const unsigned char *&p, const unsigned char *end)
{
  const int extra = utf8ExtraBytes(*lead);
  if (extra == 0)
  {
    out += static_cast<char>(kUtf8Fffd[0]);
    out += static_cast<char>(kUtf8Fffd[1]);
    out += static_cast<char>(kUtf8Fffd[2]);
    return;
  }
  const unsigned char *start = p;
  for (int i = 0; i < extra; ++i)
  {
    if (p >= end || (*p & 0xC0) != 0x80)
    {
      out += static_cast<char>(kUtf8Fffd[0]);
      out += static_cast<char>(kUtf8Fffd[1]);
      out += static_cast<char>(kUtf8Fffd[2]);
      p = start; // do not advance past invalid bytes
      return;
    }
    ++p;
  }
  // Valid sequence: copy original bytes.
  out += static_cast<char>(*lead);
  for (const unsigned char *q = start; q < p; ++q)
  {
    out += static_cast<char>(*q);
  }
}

} // namespace detail

// RFC 8259-compliant JSON string escaping.
// All control characters U+0000..U+001F are escaped; well-known ones use
// short forms (\n \r \t \b \f), the rest use \uXXXX.  Invalid UTF-8 bytes
// are replaced with U+FFFD so the output is always valid JSON.
inline std::string jsonEscape(const std::string &s)
{
  std::string out;
  out.reserve(s.size() + 8);
  const auto *p = reinterpret_cast<const unsigned char *>(s.data());
  const auto *end = p + s.size();
  while (p < end)
  {
    const unsigned char b = *p++;
    if (b < 0x20 || b == '"' || b == '\\')
      detail::appendControl(out, b);
    else if (b < 0x80)
      out += static_cast<char>(b);
    else
      detail::appendUtf8(out, &b, p, end);
  }
  return out;
}

} // namespace archcheck::report
