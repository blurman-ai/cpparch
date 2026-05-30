// Standalone C++20 spike for issue #056 — fast-backend partial (Type-3) duplication.
//
// Goal of the spike: prove the core mechanic that line-based block matching
// (#053) cannot — catch *diverged* copies where the same procedure was edited
// in place in both copies (renamed identifiers, flipped operators, inserted
// lines). The unit of comparison is the TOKEN, not the line.
//
// Pipeline (parser-free, no libclang, no compile_commands.json):
//   1. lex + normalize: identifiers -> "id", literals -> "lit"; keep keywords,
//      operators and brackets verbatim.
//   2. split into function-scale fragments via a brace-balance heuristic
//      (a "{" preceded by ")" is a function/control body).
//   3. corpus stats: document frequency per normalized token -> idf weight.
//   4. inverted index on low-frequency tokens -> candidate pairs.
//   5. weighted bag-of-tokens overlap (Ruzicka / weighted Jaccard), filtered by
//      similarity_threshold and min_tokens.
//   6. report pairs file:line <-> file:line with weighted/plain/line metrics.
//
// NOT part of the main build. NOT product plumbing. Token-LCS precise re-rank
// (P3), baseline/gate semantics (P2) and config wiring stay out of this spike.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace
{

struct Options
{
  fs::path root;
  std::size_t minTokens = 30;
  std::size_t maxTokens = 400;
  double simThreshold = 0.60;
  std::size_t rareDfCap = 4;
  std::size_t minSharedRare = 2;
  std::size_t top = 25;
  std::string metric = "weighted";  // "weighted" | "plain" — which score gates+sorts
  bool precise = false;  // --partial-precise: token-LCS re-rank + diff view (P3)
};

// ---------------------------------------------------------------------------
// Lexer + normalization
// ---------------------------------------------------------------------------

const std::unordered_set<std::string>& keywords()
{
  static const std::unordered_set<std::string> kw = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
    "bool", "break", "case", "catch", "char", "char8_t", "char16_t",
    "char32_t", "class", "compl", "concept", "const", "consteval", "constexpr",
    "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "do", "double", "dynamic_cast", "else",
    "enum", "explicit", "export", "extern", "false", "float", "for", "friend",
    "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
    "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
    "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "return", "short", "signed", "sizeof", "static",
    "static_assert", "static_cast", "struct", "switch", "template", "this",
    "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
    "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t",
    "while", "xor", "xor_eq", "override", "final"
  };
  return kw;
}

struct Token
{
  std::string sym;  // normalized symbol
  int line = 0;     // 1-based source line
};

