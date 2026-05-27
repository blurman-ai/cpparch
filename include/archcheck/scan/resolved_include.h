#pragma once

#include <string>
#include <vector>

#include "archcheck/scan/include_directive.h"
#include "archcheck/scan/project_files.h"

namespace archcheck::scan
{

enum class Resolution
{
  Project,
  External,
  Unresolved,
  Ambiguous,
};

struct ResolvedInclude
{
  IncludeDirective directive;
  std::string sourceFile; // repo-relative path of the source TU
  Resolution resolution;
  NodeId target;                  // valid only when Resolution::Project
  std::vector<NodeId> candidates; // populated when Resolution::Ambiguous
};

} // namespace archcheck::scan
