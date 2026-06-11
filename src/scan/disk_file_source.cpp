#include "archcheck/scan/disk_file_source.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>

namespace archcheck::scan
{

// S4: skip files larger than this to prevent OOM on adversarial inputs.
static constexpr std::size_t kMaxFileSizeBytes = 64ULL * 1024 * 1024; // 64 MiB

DiskFileSource::DiskFileSource(std::filesystem::path root) : root_(std::move(root)) {}

std::vector<ProjectFile> DiskFileSource::list() { return discoverFiles(root_); }

std::string DiskFileSource::read(const std::string &repoRelativePath)
{
  const auto fullPath = root_ / repoRelativePath;
  std::error_code ec;
  const auto sz = std::filesystem::file_size(fullPath, ec);
  if (!ec && sz > kMaxFileSizeBytes)
  {
    std::cerr << "archcheck: skipping oversized file (> 64 MiB): " << repoRelativePath << '\n';
    return {};
  }
  std::ifstream f(fullPath, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace archcheck::scan
