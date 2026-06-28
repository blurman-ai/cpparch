#include "archcheck/diff/md_report.h"

#include <ostream>
#include <regex>
#include <string>

#include "archcheck/diff/regression_report.h"

namespace archcheck::diff
{

namespace
{

// Turn every `path.ext:line` or `path.ext:a-b` token in `text` into a markdown link to
// that line (or line range) at the head commit, so both the introduced block and its
// source span are clickable. Empty linkBase (off-CI) ⇒ text returned unchanged.
std::string linkify(const std::string &text, const std::string &linkBase)
{
  if (linkBase.empty())
    return text;
  static const std::regex ref(R"(([A-Za-z0-9_./+-]+\.[A-Za-z]+):([0-9]+)(?:-([0-9]+))?)");
  std::string out;
  std::sregex_iterator it(text.begin(), text.end(), ref);
  const std::sregex_iterator end;
  std::size_t last = 0;
  for (; it != end; ++it)
  {
    const std::smatch &m = *it;
    out += text.substr(last, static_cast<std::size_t>(m.position()) - last);
    const std::string anchor = "#L" + m[2].str() + (m[3].matched ? "-L" + m[3].str() : "");
    out += "[`" + m.str() + "`](" + linkBase + m[1].str() + anchor + ")";
    last = static_cast<std::size_t>(m.position()) + static_cast<std::size_t>(m.length());
  }
  out += text.substr(last);
  return out;
}

// The gate summary line. Advisory findings lead (that is what a reviewer cares about
// on a PR); the structural gate state is appended so a red gate is never hidden.
void writeSummary(const RegressionReport &r, const rules::ViolationList &advisories, std::ostream &out)
{
  const char *icon =
      r.gates() ? ":x:" : (r.hasRegression() || !advisories.empty() ? ":large_yellow_circle:" : ":white_check_mark:");
  out << "**Gate:** " << icon << " ";
  if (!advisories.empty())
    out << advisories.size() << " advisory finding(s) · gate " << (r.gates() ? ":x: fail" : ":white_check_mark: ok");
  else if (!r.grownCycles.empty())
    out << r.grownCycles.size() << " grown cycle(s)";
  else if (!r.newGodHeaders.empty())
    out << r.newGodHeaders.size() << " new god-header(s)";
  else if (!r.addedEdges.empty())
    out << r.addedEdges.size() << " added edge(s)";
  else if (!r.newCrossAreaDependencies.empty())
    out << r.newCrossAreaDependencies.size() << " new area dep(s)";
  else
    out << "no violations";
  out << "\n";
}

} // namespace

void writeMdReport(const RegressionReport &r, const rules::ViolationList &advisories, const std::string &linkBase,
                   std::ostream &out)
{
  out << "## archcheck `--diff`\n\n";
  writeSummary(r, advisories, out);

  if (!advisories.empty())
  {
    out << "\n### Findings (advisory)\n\n";
    for (const auto &v : advisories)
    {
      const std::string anchor = v.file + ":" + std::to_string(v.line);
      out << "- " << linkify(anchor, linkBase) << " — " << v.ruleId << " — " << linkify(v.message, linkBase) << "\n";
    }
  }

  // Structural metadata is rarely the headline on a PR — fold it away but keep it one
  // click from the comment. The code-fence preserves the column alignment GitHub
  // markdown would otherwise collapse.
  out << "\n<details><summary>Structural diff</summary>\n\n```\n";
  writeTextReport(r, out);
  out << "```\n</details>\n";
}

void writeMdReport(const RegressionReport &r, std::ostream &out) { writeMdReport(r, {}, "", out); }

} // namespace archcheck::diff