bool isIdentStart(char c)
{
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isIdentChar(char c)
{
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Longest-match operator table. 3-char ops first, then 2-char.
const std::vector<std::string>& multiOps()
{
  static const std::vector<std::string> ops = {
    "<<=", ">>=", "->*", "...", "<=>",
    "::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", ".*"
  };
  return ops;
}

std::vector<Token> lex(const std::string& src)
{
  std::vector<Token> out;
  const std::size_t n = src.size();
  std::size_t i = 0;
  int line = 1;

  while (i < n)
  {
    const char c = src[i];

    if (c == '\n')
    {
      ++line;
      ++i;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c)) != 0)
    {
      ++i;
      continue;
    }
    // line comment
    if (c == '/' && i + 1 < n && src[i + 1] == '/')
    {
      while (i < n && src[i] != '\n')
      {
        ++i;
      }
      continue;
    }
    // block comment
    if (c == '/' && i + 1 < n && src[i + 1] == '*')
    {
      i += 2;
      while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/'))
      {
        if (src[i] == '\n')
        {
          ++line;
        }
        ++i;
      }
      i += 2;
      continue;
    }
    // string literal
    if (c == '"')
    {
      const int startLine = line;
      ++i;
      while (i < n && src[i] != '"')
      {
        if (src[i] == '\\' && i + 1 < n)
        {
          i += 2;
          continue;
        }
        if (src[i] == '\n')
        {
          ++line;
        }
        ++i;
      }
      ++i;  // closing quote
      out.push_back({"lit", startLine});
      continue;
    }
    // char literal
    if (c == '\'')
    {
      ++i;
      while (i < n && src[i] != '\'')
      {
        if (src[i] == '\\' && i + 1 < n)
        {
          i += 2;
          continue;
        }
        ++i;
      }
      ++i;
      out.push_back({"lit", line});
      continue;
    }
    // number
    if (std::isdigit(static_cast<unsigned char>(c)) != 0
        || (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(src[i + 1])) != 0))
    {
      ++i;
      while (i < n)
      {
        const char d = src[i];
        if (isIdentChar(d) || d == '.' || d == '\'')
        {
          ++i;
        }
        else if ((d == '+' || d == '-') && i > 0
                 && (src[i - 1] == 'e' || src[i - 1] == 'E'
                     || src[i - 1] == 'p' || src[i - 1] == 'P'))
        {
          ++i;  // exponent sign
        }
        else
        {
          break;
        }
      }
      out.push_back({"lit", line});
      continue;
    }
    // identifier / keyword
    if (isIdentStart(c))
    {
      const std::size_t start = i;
      while (i < n && isIdentChar(src[i]))
      {
        ++i;
      }
      std::string word = src.substr(start, i - start);
      out.push_back({keywords().count(word) != 0 ? word : std::string("id"), line});
      continue;
    }
    // operator / punctuation (longest match)
    bool matched = false;
    for (const std::string& op : multiOps())
    {
      if (src.compare(i, op.size(), op) == 0)
      {
        out.push_back({op, line});
        i += op.size();
        matched = true;
        break;
      }
    }
    if (!matched)
    {
      out.push_back({std::string(1, c), line});
      ++i;
    }
  }

  return out;
}

// ---------------------------------------------------------------------------
// Fragments
// ---------------------------------------------------------------------------

struct Fragment
{
  std::string file;
  int startLine = 0;
  int endLine = 0;
  std::size_t tokenCount = 0;
  std::unordered_map<std::string, int> bag;  // normalized token -> count
  std::vector<std::string> seq;  // ordered normalized tokens (for token-LCS)
  std::unordered_set<std::string> normLines;  // illustrative line-based view
};

// match[i] = index of the brace matching the one at i, or -1.
std::vector<int> braceMatch(const std::vector<Token>& t)
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

