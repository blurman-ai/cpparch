#include "archcheck/scan/duplication/fragmenter.h"

#include <cctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace archcheck::scan::duplication
{

namespace
{

double trigramDiversity(const std::vector<std::string> &seq)
{
  if (seq.size() < 3)
  {
    return 1.0;
  }
  std::unordered_set<std::string> grams;
  for (std::size_t i = 0; i + 2 < seq.size(); ++i)
  {
    grams.insert(seq[i] + "\x01" + seq[i + 1] + "\x01" + seq[i + 2]);
  }
  return static_cast<double>(grams.size()) / static_cast<double>(seq.size() - 2);
}

std::vector<int> braceMatch(const std::vector<Token> &t)
{
  std::vector<int> match(t.size(), -1);
  std::vector<std::size_t> stack;
  for (std::size_t i = 0; i < t.size(); ++i)
  {
    if (t[i].sym == "{")
    {
      stack.push_back(i);
    }
    else if (t[i].sym == "}" && !stack.empty())
    {
      const std::size_t open = stack.back();
      stack.pop_back();
      match[open] = static_cast<int>(i);
      match[i] = static_cast<int>(open);
    }
  }
  return match;
}

std::string normalizeLine(const std::string &raw)
{
  std::string out;
  bool prevSpace = true;
  for (char c : raw)
  {
    if (std::isspace(static_cast<unsigned char>(c)) != 0)
    {
      if (!prevSpace)
      {
        out.push_back(' ');
      }
      prevSpace = true;
    }
    else
    {
      out.push_back(c);
      prevSpace = false;
    }
  }
  while (!out.empty() && out.back() == ' ')
  {
    out.pop_back();
  }
  return out;
}

std::vector<std::string> getSourceLines(const std::string &source)
{
  std::vector<std::string> lines;
  std::istringstream iss(source);
  std::string line;
  while (std::getline(iss, line))
  {
    lines.push_back(line);
  }
  return lines;
}

struct CollectContext
{
  const std::vector<Token> &tokens;
  const std::vector<int> &match;
  const FragmentOptions &opts;
  const std::string &file;
  const std::vector<std::string> &lines;
  std::vector<Fragment> &out;
};

// Collect the fragment's verbatim line views: a set for the union ratio, and an
// ordered sequence of substantive lines (drop "}", "{", "});") for the line-LCS run.
// A switch-skeleton line ("case X:", "break;", "default:", "switch (...)") carries no
// distinctive content — every switch has them. Counting them in the ordered line-run lets
// two unrelated switches share a long "run" that is really just dispatch boilerplate.
// #158 Part D experiment: exclude them from the run measure (not the union ratio).
bool isSwitchSkeletonLine(const std::string &n)
{
  if (n == "break;" || n == "default:" || n.rfind("switch (", 0) == 0 || n.rfind("switch(", 0) == 0)
  {
    return true;
  }
  return n.rfind("case ", 0) == 0 && n.back() == ':'; // bare case label, no inline body
}

void collectNormLines(Fragment &f, const std::vector<std::string> &lines)
{
  for (int ln = f.startLine; ln <= f.endLine; ++ln)
  {
    if (ln < 1 || static_cast<std::size_t>(ln) > lines.size())
    {
      continue;
    }
    std::string norm = normalizeLine(lines[ln - 1]);
    if (norm.empty())
    {
      continue;
    }
    if (norm.size() >= 4 && !isSwitchSkeletonLine(norm))
    {
      f.normLineSeq.push_back(norm);
    }
    f.normLines.insert(std::move(norm));
  }
}

Fragment makeFragment(const std::vector<Token> &t, std::size_t lo, std::size_t hi, const std::string &file,
                      const std::vector<std::string> &lines)
{
  Fragment f;
  f.file = file;
  f.startLine = t[lo].line;
  f.endLine = t[hi - 1].line;
  f.tokenCount = hi - lo;
  f.seq.reserve(hi - lo);
  f.rawSeq.reserve(hi - lo);
  for (std::size_t i = lo; i < hi; ++i)
  {
    ++f.bag[t[i].sym];
    f.seq.push_back(t[i].sym);
    f.rawSeq.push_back(t[i].raw.empty() ? t[i].sym : t[i].raw);
  }
  collectNormLines(f, lines);
  f.diversity = trigramDiversity(f.seq);
  return f;
}

using Range = std::pair<std::size_t, std::size_t>;

// Scan [from, to): emit fn-body fragments inline; on the first nested block to
// descend, queue its inner range and the continuation on `work` and stop.
void scanRange(const CollectContext &ctx, std::size_t from, std::size_t to, std::vector<Range> &work)
{
  for (std::size_t i = from; i < to;)
  {
    const bool open = ctx.tokens[i].sym == "{" && ctx.match[i] >= 0 && static_cast<std::size_t>(ctx.match[i]) < to;
    if (!open)
    {
      ++i;
      continue;
    }
    const std::size_t j = static_cast<std::size_t>(ctx.match[i]);
    const std::size_t body = j - i - 1;
    const bool fnBody = (i > 0 && ctx.tokens[i - 1].sym == ")");
    if (fnBody && body >= ctx.opts.minTokens && body <= ctx.opts.maxTokens)
    {
      ctx.out.push_back(makeFragment(ctx.tokens, i + 1, j, ctx.file, ctx.lines));
      i = j + 1;
      continue;
    }
    // Descend into [i+1, j), then resume at j+1. Push continuation first so the
    // inner range pops (runs) first — matching the old recursion's pre-order.
    work.emplace_back(j + 1, to);
    work.emplace_back(i + 1, j);
    return;
  }
}

// Iterative DFS over brace-nested ranges. Recursion here was bounded only by the
// input's brace-nesting depth, so a pathologically deep file (e.g. ctags' vendored
// LLVM parser_overflow fixture — 16k nested braces) blew the call stack → SIGSEGV
// (only under -O2, where makeFragment inlines into the recursive frame). The explicit
// work stack keeps the original pre-order, so the emitted fragment set is identical.
void collect(const CollectContext &ctx, std::size_t lo, std::size_t hi)
{
  std::vector<Range> work;
  work.emplace_back(lo, hi);
  while (!work.empty())
  {
    const auto [from, to] = work.back();
    work.pop_back();
    scanRange(ctx, from, to, work);
  }
}

} // namespace

std::vector<Fragment> extractFragments(const std::vector<Token> &tokens, const std::string &source,
                                       const std::string &file, const FragmentOptions &opts)
{
  if (tokens.empty())
  {
    return {};
  }

  const std::vector<std::string> lines = getSourceLines(source);
  std::vector<Fragment> fragments;
  const std::vector<int> match = braceMatch(tokens);

  CollectContext ctx = {tokens, match, opts, file, lines, fragments};
  collect(ctx, 0, tokens.size());
  return fragments;
}

} // namespace archcheck::scan::duplication
