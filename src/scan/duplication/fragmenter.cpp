#include "archcheck/scan/duplication/fragmenter.h"

#include <cctype>
#include <sstream>
#include <string>
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
  for (int ln = f.startLine; ln <= f.endLine; ++ln)
  {
    if (ln >= 1 && static_cast<std::size_t>(ln) <= lines.size())
    {
      std::string norm = normalizeLine(lines[ln - 1]);
      if (!norm.empty())
      {
        f.normLines.insert(norm);
      }
    }
  }
  f.diversity = trigramDiversity(f.seq);
  return f;
}

void collect(const CollectContext &ctx, std::size_t lo, std::size_t hi)
{
  std::size_t i = lo;
  while (i < hi)
  {
    if (ctx.tokens[i].sym == "{" && ctx.match[i] >= 0 && static_cast<std::size_t>(ctx.match[i]) < hi)
    {
      const std::size_t j = static_cast<std::size_t>(ctx.match[i]);
      const std::size_t body = j - i - 1;
      const bool fnBody = (i > 0 && ctx.tokens[i - 1].sym == ")");
      if (fnBody && body >= ctx.opts.minTokens && body <= ctx.opts.maxTokens)
      {
        ctx.out.push_back(makeFragment(ctx.tokens, i + 1, j, ctx.file, ctx.lines));
      }
      else
      {
        collect(ctx, i + 1, j);
      }
      i = j + 1;
    }
    else
    {
      ++i;
    }
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
