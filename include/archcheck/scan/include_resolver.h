#pragma once

#include <string_view>
#include <vector>

#include "archcheck/scan/include_directive.h"
#include "archcheck/scan/project_files.h"
#include "archcheck/scan/resolved_include.h"

namespace archcheck::scan
{

// Resolve a single include directive against the project index.
// `sourceFile` is the repo-relative POSIX path of the translation unit
// that contained the directive. Returns a ResolvedInclude tagged with
// one of Project / External / Unresolved / Ambiguous per the algorithm
// described in §4 mini-design of #008.
ResolvedInclude resolveInclude(const IncludeDirective &directive, std::string_view sourceFile,
                                const std::vector<ProjectFile> &files, const ProjectIndex &index);

// Batch convenience: resolve every directive from a single source TU.
std::vector<ResolvedInclude> resolveIncludes(const std::vector<IncludeDirective> &directives,
                                              std::string_view sourceFile, const std::vector<ProjectFile> &files,
                                              const ProjectIndex &index);

} // namespace archcheck::scan
