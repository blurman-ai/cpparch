#pragma once

#include <string>
#include <vector>

namespace archcheck::scan
{

enum class IncludeKind
{
  Quote,
  Angle,
};

struct IncludeDirective
{
  IncludeKind kind;
  std::string token;
  int line;
};

enum class DiagnosticKind
{
  MacroInclude,
};

struct IncludeScanDiagnostic
{
  DiagnosticKind kind;
  std::string rawToken;
  int line;
};

struct ScanResult
{
  std::vector<IncludeDirective> directives;
  std::vector<IncludeScanDiagnostic> diagnostics;
};

} // namespace archcheck::scan
