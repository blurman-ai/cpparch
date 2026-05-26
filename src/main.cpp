#include <iostream>
#include <string>
#include <string_view>

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
      << "\n"
      << "Under construction. Configuration parsing, graph builder, and rules\n"
      << "land in subsequent v0.1 commits. See:\n"
      << "  https://github.com/blurman-ai/archcheck\n"
      << "  docs/architecture-spec.md\n";
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

   std::cerr << "archcheck: unknown argument '" << arg << "'\n";
   print_help();
   return 2;
}