std::string normalizeLine(const std::string& raw)
{
  std::string out;
  bool prevSpace = true;  // collapse leading ws
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

Fragment makeFragment(const std::vector<Token>& t, std::size_t lo, std::size_t hi,
                      const std::string& file, const std::vector<std::string>& lines)
{
  Fragment f;
  f.file = file;
  f.startLine = t[lo].line;
  f.endLine = t[hi].line;
  f.tokenCount = hi - lo;
  f.seq.reserve(hi - lo);
  for (std::size_t i = lo; i < hi; ++i)
  {
    ++f.bag[t[i].sym];
    f.seq.push_back(t[i].sym);
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
  return f;
}

// A "{" preceded by ")" is a function/control body. Emit such blocks of
// function scale; otherwise descend to find inner bodies.
void collect(const std::vector<Token>& t, const std::vector<int>& match,
             std::size_t lo, std::size_t hi, const Options& opt,
             const std::string& file, const std::vector<std::string>& lines,
             std::vector<Fragment>& out)
{
  std::size_t i = lo;
  while (i < hi)
  {
    if (t[i].sym == "{" && match[i] >= 0 && static_cast<std::size_t>(match[i]) < hi)
    {
      const std::size_t j = static_cast<std::size_t>(match[i]);
      const std::size_t body = j - i - 1;
      const bool fnBody = (i > 0 && t[i - 1].sym == ")");
      if (fnBody && body >= opt.minTokens && body <= opt.maxTokens)
      {
        out.push_back(makeFragment(t, i + 1, j, file, lines));
      }
      else
      {
        collect(t, match, i + 1, j, opt, file, lines, out);
      }
      i = j + 1;
    }
    else
    {
      ++i;
    }
  }
}

std::vector<std::string> splitLines(const std::string& src)
{
  std::vector<std::string> lines;
  std::string cur;
  for (char c : src)
  {
    if (c == '\n')
    {
      lines.push_back(cur);
      cur.clear();
    }
    else if (c != '\r')
    {
      cur.push_back(c);
    }
  }
  lines.push_back(cur);
  return lines;
}

// ---------------------------------------------------------------------------
// Similarity
// ---------------------------------------------------------------------------

double weightedJaccard(const Fragment& a, const Fragment& b,
                       const std::unordered_map<std::string, double>& idf)
{
  double num = 0.0;
  double den = 0.0;
  // union of keys
  std::unordered_set<std::string> keys;
  for (const auto& [k, v] : a.bag)
  {
    keys.insert(k);
  }
  for (const auto& [k, v] : b.bag)
  {
    keys.insert(k);
  }
  for (const std::string& k : keys)
  {
    auto ia = a.bag.find(k);
    auto ib = b.bag.find(k);
    const int ca = ia == a.bag.end() ? 0 : ia->second;
    const int cb = ib == b.bag.end() ? 0 : ib->second;
    double w = 1.0;
    auto wi = idf.find(k);
    if (wi != idf.end())
    {
      w = wi->second;
    }
    num += w * std::min(ca, cb);
    den += w * std::max(ca, cb);
  }
  return den > 0.0 ? num / den : 0.0;
}

double plainJaccard(const Fragment& a, const Fragment& b)
{
  int num = 0;
  int den = 0;
  std::unordered_set<std::string> keys;
  for (const auto& [k, v] : a.bag)
  {
    keys.insert(k);
  }
  for (const auto& [k, v] : b.bag)
  {
    keys.insert(k);
  }
  for (const std::string& k : keys)
  {
    auto ia = a.bag.find(k);
    auto ib = b.bag.find(k);
    const int ca = ia == a.bag.end() ? 0 : ia->second;
    const int cb = ib == b.bag.end() ? 0 : ib->second;
    num += std::min(ca, cb);
    den += std::max(ca, cb);
  }
  return den > 0 ? static_cast<double>(num) / den : 0.0;
}

double lineOverlap(const Fragment& a, const Fragment& b)
{
  if (a.normLines.empty() || b.normLines.empty())
  {
    return 0.0;
  }
  std::size_t inter = 0;
  const auto& small = a.normLines.size() < b.normLines.size() ? a.normLines : b.normLines;
  const auto& big = a.normLines.size() < b.normLines.size() ? b.normLines : a.normLines;
  for (const std::string& s : small)
  {
    if (big.count(s) != 0)
    {
      ++inter;
    }
  }
  const std::size_t uni = a.normLines.size() + b.normLines.size() - inter;
  return uni > 0 ? static_cast<double>(inter) / uni : 0.0;
}

// ---------------------------------------------------------------------------
// Token-LCS (precise re-rank, P3)
// ---------------------------------------------------------------------------
//
// Order-aware: bag overlap is blind to sequence, so a flipped-operator copy
// (case A) and a different body behind a shared case/break skeleton (case D)
// can look alike. LCS over the ordered token streams separates them — A aligns
// ~1:1 (only substituted operators break runs), D only aligns on the skeleton.

// Rolling-array LCS length. O(n*m) time, O(m) space.
std::size_t lcsLength(const std::vector<std::string>& a, const std::vector<std::string>& b)
{
  const std::size_t n = a.size();
  const std::size_t m = b.size();
  std::vector<std::size_t> prev(m + 1, 0);
  std::vector<std::size_t> cur(m + 1, 0);
  for (std::size_t i = 1; i <= n; ++i)
  {
    for (std::size_t j = 1; j <= m; ++j)
    {
      cur[j] = a[i - 1] == b[j - 1] ? prev[j - 1] + 1 : std::max(prev[j], cur[j - 1]);
    }
    std::swap(prev, cur);
  }
  return prev[m];
}

// Dice-style LCS ratio in [0,1]: 1.0 when the two token streams are identical.
double lcsRatio(const Fragment& a, const Fragment& b)
{
  const std::size_t denom = a.seq.size() + b.seq.size();
  if (denom == 0)
  {
    return 0.0;
  }
  return 2.0 * static_cast<double>(lcsLength(a.seq, b.seq)) / static_cast<double>(denom);
}

struct DiffOp
{
  char tag = '=';  // '=' equal, '~' changed, '-' delete, '+' insert
  std::string a;
  std::string b;
};

// Full-table LCS backtrack -> edit script, with adjacent del+ins collapsed into
// a single "changed" op so the report reads as "+ -> -", not "- then +".
// Only called on the few reported pairs, so the O(n*m) table is affordable.
std::vector<DiffOp> diffTokens(const std::vector<std::string>& a, const std::vector<std::string>& b)
{
  const std::size_t n = a.size();
  const std::size_t m = b.size();
  std::vector<std::vector<std::size_t>> dp(n + 1, std::vector<std::size_t>(m + 1, 0));
  for (std::size_t i = 1; i <= n; ++i)
  {
    for (std::size_t j = 1; j <= m; ++j)
    {
      dp[i][j] = a[i - 1] == b[j - 1] ? dp[i - 1][j - 1] + 1 : std::max(dp[i - 1][j], dp[i][j - 1]);
    }
  }
  std::vector<DiffOp> raw;
  std::size_t i = n;
  std::size_t j = m;
  while (i > 0 && j > 0)
  {
    if (a[i - 1] == b[j - 1])
    {
      raw.push_back({'=', a[i - 1], b[j - 1]});
      --i;
      --j;
    }
    else if (dp[i - 1][j] >= dp[i][j - 1])
    {
      raw.push_back({'-', a[i - 1], ""});
      --i;
    }
    else
    {
      raw.push_back({'+', "", b[j - 1]});
      --j;
    }
  }
  while (i > 0)
  {
    raw.push_back({'-', a[--i], ""});
  }
  while (j > 0)
  {
    raw.push_back({'+', "", b[--j]});
  }
  std::reverse(raw.begin(), raw.end());

  std::vector<DiffOp> out;
  for (std::size_t k = 0; k < raw.size(); ++k)
  {
    const bool delThenIns = raw[k].tag == '-' && k + 1 < raw.size() && raw[k + 1].tag == '+';
    const bool insThenDel = raw[k].tag == '+' && k + 1 < raw.size() && raw[k + 1].tag == '-';
    if (delThenIns)
    {
      out.push_back({'~', raw[k].a, raw[k + 1].b});
      ++k;
    }
    else if (insThenDel)
    {
      out.push_back({'~', raw[k + 1].a, raw[k].b});
      ++k;
    }
    else
    {
      out.push_back(raw[k]);
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------

bool isSourceFile(const fs::path& p)
{
  static const std::unordered_set<std::string> exts = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"
  };
  return exts.count(p.extension().string()) != 0;
}

struct Pair
{
  std::size_t a = 0;
  std::size_t b = 0;
  double weighted = 0.0;
  double plain = 0.0;
  double line = 0.0;
  double lcs = 0.0;
  std::size_t sharedRare = 0;
};

void printUsage()
{
  std::cout << "usage: partial_duplication <root> [--min-tokens N] [--max-tokens N]\n"
               "                           [--threshold F] [--rare-df N]\n"
               "                           [--min-shared N] [--metric weighted|plain]\n"
               "                           [--partial-precise] [--top N]\n";
}

}  // namespace

int main(int argc, char** argv)
{
  Options opt;
  std::vector<std::string> positional;
  bool thresholdSet = false;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (arg == "--min-tokens")
    {
      opt.minTokens = std::stoul(next());
    }
    else if (arg == "--max-tokens")
    {
      opt.maxTokens = std::stoul(next());
    }
    else if (arg == "--threshold")
    {
      opt.simThreshold = std::stod(next());
      thresholdSet = true;
    }
    else if (arg == "--rare-df")
    {
      opt.rareDfCap = std::stoul(next());
    }
    else if (arg == "--min-shared")
    {
      opt.minSharedRare = std::stoul(next());
    }
    else if (arg == "--metric")
    {
      opt.metric = next();
    }
    else if (arg == "--partial-precise")
    {
      opt.precise = true;
    }
    else if (arg == "--top")
    {
      opt.top = std::stoul(next());
    }
    else if (arg == "-h" || arg == "--help")
    {
      printUsage();
      return 0;
    }
    else
    {
      positional.push_back(arg);
    }
  }
  if (positional.empty())
  {
    printUsage();
    return 2;
  }
  opt.root = positional.front();
  if (!fs::exists(opt.root))
  {
    std::cerr << "error: path not found: " << opt.root << "\n";
    return 2;
  }
  // token-LCS lives on a higher scale than bag-overlap: the normalized alphabet
  // (id/lit/keywords/operators) is tiny, so any two C++ bodies share a long
  // subsequence (~0.7 floor). Give precise mode its own default gate unless the
  // user pinned --threshold explicitly.
  if (opt.precise && !thresholdSet)
  {
    opt.simThreshold = 0.80;
  }

  // 1-2. collect fragments from every source file under root.
  std::vector<Fragment> frags;
  std::size_t fileCount = 0;
  std::vector<fs::path> files;
  if (fs::is_directory(opt.root))
  {
    for (const auto& e : fs::recursive_directory_iterator(opt.root))
    {
      if (e.is_regular_file() && isSourceFile(e.path()))
      {
        files.push_back(e.path());
      }
    }
  }
  else
  {
    files.push_back(opt.root);
  }
  std::sort(files.begin(), files.end());
  for (const fs::path& p : files)
  {
    std::ifstream in(p, std::ios::binary);
    if (!in)
    {
      continue;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string src = ss.str();
    ++fileCount;
    const std::vector<Token> toks = lex(src);
    const std::vector<int> match = braceMatch(toks);
    const std::vector<std::string> lines = splitLines(src);
    const std::string rel = fs::relative(p, fs::is_directory(opt.root) ? opt.root : opt.root.parent_path()).string();
    collect(toks, match, 0, toks.size(), opt, rel, lines, frags);
  }

  const std::size_t N = frags.size();
  if (N < 2)
  {
    std::cout << "scanned " << fileCount << " files, " << N
              << " fragments — nothing to compare.\n";
    return 0;
  }

  // 3. document frequency -> idf weight (ubiquitous tokens collapse to ~0).
  std::unordered_map<std::string, int> df;
  for (const Fragment& f : frags)
  {
    for (const auto& [sym, cnt] : f.bag)
    {
      ++df[sym];
    }
  }
  std::unordered_map<std::string, double> idf;
  for (const auto& [sym, d] : df)
  {
    idf[sym] = std::log(static_cast<double>(N) / static_cast<double>(d));
  }

  // 4. inverted index on low-frequency tokens -> candidate pairs.
  // Precise mode (P3) widens candidate recall: LCS provides the precision, so
  // the cheap gate can afford to be loose. Normalized C++ has a tiny alphabet,
  // so a tight rare-token gate alone misses order-divergent copies (case A never
  // shares >=2 df<=4 tokens with its original after id/lit collapsing).
  const std::size_t effRareDf = opt.precise ? std::max<std::size_t>(opt.rareDfCap, 8) : opt.rareDfCap;
  const std::size_t effMinShared = opt.precise ? std::size_t{1} : opt.minSharedRare;
  std::unordered_map<std::string, std::vector<std::size_t>> postings;
  for (std::size_t fi = 0; fi < N; ++fi)
  {
    for (const auto& [sym, cnt] : frags[fi].bag)
    {
      if (static_cast<std::size_t>(df[sym]) <= effRareDf)
      {
        postings[sym].push_back(fi);
      }
    }
  }
  std::map<std::pair<std::size_t, std::size_t>, std::size_t> sharedRare;
  for (const auto& [sym, list] : postings)
  {
    for (std::size_t x = 0; x < list.size(); ++x)
    {
      for (std::size_t y = x + 1; y < list.size(); ++y)
      {
        ++sharedRare[{list[x], list[y]}];
      }
    }
  }

  // 5. score candidates, keep those above the gating-metric threshold.
  // gate metric: --partial-precise -> token-LCS; else --metric (weighted|plain).
  auto scoreOf = [&opt](const Pair& p) -> double
  { return opt.precise ? p.lcs : (opt.metric == "plain" ? p.plain : p.weighted); };
  std::vector<Pair> reported;
  std::size_t candidateCount = 0;
  for (const auto& [pr, shared] : sharedRare)
  {
    if (shared < effMinShared)
    {
      continue;
    }
    ++candidateCount;
    Pair p;
    p.a = pr.first;
    p.b = pr.second;
    p.sharedRare = shared;
    p.weighted = weightedJaccard(frags[p.a], frags[p.b], idf);
    p.plain = plainJaccard(frags[p.a], frags[p.b]);
    p.line = lineOverlap(frags[p.a], frags[p.b]);
    if (opt.precise)
    {
      p.lcs = lcsRatio(frags[p.a], frags[p.b]);
    }
    if (scoreOf(p) >= opt.simThreshold)
    {
      reported.push_back(p);
    }
  }
  std::sort(reported.begin(), reported.end(),
            [&scoreOf](const Pair& l, const Pair& r) { return scoreOf(l) > scoreOf(r); });

  // 6. report.
  const std::string gateName = opt.precise ? "token-LCS" : (opt.metric == "plain" ? "plain" : "weighted");
  const std::size_t totalPairs = N * (N - 1) / 2;
  std::cout << "scanned " << fileCount << " files, " << N << " fragments\n";
  std::cout << "candidate pairs (>= " << effMinShared << " rare tokens, df<="
            << effRareDf << "): " << candidateCount << " of " << totalPairs
            << " possible\n";
  std::cout << "reported (" << gateName << " >= " << opt.simThreshold
            << "): " << reported.size() << "\n\n";

  const std::size_t shown = std::min(reported.size(), opt.top);
  for (std::size_t i = 0; i < shown; ++i)
  {
    const Pair& p = reported[i];
    const Fragment& fa = frags[p.a];
    const Fragment& fb = frags[p.b];
    std::cout << "[" << gateName << "=" << scoreOf(p) << "]  " << fa.file << ":"
              << fa.startLine << "-" << fa.endLine << "  <->  " << fb.file << ":"
              << fb.startLine << "-" << fb.endLine << "\n";
    std::cout << "         tokens " << fa.tokenCount << "/" << fb.tokenCount
              << "  weighted=" << p.weighted << "  plain=" << p.plain
              << "  line=" << p.line;
    if (opt.precise)
    {
      std::cout << "  lcs=" << p.lcs;
    }
    std::cout << "  shared-rare=" << p.sharedRare << "\n";
    if (opt.precise)
    {
      const std::vector<DiffOp> ops = diffTokens(fa.seq, fb.seq);
      std::size_t changed = 0;
      std::size_t dels = 0;
      std::size_t ins = 0;
      for (const DiffOp& o : ops)
      {
        changed += o.tag == '~' ? 1 : 0;
        dels += o.tag == '-' ? 1 : 0;
        ins += o.tag == '+' ? 1 : 0;
      }
      std::cout << "         diff: " << changed << " changed, " << dels
                << " removed, " << ins << " added\n";
      std::size_t printed = 0;
      for (const DiffOp& o : ops)
      {
        if (o.tag == '=')
        {
          continue;
        }
        if (printed == 12)
        {
          std::cout << "           ...\n";
          break;
        }
        if (o.tag == '~')
        {
          std::cout << "           ~ " << o.a << " -> " << o.b << "\n";
        }
        else if (o.tag == '-')
        {
          std::cout << "           - " << o.a << "\n";
        }
        else
        {
          std::cout << "           + " << o.b << "\n";
        }
        ++printed;
      }
    }
  }

  return 0;
}
