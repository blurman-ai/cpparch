#include "archcheck/diff/diff_json_report.h"

#include "archcheck/report/json_escape.h"

namespace archcheck::diff
{

namespace
{

using report::jsonEscape;

void writeStringArray(const std::vector<std::string> &v, std::ostream &out)
{
  out << '[';
  for (std::size_t i = 0; i < v.size(); ++i)
    out << (i ? ", " : "") << '"' << jsonEscape(v[i]) << '"';
  out << ']';
}

void writeGrownCycles(const std::vector<GrownCycle> &v, std::ostream &out)
{
  out << '[';
  for (std::size_t i = 0; i < v.size(); ++i)
  {
    out << (i ? ", " : "") << "{\"baseline_size\": " << v[i].baselineSize << ", \"current_size\": " << v[i].currentSize
        << ", \"members\": ";
    writeStringArray(v[i].members, out);
    out << '}';
  }
  out << ']';
}

template <typename Edge> void writeEdges(const std::vector<Edge> &v, std::ostream &out)
{
  out << '[';
  for (std::size_t i = 0; i < v.size(); ++i)
    out << (i ? ", " : "") << "{\"from\": \"" << jsonEscape(v[i].from) << "\", \"to\": \"" << jsonEscape(v[i].to)
        << "\"}";
  out << ']';
}

void writeCrossArea(const std::vector<NewCrossAreaDependency> &v, std::ostream &out)
{
  out << '[';
  for (std::size_t i = 0; i < v.size(); ++i)
    out << (i ? ", " : "") << "{\"from_area\": \"" << jsonEscape(v[i].fromArea) << "\", \"to_area\": \""
        << jsonEscape(v[i].toArea) << "\", \"edge_count\": " << v[i].edgeCount << ", \"sample_from\": \""
        << jsonEscape(v[i].sampleFrom) << "\", \"sample_to\": \"" << jsonEscape(v[i].sampleTo) << "\"}";
  out << ']';
}

void writeViolations(const rules::ViolationList &v, std::ostream &out)
{
  out << '[';
  for (std::size_t i = 0; i < v.size(); ++i)
    out << (i ? ", " : "") << "{\"rule\": \"" << jsonEscape(v[i].ruleId) << "\", \"file\": \"" << jsonEscape(v[i].file)
        << "\", \"line\": " << v[i].line << ", \"message\": \"" << jsonEscape(v[i].message) << "\"}";
  out << ']';
}

void writeChainLength(const std::optional<MetricDelta> &d, std::ostream &out)
{
  if (!d)
  {
    out << "null";
    return;
  }
  out << "{\"baseline\": " << d->baseline << ", \"current\": " << d->current << '}';
}

} // namespace

void writeJsonReport(const RegressionReport &r, const DiffJsonContext &ctx, std::ostream &out)
{
  out << "{\n  \"version\": 1,\n"
      << "  \"baseline_ref\": \"" << jsonEscape(ctx.baselineRef) << "\",\n"
      << "  \"current_ref\": \"" << jsonEscape(ctx.currentRef) << "\",\n"
      << "  \"gate\": \"" << (r.gates() ? "fail" : "ok") << "\",\n"
      << "  \"gating\": {\n    \"grown_cycles\": ";
  writeGrownCycles(r.grownCycles, out);
  out << ",\n    \"new_god_headers\": ";
  writeStringArray(r.newGodHeaders, out);
  out << "\n  },\n  \"advisory\": {\n    \"added_edges\": ";
  writeEdges(r.addedEdges, out);
  out << ",\n    \"removed_edges\": ";
  writeEdges(r.removedEdges, out);
  out << ",\n    \"new_cross_area_dependencies\": ";
  writeCrossArea(r.newCrossAreaDependencies, out);
  out << ",\n    \"chain_length_grown\": ";
  writeChainLength(r.chainLengthGrown, out);
  out << ",\n    \"nccd_delta\": ";
  if (r.nccdDelta)
    out << *r.nccdDelta;
  else
    out << "null";
  out << ",\n    \"complexity_skipped_added_lines\": " << ctx.complexitySkippedAddedLines;
  out << ",\n    \"violations\": ";
  writeViolations(ctx.advisoryViolations, out);
  out << "\n  }\n}\n";
}

} // namespace archcheck::diff
