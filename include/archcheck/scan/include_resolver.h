#pragma once

#include <string_view>
#include <vector>

#include "archcheck/scan/include_directive.h"
#include "archcheck/scan/project_files.h"
#include "archcheck/scan/resolved_include.h"

namespace archcheck::scan
{

// Resolve a single include directive against the project index.
// `source_file` is the repo-relative POSIX path of the translation unit
// that contained the directive. Returns a ResolvedInclude tagged with
// one of Project / External / Unresolved / Ambiguous per the algorithm
// described in §4 mini-design of #008.
ResolvedInclude resolve_include(const IncludeDirective &directive, std::string_view source_file,
                                const std::vector<ProjectFile> &files, const ProjectIndex &index);

// Batch convenience: resolve every directive from a single source TU.
std::vector<ResolvedInclude> resolve_includes(const std::vector<IncludeDirective> &directives,
                                              std::string_view source_file, const std::vector<ProjectFile> &files,
                                              const ProjectIndex &index);

} // namespace archcheck::scan
