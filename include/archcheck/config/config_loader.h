#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "archcheck/config/config.h"

namespace archcheck::config
{

class ConfigError : public std::runtime_error
{
public:
  ConfigError(std::string file, int line, int column, const std::string &message);

  const std::string &file() const noexcept { return file_; }
  int line() const noexcept { return line_; }
  int column() const noexcept { return column_; }

private:
  std::string file_;
  int line_;
  int column_;
};

Config load(const std::filesystem::path &path);

} // namespace archcheck::config
