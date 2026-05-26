#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "archcheck/scan/include_scanner.h"
#include "archcheck/scan/project_files.h"
#include "archcheck/version.h"

namespace
{

void print_version()
{
   std::cout << "archcheck " << archcheck::kVersionString << '\n';
}

void print_help()
{
   std::cout
      << "archcheck - architecture rules for C++ projects\n"
      << "\n"
      << "Usage:\n"
      << "  archcheck --version\n"
      << "  archcheck --help\n"
      << "  archcheck --scan <path>   (preview: discover + scan #includes)\n"
      << "\n"
      << "Under construction. Configuration parsing, graph builder, and rules\n"
      << "land in subsequent v0.1 commits. See:\n"
      << "  https://github.com/blurman-ai/archcheck\n"
      << "  docs/architecture-spec.md\n";
}

std::string read_file(const std::filesystem::path& p)
{
   std::ifstream f(p, std::ios::binary);
   return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

int run_scan(const std::filesystem::path& root)
{
   const auto files = archcheck::scan::discover_files(root);
   std::size_t directives = 0;
   std::size_t quotes = 0;
   std::size_t angles = 0;
   std::size_t diagnostics = 0;
   for (const auto& f : files)
   {
      const auto res = archcheck::scan::scan_includes(read_file(root / f.path));
      directives += res.directives.size();
      diagnostics += res.diagnostics.size();
      for (const auto& d : res.directives)
      {
         (d.kind == archcheck::scan::IncludeKind::Quote ? quotes : angles)++;
      }
   }
   std::cout << "files:       " << files.size() << '\n'
             << "directives:  " << directives << " (quote=" << quotes << ", angle=" << angles << ")\n"
             << "diagnostics: " << diagnostics << '\n';
   return 0;
}

} // namespace

int main(int argc, char* argv[])
{
   if (argc < 2)
   {
      print_help();
      return 0;
   }

   const std::string_view arg{argv[1]};

   if (arg == "--version" || arg == "-V")
   {
      print_version();
      return 0;
   }

   if (arg == "--help" || arg == "-h")
   {
      print_help();
      return 0;
   }

   if (arg == "--scan")
   {
      if (argc < 3)
      {
         std::cerr << "archcheck: --scan requires <path>\n";
         return 2;
      }
      return run_scan(argv[2]);
   }

   std::cerr << "archcheck: unknown argument '" << arg << "'\n";
   print_help();
   return 2;
}
