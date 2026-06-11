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
  ConfigError(const std::string &file, int line, int column, const std::string &message);
};

Config load(const std::filesystem::path &path);

// Walks up from `start` to the filesystem root and returns the first
// .archcheck.yml found, or nullopt if none exists up the tree.
std::optional<std::filesystem::path> findConfig(const std::filesystem::path &start);

// Discovery policy shared by all CLI modes: walkup from the analyzed root
// (not the process CWD), embedded defaults if no file is found.
// Throws ConfigError on a malformed file.
Config discover(const std::filesystem::path &root);

} // namespace archcheck::config
