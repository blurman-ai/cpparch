#include "archcheck/graph/baseline.h"

#include <algorithm>
#include <cstdint>
#include <istream>
#include <ostream>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archcheck/graph/dependency_graph.h"
#include "archcheck/graph/node_id.h"

namespace archcheck::graph
{

namespace
{

constexpr std::string_view kFormatVersion = "1";

using Edge = std::pair<std::uint32_t, std::uint32_t>;

std::vector<std::string> sorted_node_paths(const DependencyGraph &g)
{
  std::vector<std::string> out;
  out.reserve(g.nodeCount());
  for (std::uint32_t i = 0; i < g.nodeCount(); ++i)
  {
    out.emplace_back(g.pathOf(NodeId{i}));
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<Edge> remapped_sorted_edges(const DependencyGraph &g, const std::vector<std::string> &sorted_paths)
{
  std::vector<std::uint32_t> remap(g.nodeCount(), 0);
  for (std::uint32_t new_idx = 0; new_idx < sorted_paths.size(); ++new_idx)
  {
    const std::string_view p = sorted_paths[new_idx];
    for (std::uint32_t old_idx = 0; old_idx < g.nodeCount(); ++old_idx)
    {
      if (g.pathOf(NodeId{old_idx}) == p)
      {
        remap[old_idx] = new_idx;
        break;
      }
    }
  }
  std::vector<Edge> edges;
  for (std::uint32_t old_from = 0; old_from < g.nodeCount(); ++old_from)
  {
    for (NodeId to : g.successors(NodeId{old_from}))
    {
      edges.emplace_back(remap[old_from], remap[to.value]);
    }
  }
  std::sort(edges.begin(), edges.end());
  return edges;
}

void write_nodes_block(std::ostream &out, const std::vector<std::string> &nodes)
{
  out << "nodes:\n";
  for (const auto &p : nodes)
  {
    out << "   - \"" << p << "\"\n";
  }
}

void write_edges_block(std::ostream &out, const std::vector<Edge> &edges)
{
  out << "edges:\n";
  for (const auto &e : edges)
  {
    out << "   - [" << e.first << ", " << e.second << "]\n";
  }
}

} // namespace

void saveBaseline(const DependencyGraph &g, std::ostream &out)
{
  const auto nodes = sorted_node_paths(g);
  const auto edges = remapped_sorted_edges(g, nodes);
  out << "format_version: \"" << kFormatVersion << "\"\n";
  write_nodes_block(out, nodes);
  write_edges_block(out, edges);
}

namespace
{

// ryml requires that the error callback NEVER returns to the parser
// (otherwise the parser loops forever). We throw, catch inside loadBaseline,
// translate into BaselineLoadError, and never let the exception escape.
struct RymlParseException
{
  std::string message;
};

[[noreturn]] void on_ryml_error(const char *msg, std::size_t msg_len, ryml::Location /*loc*/, void * /*user_data*/)
{
  throw RymlParseException{std::string{msg, msg_len}};
}

std::string read_all(std::istream &in)
{
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

BaselineLoadError make_error(BaselineLoadError::Kind kind, std::string msg)
{
  return BaselineLoadError{kind, std::move(msg), 0};
}

std::string_view csub_to_sv(ryml::csubstr s) { return std::string_view{s.str, s.len}; }

std::optional<BaselineLoadError> check_version(ryml::ConstNodeRef root)
{
  if (!root.has_child("format_version"))
  {
    return make_error(BaselineLoadError::Kind::MissingField, "missing required field: format_version");
  }
  const auto v = root["format_version"];
  if (!v.has_val())
  {
    return make_error(BaselineLoadError::Kind::MalformedSchema, "format_version must be a scalar");
  }
  if (csub_to_sv(v.val()) != kFormatVersion)
  {
    std::string m = "unknown format_version: \"";
    m.append(csub_to_sv(v.val()));
    m += "\"";
    return make_error(BaselineLoadError::Kind::UnknownVersion, std::move(m));
  }
  return std::nullopt;
}

std::optional<BaselineLoadError> load_nodes(ryml::ConstNodeRef root, std::vector<std::string> &out)
{
  if (!root.has_child("nodes"))
  {
    return make_error(BaselineLoadError::Kind::MissingField, "missing required field: nodes");
  }
  const auto seq = root["nodes"];
  if (!seq.is_seq())
  {
    return make_error(BaselineLoadError::Kind::MalformedSchema, "nodes must be a sequence");
  }
  for (const auto child : seq.children())
  {
    if (!child.has_val())
    {
      return make_error(BaselineLoadError::Kind::MalformedSchema, "nodes entries must be scalars");
    }
    out.emplace_back(csub_to_sv(child.val()));
  }
  return std::nullopt;
}

std::optional<BaselineLoadError> parse_edge_pair(ryml::ConstNodeRef pair, std::uint32_t nodeCount, Edge &out)
{
  if (!pair.is_seq() || pair.num_children() != 2)
  {
    return make_error(BaselineLoadError::Kind::MalformedSchema, "edge entry must be a pair [from, to]");
  }
  std::uint32_t from = 0;
  std::uint32_t to = 0;
  if (!c4::atox(pair[0].val(), &from) || !c4::atox(pair[1].val(), &to))
  {
    return make_error(BaselineLoadError::Kind::MalformedSchema, "edge indexes must be non-negative integers");
  }
  if (from >= nodeCount || to >= nodeCount)
  {
    return make_error(BaselineLoadError::Kind::MalformedSchema, "edge index out of range");
  }
  out = Edge{from, to};
  return std::nullopt;
}

std::optional<BaselineLoadError> load_edges(ryml::ConstNodeRef root, std::uint32_t nodeCount, std::vector<Edge> &out)
{
  if (!root.has_child("edges"))
  {
    return make_error(BaselineLoadError::Kind::MissingField, "missing required field: edges");
  }
  const auto seq = root["edges"];
  if (!seq.is_seq())
  {
    return make_error(BaselineLoadError::Kind::MalformedSchema, "edges must be a sequence");
  }
  for (const auto pair : seq.children())
  {
    Edge e{0, 0};
    if (auto err = parse_edge_pair(pair, nodeCount, e))
    {
      return err;
    }
    out.push_back(e);
  }
  return std::nullopt;
}

DependencyGraph assemble_graph(const std::vector<std::string> &nodes, const std::vector<Edge> &edges)
{
  DependencyGraph g;
  std::vector<NodeId> ids;
  ids.reserve(nodes.size());
  for (const auto &p : nodes)
  {
    ids.push_back(g.addNode(p));
  }
  for (const auto &e : edges)
  {
    g.addEdge(ids[e.first], ids[e.second]);
  }
  return g;
}

std::pair<DependencyGraph, std::optional<BaselineLoadError>> fail(BaselineLoadError err)
{
  return {DependencyGraph{}, std::move(err)};
}

std::optional<BaselineLoadError> parse_tree(const std::string &text, ryml::Tree &out_tree)
{
  const ryml::Callbacks defaults = ryml::get_callbacks();
  ryml::Callbacks rcb(nullptr, nullptr, nullptr, &on_ryml_error);
  ryml::set_callbacks(rcb);
  try
  {
    out_tree = ryml::parse_in_arena(ryml::to_csubstr(text));
  }
  catch (const RymlParseException &e)
  {
    ryml::set_callbacks(defaults);
    return make_error(BaselineLoadError::Kind::ParseError, e.message);
  }
  ryml::set_callbacks(defaults);
  return std::nullopt;
}

} // namespace

std::pair<DependencyGraph, std::optional<BaselineLoadError>> loadBaseline(std::istream &in)
{
  const std::string text = read_all(in);
  ryml::Tree tree;
  if (auto err = parse_tree(text, tree))
  {
    return fail(std::move(*err));
  }
  const auto root = tree.crootref();
  if (!root.is_map())
  {
    return fail(make_error(BaselineLoadError::Kind::MalformedSchema, "top-level node must be a mapping"));
  }
  if (auto err = check_version(root))
  {
    return fail(std::move(*err));
  }
  std::vector<std::string> nodes;
  if (auto err = load_nodes(root, nodes))
  {
    return fail(std::move(*err));
  }
  std::vector<Edge> edges;
  if (auto err = load_edges(root, static_cast<std::uint32_t>(nodes.size()), edges))
  {
    return fail(std::move(*err));
  }
  return {assemble_graph(nodes, edges), std::nullopt};
}

} // namespace archcheck::graph
