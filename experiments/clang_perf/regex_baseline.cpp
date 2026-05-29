// Spike #043 — naive fast-backend baseline.
//
// Reads compile_commands.json (just the "file" fields), opens each TU, and
// counts #include directives via a regex. No preprocessor, no path resolution
// — strictly the "regex on .cpp" lower bound that we want to compare libclang
// against.
//
// Usage: regex_baseline <compile_commands.json>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// Extracts top-level "file" string values from a CMake-produced
// compile_commands.json. Hand-rolled because JSON deps aren't worth it for a
// throw-away spike.
std::vector<std::string> parse_files(const std::string &db_path)
{
  std::ifstream in(db_path);
  std::stringstream buf;
  buf << in.rdbuf();
  std::string text = buf.str();

  std::vector<std::string> files;
  static const std::regex file_re(R"REGEX("file"\s*:\s*"([^"]+)")REGEX");
  auto it = std::sregex_iterator(text.begin(), text.end(), file_re);
  auto end = std::sregex_iterator();
  for (; it != end; ++it)
  {
    files.push_back((*it)[1].str());
  }
  return files;
}

unsigned count_includes(const std::string &path)
{
  std::ifstream in(path);
  static const std::regex inc_re(R"REGEX(^\s*#\s*include[\s<"])REGEX");
  unsigned n = 0;
  std::string line;
  while (std::getline(in, line))
  {
    if (std::regex_search(line, inc_re))
    {
      ++n;
    }
  }
  return n;
}

} // namespace

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    std::fprintf(stderr, "usage: %s <compile_commands.json>\n", argv[0]);
    return 2;
  }

  auto files = parse_files(argv[1]);
  std::fprintf(stderr, "[baseline] %zu translation units\n", files.size());

  auto t0 = std::chrono::steady_clock::now();
  std::vector<double> per_tu_ms;
  per_tu_ms.reserve(files.size());
  unsigned total_incs = 0;
  for (size_t i = 0; i < files.size(); ++i)
  {
    auto tu_t0 = std::chrono::steady_clock::now();
    unsigned incs = count_includes(files[i]);
    auto tu_t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(tu_t1 - tu_t0).count();
    per_tu_ms.push_back(ms);
    total_incs += incs;
    std::printf("%zu,%.3f,%u,%s\n", i, ms, incs, files[i].c_str());
  }
  auto t1 = std::chrono::steady_clock::now();
  double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

  std::sort(per_tu_ms.begin(), per_tu_ms.end());
  double median = per_tu_ms.empty() ? 0.0 : per_tu_ms[per_tu_ms.size() / 2];

  std::printf("SUMMARY total_ms=%.0f tus=%zu median_tu_ms=%.3f total_includes=%u\n", total_ms, files.size(), median,
              total_incs);
  return 0;
}
