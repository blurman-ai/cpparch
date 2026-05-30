#pragma once

#include <filesystem>
#include <optional>
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

// Walks up from `start` to the filesystem root and returns the first
// .archcheck.yml found, or nullopt if none exists up the tree.
std::optional<std::filesystem::path> findConfig(const std::filesystem::path &start);

} // namespace archcheck::config
