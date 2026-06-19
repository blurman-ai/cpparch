#pragma once

namespace archcheck::git
{

// S6 (#105): hardening flags prepended to every git invocation so an untrusted
// repo's .git/config cannot run commands via hooks / fsmonitor / pager. Single
// source of truth: runGit (one-shot, captures stdout+stderr) and
// GitObjectFileSource (long-running `cat-file --batch`, bidirectional stdio)
// each spawn git with their OWN pipe wiring, but MUST apply the same hardening
// policy — a divergence here is a security hole. Only the policy is shared; the
// fork/pipe plumbing stays separate because the two have different stdio
// topologies. diff.external is intentionally NOT suppressed (an empty value
// breaks `git diff` on git 2.30; callers pass --no-ext-diff instead).
inline constexpr const char *kGitHardeningArgs[] = {
    "-c", "core.hooksPath=/dev/null", "-c", "core.fsmonitor=", "-c", "core.pager=cat",
};
inline constexpr int kGitHardeningCount = 6; // entries in kGitHardeningArgs

} // namespace archcheck::git
