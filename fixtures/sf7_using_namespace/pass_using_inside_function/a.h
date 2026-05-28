#pragma once
#include <string>

template <> struct StringMaker
{
  static std::string convert(bool b)
  {
    using namespace std::string_literals;
    return b ? "true"s : "false"s;
  }
};
