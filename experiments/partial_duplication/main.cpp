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
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "archcheck/scan/file_classification.h"  // #069: shared isVendoredFile (curated names + license header)

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
  double rareDfPct = 0.0;  // --rare-df-pct: if >0, rare cutoff = max(rareDfCap, N*pct/100)
  std::size_t minSharedRare = 2;
  std::size_t top = 25;
  std::string metric = "weighted";  // "weighted" | "plain" — which score gates+sorts
  bool precise = false;  // --partial-precise: token-LCS re-rank + diff view (P3)
  std::vector<std::string> excludes;  // --exclude substr: skip files whose path contains it
  bool skipGenerated = true;  // #065: skip in-tree generated code (protobuf/moc/flatbuffers) by header marker; --no-skip-generated to disable
  bool skipVendored = true;  // #069: skip vendored single-file libs (qcustomplot/json.hpp/stb_*) by curated name + license header; --no-skip-vendored to disable
  double minDiversity = 0.0;  // --min-diversity: drop pairs where either fragment is skeletal (low trigram diversity)
  bool keepCalls = true;  // selective normalization (DEFAULT): keep callee names (id before "(") and case labels; --no-keep-calls to disable. Kills coincidental call-skeleton + switch-idiom FPs.
  // --- diff mode (#054): "did commit C add code that is a Type-3 near-dup of
  //     code that already existed at parent P?" ---
  std::string diffSpec;      // --diff <sha> | <A>..<B>  (empty => snapshot mode)
  fs::path repo;             // --repo <path> for diff mode
  std::string subpath;       // --subpath <rel>: restrict diff+baseline to this subtree
  // --- LD.14 clone growth (#071): persist density, gate on its delta across runs ---
  std::string cloneBaseline;    // --clone-baseline <path>: read prior density, report delta, rewrite
  double cloneGrowthMax = 0.0;  // --clone-growth-max <pct>: exit 1 if density grew by more than this
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
  std::string raw;  // original spelling when sym is a placeholder ("id"/"lit"); empty => == sym
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

// Raw string opener length: if `src` at `i` begins a raw string literal
// (R"( ... )", also prefixed LR/uR/UR/u8R), returns the introducer length up to
// and including the opening `"(`; else 0. Embedded shaders/SQL/scripts inside
// raw strings are DATA, not code — the body must collapse to one "lit" rather
// than be tokenized. #056's plain string scanner mishandles blobs containing
// `"` or `\`. Ported from #053 (#056 data-table FP class). Scope: default
// delimiter R"(...)" only — no custom R"xx(...)xx" (documented limitation).
[[nodiscard]] std::size_t rawStringPrefixLen(const std::string& src, std::size_t i)
{
  const std::size_t n = src.size();
  std::size_t j = i;
  if (src.compare(j, 2, "u8") == 0)
  {
    j += 2;
  }
  else if (j < n && (src[j] == 'L' || src[j] == 'u' || src[j] == 'U'))
  {
    j += 1;
  }
  if (j >= n || src[j] != 'R')
  {
    return 0;
  }
  ++j;
  if (j + 1 < n && src[j] == '"' && src[j + 1] == '(')
  {
    return (j + 2) - i;
  }
  return 0;
}

// Is the `#` at `i` (line start) a literal-false conditional opener — `#if 0`
// or `#if false`? Pragmatic: only the bare-false form (not `#if defined(X)&&0`),
// enough for embedded dead code (fxc disassembly listings in shader headers).
[[nodiscard]] bool isDeadIfOpener(const std::string& src, std::size_t i)
{
  const std::size_t n = src.size();
  std::size_t j = i + 1;  // past '#'
  while (j < n && (src[j] == ' ' || src[j] == '\t'))
  {
    ++j;
  }
  if (src.compare(j, 2, "if") != 0)
  {
    return false;
  }
  j += 2;
  if (j < n && isIdentChar(src[j]))  // reject #ifdef / #ifndef
  {
    return false;
  }
  while (j < n && (src[j] == ' ' || src[j] == '\t'))
  {
    ++j;
  }
  if (src.compare(j, 5, "false") == 0)
  {
    return true;
  }
  if (j < n && src[j] == '0')
  {
    const char d = (j + 1 < n) ? src[j + 1] : '\n';
    return d == '\n' || d == '\r' || d == ' ' || d == '\t' || d == '/';
  }
  return false;
}

// Skip a `#if 0`/`#if false` ... `#endif` region (inclusive), honoring nested
// `#if*`. Advances `i`/`line` past the dead block. #056 doesn't preprocess, so
// without this the dead body tokenizes as live code and inflates matches.
// Ported from #053.
void skipDeadIfBlock(const std::string& src, std::size_t& i, int& line)
{
  const std::size_t n = src.size();
  int depth = 0;  // opener brings depth to 1; matching #endif back to 0
  while (i < n)
  {
    if (src[i] == '#')
    {
      std::size_t j = i + 1;
      while (j < n && (src[j] == ' ' || src[j] == '\t'))
      {
        ++j;
      }
      if (src.compare(j, 2, "if") == 0)  // #if / #ifdef / #ifndef
      {
        ++depth;
      }
      else if (src.compare(j, 5, "endif") == 0)
      {
        --depth;
      }
    }
    while (i < n && src[i] != '\n')  // consume to end of line
    {
      ++i;
    }
    if (i < n)
    {
      ++i;
      ++line;
    }
    if (depth == 0)
    {
      break;
    }
  }
}

std::vector<Token> lex(const std::string& src, bool keepCalls = false)
{
  std::vector<Token> out;
  const std::size_t n = src.size();
  std::size_t i = 0;
  int line = 1;
  bool atLineStart = true;  // only whitespace seen since last newline

  while (i < n)
  {
    const char c = src[i];

    if (c == '\n')
    {
      ++line;
      ++i;
      atLineStart = true;
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
    // dead preprocessor block: #if 0 / #if false ... #endif (line-start only).
    // Without this the dead body tokenizes as live code and inflates matches.
    if (atLineStart && c == '#' && isDeadIfOpener(src, i))
    {
      skipDeadIfBlock(src, i, line);
      atLineStart = true;
      continue;
    }
    atLineStart = false;  // any char reaching here produces a token
    // raw string literal R"( ... )" — collapse body (data, not code) to "lit"
    if (const std::size_t pre = rawStringPrefixLen(src, i); pre != 0)
    {
      const std::size_t litStart = i;
      const int startLine = line;
      i += pre;  // positioned just past the opening "("
      while (i + 1 < n && !(src[i] == ')' && src[i + 1] == '"'))
      {
        if (src[i] == '\n')
        {
          ++line;
        }
        ++i;
      }
      i += 2;  // past the closing )"
      out.push_back({"lit", startLine, src.substr(litStart, i - litStart)});
      continue;
    }
    // string literal
    if (c == '"')
    {
      const std::size_t litStart = i;
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
      out.push_back({"lit", startLine, src.substr(litStart, i - litStart)});
      continue;
    }
    // char literal
    if (c == '\'')
    {
      const std::size_t litStart = i;
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
      out.push_back({"lit", line, src.substr(litStart, i - litStart)});
      continue;
    }
    // number
    if (std::isdigit(static_cast<unsigned char>(c)) != 0
        || (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(src[i + 1])) != 0))
    {
      const std::size_t litStart = i;
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
      out.push_back({"lit", line, src.substr(litStart, i - litStart)});
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
      if (keywords().count(word) != 0)
      {
        out.push_back({word, line});
      }
      else if (keepCalls)
      {
        // Keep semantically distinctive identifiers, collapse only local
        // var/param names. Two cases:
        //  - callee names: identifier immediately followed by `(` (API/helper
        //    calls) — kills coincidental "id = id(id);" skeleton matches;
        //  - case labels: identifier right after `case` — kills the
        //    "switch{case id: return lit}" idiom-FP (enum tables of different
        //    domains stop matching: ddsA8 vs IDC_ALT_MAT vs TRMT_SHIP).
        std::size_t j = i;
        while (j < n && (src[j] == ' ' || src[j] == '\t' || src[j] == '\n' || src[j] == '\r'))
        {
          ++j;
        }
        const bool callee = (j < n && src[j] == '(');
        const bool caseLabel = (!out.empty() && out.back().sym == "case");
        const bool keep = callee || caseLabel;
        out.push_back({keep ? word : std::string("id"), line, keep ? std::string() : word});
      }
      else
      {
        out.push_back({std::string("id"), line, word});
      }
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
  std::vector<std::string> rawSeq;  // raw spelling per token, aligned with seq (== sym for non-placeholders) — LD.10 clone-type
  std::unordered_set<std::string> normLines;  // illustrative line-based view
  double diversity = 1.0;  // distinct-trigram ratio; low = skeletal (dispatch switch, data table)
};

// Distinct normalized-trigram ratio. A dispatch switch / enum table repeats
// `case id : return lit ;` — few distinct trigrams over many tokens => low ratio.
// Real code is more varied. Used to flag "skeletal" fragments (idiom-FP floor).
double trigramDiversity(const std::vector<std::string>& seq)
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
  int ai = -1;  // source index in a (LD.11: recover raw spelling at aligned position); -1 if absent
  int bj = -1;  // source index in b; -1 if absent
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
      raw.push_back({'=', a[i - 1], b[j - 1], static_cast<int>(i - 1), static_cast<int>(j - 1)});
      --i;
      --j;
    }
    else if (dp[i - 1][j] >= dp[i][j - 1])
    {
      raw.push_back({'-', a[i - 1], "", static_cast<int>(i - 1), -1});
      --i;
    }
    else
    {
      raw.push_back({'+', "", b[j - 1], -1, static_cast<int>(j - 1)});
      --j;
    }
  }
  while (i > 0)
  {
    --i;
    raw.push_back({'-', a[i], "", static_cast<int>(i), -1});
  }
  while (j > 0)
  {
    --j;
    raw.push_back({'+', "", b[j], -1, static_cast<int>(j)});
  }
  std::reverse(raw.begin(), raw.end());

  std::vector<DiffOp> out;
  for (std::size_t k = 0; k < raw.size(); ++k)
  {
    const bool delThenIns = raw[k].tag == '-' && k + 1 < raw.size() && raw[k + 1].tag == '+';
    const bool insThenDel = raw[k].tag == '+' && k + 1 < raw.size() && raw[k + 1].tag == '-';
    if (delThenIns)
    {
      out.push_back({'~', raw[k].a, raw[k + 1].b, raw[k].ai, raw[k + 1].bj});
      ++k;
    }
    else if (insThenDel)
    {
      out.push_back({'~', raw[k + 1].a, raw[k].b, raw[k + 1].ai, raw[k].bj});
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

bool hasSourceExt(const std::string& path)
{
  return isSourceFile(fs::path(path));
}

std::string toLowerCopy(std::string s)
{
  for (char& c : s)
  {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

// Case-insensitive substring match: one `--exclude thirdparty` catches
// ThirdParty / THIRDPARTY / thirdParty (vendored dirs are spelled every way).
bool isExcluded(const std::string& path, const std::vector<std::string>& excludes)
{
  const std::string lpath = toLowerCopy(path);
  for (const std::string& ex : excludes)
  {
    if (lpath.find(toLowerCopy(ex)) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

// In-tree generated code (protobuf *.pb.cc, Qt moc, flatbuffers, …) is real text
// but not human authorship: its repeated boilerplate fires both within-file and
// cross-file dup, and vendor-excludes miss it (it lives under gen/, not
// third_party/). Detect by a marker in the file header (first ~15 lines) and skip
// the whole file. Like SF.7's stateful comment strip, this is default dedup
// behaviour, not agent config. (#065)
bool hasGeneratedMarker(const std::string& src)
{
  // header = first ~1200 bytes (≈ top 15-20 lines); generated markers always sit at the very top
  std::string head = src.substr(0, std::min<std::size_t>(src.size(), 1200));
  for (char& c : head)
  {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  static const char* const kMarkers[] = {
      "@generated", "do not edit", "do not modify", "code generated by",
      "automatically generated", "machine generated", "qt meta object compiler",
      "generated by the protocol buffer"};
  for (const char* m : kMarkers)
  {
    if (head.find(m) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

// Tokenize + fragment a single source blob, tagging fragments with `label` as
// their file. Shared by snapshot mode (read from disk) and diff mode (read from
// `git show`). Appends to `out`.
void collectFromSource(const std::string& src, const std::string& label,
                       const Options& opt, std::vector<Fragment>& out)
{
  if (opt.skipGenerated && hasGeneratedMarker(src))
  {
    return;
  }
  // #069: vendored single-file libs (qcustomplot.cpp 30k lines, json.hpp, stb_*) are
  // real text but not author copy-paste — skip by curated basename + license header.
  if (opt.skipVendored && archcheck::scan::isVendoredFile(archcheck::scan::baseName(label), src))
  {
    return;
  }
  const std::vector<Token> toks = lex(src, opt.keepCalls);
  const std::vector<int> match = braceMatch(toks);
  const std::vector<std::string> lines = splitLines(src);
  collect(toks, match, 0, toks.size(), opt, label, lines, out);
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

// The score the active mode gates and ranks on: token-LCS in precise mode, else
// the chosen bag metric.
double gateScore(const std::string& metric, bool precise, const Pair& p)
{
  return precise ? p.lcs : (metric == "plain" ? p.plain : p.weighted);
}

// LD.10 — clone character. Labels a confirmed pair by WHAT diverges, reading the
// raw spellings the normalizer collapsed (identifiers -> "id", literals -> "lit"):
//   EXACT       identical token-for-token, identifiers and literals included
//   RENAMED     only local identifiers differ; structure intact
//   LITERAL     only literals differ; structure intact
//   MIXED       both identifiers and literals differ; structure intact
//   STRUCTURAL  the normalized streams themselves diverge (Type-3 proper)
// Renamed/literal copies keep an identical normalized stream (id->id, lit->lit),
// so seq-equality cleanly separates them from structural edits; raw is compared
// only at the placeholder positions, where the normalizer threw the spelling away.
const char* cloneType(const Fragment& a, const Fragment& b)
{
  if (a.seq != b.seq)  // normalized streams differ => structural edits
  {
    return "STRUCTURAL";
  }
  bool idDiff = false;
  bool litDiff = false;
  for (std::size_t i = 0; i < a.seq.size(); ++i)
  {
    if (a.rawSeq[i] == b.rawSeq[i])
    {
      continue;
    }
    idDiff = idDiff || a.seq[i] == "id";
    litDiff = litDiff || a.seq[i] == "lit";
  }
  if (!idDiff && !litDiff)
  {
    return "EXACT";
  }
  if (idDiff && litDiff)
  {
    return "MIXED";
  }
  return idDiff ? "RENAMED" : "LITERAL";
}

// Print one "ignored <kind>: a -> b; ..." line, or nothing when empty (LD.11).
void printIgnored(const char* kind, const std::vector<std::string>& entries)
{
  if (entries.empty())
  {
    return;
  }
  std::cout << "           ignored " << kind << ":";
  for (const std::string& e : entries)
  {
    std::cout << " " << e << ";";
  }
  std::cout << "\n";
}

// LD.11 — clone explanation. Tells the user WHY a pair matched by surfacing what
// normalization HID: at aligned (=) positions the normalized symbols are equal,
// but the raw spellings the normalizer collapsed (id->"id", lit->"lit") may
// differ — those are exactly the identifier renames and literal edits the plain
// diff cannot show. diffTokens aligns on the normalized seq; we read fa/fb.rawSeq
// at the matched indices to recover the spellings. Deduplicated, in order seen.
void explainPair(const Fragment& fa, const Fragment& fb, double similarity)
{
  std::vector<std::string> ids;
  std::vector<std::string> lits;
  std::size_t matched = 0;
  for (const DiffOp& o : diffTokens(fa.seq, fb.seq))
  {
    if (o.tag != '=')
    {
      continue;
    }
    ++matched;
    if (o.ai < 0 || o.bj < 0 || fa.rawSeq[o.ai] == fb.rawSeq[o.bj])
    {
      continue;
    }
    std::vector<std::string>& bucket = o.a == "lit" ? lits : ids;
    const std::string entry = fa.rawSeq[o.ai] + " -> " + fb.rawSeq[o.bj];
    if (std::find(bucket.begin(), bucket.end(), entry) == bucket.end())
    {
      bucket.push_back(entry);
    }
  }
  std::cout << "         explain: matched " << matched << " tokens, similarity "
            << static_cast<int>(similarity * 100.0 + 0.5) << "%\n";
  printIgnored("identifiers", ids);
  printIgnored("literals", lits);
}

// LD.14 — duplicated LOC over the reported pairs. A fragment is counted once even
// if it appears in several pairs (set-dedup); blocks = distinct clone fragments.
// Returns {clone LOC, block count}; density is clone LOC over scanned total LOC.
std::pair<std::size_t, std::size_t> cloneLocAndBlocks(const std::vector<Pair>& reported,
                                                      const std::vector<Fragment>& frags)
{
  std::unordered_set<std::size_t> seen;
  for (const Pair& p : reported)
  {
    seen.insert(p.a);
    seen.insert(p.b);
  }
  std::size_t loc = 0;
  for (std::size_t idx : seen)
  {
    loc += static_cast<std::size_t>(frags[idx].endLine - frags[idx].startLine + 1);
  }
  return {loc, seen.size()};
}

// LD.14 — clone-density baseline persistence for CI growth-gating. The file holds
// one line: "<density> <cloneLoc> <totalLoc> <blocks>". readBaselineDensity reads
// the leading density; returns false when the file is absent (first run = establish).
bool readBaselineDensity(const std::string& path, double& prior)
{
  std::ifstream in(path);
  return static_cast<bool>(in >> prior);
}

void writeBaselineDensity(const std::string& path, double density, std::size_t cloneLoc,
                          std::size_t totalLoc, std::size_t blocks)
{
  std::ofstream out(path);
  out.precision(12);  // round-trip cleanly so an unchanged tree reads back delta ~0 (no flaky gate)
  out << density << " " << cloneLoc << " " << totalLoc << " " << blocks << "\n";
}

// LD.16 — module = leading path segment of a fragment's relative file; a file at
// the scan root (no subdir) falls under "(root)". A clone whose two fragments live
// in different modules crosses an architectural boundary — the dangerous kind.
std::string moduleOf(const std::string& file)
{
  const std::size_t slash = file.find('/');
  return slash == std::string::npos ? std::string("(root)") : file.substr(0, slash);
}

void printUsage()
{
  std::cout << "usage: partial_duplication <root> [--min-tokens N] [--max-tokens N]\n"
               "                           [--threshold F] [--rare-df N] [--rare-df-pct P]\n"
               "                           [--min-shared N] [--metric weighted|plain]\n"
               "                           [--partial-precise] [--exclude substr]...\n"
               "                           [--no-keep-calls] [--no-skip-vendored] [--min-diversity F] [--top N]\n"
               "                           [--clone-baseline <path>] [--clone-growth-max <pct>]\n"
               "       partial_duplication --diff <sha>|<A>..<B> --repo <path>\n"
               "                           [--subpath <rel>] [common flags above]\n"
               "\n"
               "  snapshot mode: report near-duplicate fragment pairs in a tree.\n"
               "  LD.14 growth: --clone-baseline persists clone density; on the next run it\n"
               "  reports the delta and exits 1 if density grew past --clone-growth-max (CI gate).\n"
               "  diff mode (#054): for the commit(s) in --diff, report fragments\n"
               "  ADDED/MODIFIED in the commit that are Type-3 near-dups of code that\n"
               "  already existed at the parent. A hit = a missing-reuse edge born here.\n";
}

// ---------------------------------------------------------------------------
// Diff mode (#054) — git-backed, parser-free
// ---------------------------------------------------------------------------

// Run a git command in `repo` and capture stdout. stderr is sent to the shell's
// stderr (visible in logs). Returns false on popen failure.
bool runGit(const fs::path& repo, const std::string& gitArgs, std::string& out)
{
  out.clear();
  std::string cmd = "git -C '" + repo.string() + "' " + gitArgs;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr)
  {
    return false;
  }
  char buf[65536];
  std::size_t got = 0;
  while ((got = std::fread(buf, 1, sizeof(buf), pipe)) > 0)
  {
    out.append(buf, got);
  }
  pclose(pipe);
  return true;
}

// Resolve `spec` (a single rev, or "A..B") to parent and child revs.
// For a single rev C, parent = C^. Returns false if no parent (root commit).
bool resolveRange(const fs::path& repo, const std::string& spec,
                  std::string& parent, std::string& child)
{
  const std::size_t dots = spec.find("..");
  std::string parentSpec;
  std::string childSpec;
  if (dots != std::string::npos)
  {
    parentSpec = spec.substr(0, dots);
    childSpec = spec.substr(dots + 2);
  }
  else
  {
    childSpec = spec;
    parentSpec = spec + "^";
  }
  std::string out;
  if (!runGit(repo, "rev-parse --verify " + childSpec + " 2>/dev/null", out) || out.empty())
  {
    return false;
  }
  child = out.substr(0, out.find('\n'));
  if (!runGit(repo, "rev-parse --verify " + parentSpec + " 2>/dev/null", out) || out.empty())
  {
    return false;  // root commit: no parent, no baseline.
  }
  parent = out.substr(0, out.find('\n'));
  return true;
}

// Parse `git diff -U0` hunk headers ("@@ -a,b +c,d @@") -> set of line numbers
// ADDED in the NEW file. -U0 means each hunk's "+c,d" is exactly the added span.
std::unordered_set<int> addedLines(const std::string& diff)
{
  std::unordered_set<int> added;
  std::istringstream ss(diff);
  std::string line;
  while (std::getline(ss, line))
  {
    if (line.rfind("@@", 0) != 0)
    {
      continue;
    }
    const std::size_t plus = line.find('+');
    if (plus == std::string::npos)
    {
      continue;
    }
    std::size_t i = plus + 1;
    int start = 0;
    while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])) != 0)
    {
      start = start * 10 + (line[i] - '0');
      ++i;
    }
    int count = 1;
    if (i < line.size() && line[i] == ',')
    {
      ++i;
      count = 0;
      while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])) != 0)
      {
        count = count * 10 + (line[i] - '0');
        ++i;
      }
    }
    for (int ln = start; ln < start + count; ++ln)
    {
      added.insert(ln);
    }
  }
  return added;
}

// `git diff --numstat P..C` -> paths of changed source files (subpath-filtered).
std::vector<std::string> changedSourceFiles(const fs::path& repo, const std::string& parent,
                                            const std::string& child, const Options& opt)
{
  std::string out;
  // -M: detect renames. A renamed file is the SAME file moved, not new duplicated
  // code; without -M git reports it as add(new)+delete(old) and the "added" path
  // spuriously matches its own pre-rename copy in the baseline (FP-other, #061).
  runGit(repo, "diff -M --numstat " + parent + " " + child, out);
  std::vector<std::string> files;
  std::istringstream ss(out);
  std::string line;
  while (std::getline(ss, line))
  {
    // numstat: "<added>\t<deleted>\t<path>"  (binary files use "-")
    const std::size_t t1 = line.find('\t');
    if (t1 == std::string::npos)
    {
      continue;
    }
    const std::size_t t2 = line.find('\t', t1 + 1);
    if (t2 == std::string::npos)
    {
      continue;
    }
    std::string path = line.substr(t2 + 1);
    // renames show as "old => new" or "{a => b}/c"; --numstat keeps a path, but
    // be defensive and skip rename arrows we can't `git show`.
    if (path.find(" => ") != std::string::npos)
    {
      continue;
    }
    if (!hasSourceExt(path) || isExcluded(path, opt.excludes))
    {
      continue;
    }
    if (!opt.subpath.empty() && path.rfind(opt.subpath, 0) != 0)
    {
      continue;
    }
    files.push_back(path);
  }
  return files;
}

// Compute the effective rare-df cutoff and min-shared from the corpus size N,
// mirroring snapshot mode's logic (precise widens recall).
void effectiveRecall(const Options& opt, std::size_t N, std::size_t& effRareDf,
                     std::size_t& effMinShared)
{
  effRareDf = opt.precise ? std::max<std::size_t>(opt.rareDfCap, 8) : opt.rareDfCap;
  if (opt.rareDfPct > 0.0)
  {
    const std::size_t pctCut = static_cast<std::size_t>(static_cast<double>(N) * opt.rareDfPct / 100.0);
    effRareDf = std::max(effRareDf, pctCut);
  }
  effMinShared = opt.precise ? std::size_t{1} : opt.minSharedRare;
}

// Baseline = the whole parent tree (code that already existed). `git archive P
// | tar -x` into `tmp`, then scan it as a directory. Appends to `frags`.
void collectBaseline(const Options& opt, const std::string& parent, const fs::path& tmp,
                     std::vector<Fragment>& frags)
{
  std::error_code ec;
  fs::remove_all(tmp, ec);
  fs::create_directories(tmp, ec);
  std::string sink;
  runGit(opt.repo, "archive " + parent + " | tar -x -C '" + tmp.string() + "'", sink);

  std::vector<fs::path> files;
  const fs::path scanRoot = opt.subpath.empty() ? tmp : (tmp / opt.subpath);
  if (fs::is_directory(scanRoot))
  {
    for (const auto& e : fs::recursive_directory_iterator(scanRoot, ec))
    {
      if (e.is_regular_file() && isSourceFile(e.path())
          && !isExcluded(fs::relative(e.path(), tmp).string(), opt.excludes))
      {
        files.push_back(e.path());
      }
    }
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
    collectFromSource(ss.str(), fs::relative(p, tmp).string(), opt, frags);
  }
}

// Added fragments = function-scale fragments in each changed file's NEW content
// that overlap a line ADDED in this commit. Appends to `frags`, returns their
// indices.
std::vector<std::size_t> collectAdded(const Options& opt, const std::string& parent,
                                      const std::string& child, std::vector<Fragment>& frags)
{
  std::vector<std::size_t> addedIdx;
  for (const std::string& path : changedSourceFiles(opt.repo, parent, child, opt))
  {
    std::string newSrc;
    runGit(opt.repo, "show " + child + ":'" + path + "'", newSrc);
    if (newSrc.empty())
    {
      continue;
    }
    std::string diff;
    runGit(opt.repo, "diff -U0 " + parent + " " + child + " -- '" + path + "'", diff);
    const std::unordered_set<int> added = addedLines(diff);
    std::vector<Fragment> fileFrags;
    collectFromSource(newSrc, path, opt, fileFrags);
    for (Fragment& f : fileFrags)
    {
      bool touchesAdded = false;
      for (int ln = f.startLine; ln <= f.endLine && !touchesAdded; ++ln)
      {
        touchesAdded = added.count(ln) != 0;
      }
      if (touchesAdded)
      {
        addedIdx.push_back(frags.size());
        frags.push_back(std::move(f));
      }
    }
  }
  return addedIdx;
}

// Candidate cross pairs (added <-> baseline) sharing rare tokens. Only cross
// pairs matter: we want "did this commit re-create existing code", not
// added<->added self-similarity. Indexes baseline frags [0,baseCount) by rare
// token, then probes with each added fragment.
std::map<std::pair<std::size_t, std::size_t>, std::size_t>
crossCandidates(const std::vector<Fragment>& frags, std::size_t baseCount,
                const std::vector<std::size_t>& addedIdx,
                const std::unordered_map<std::string, int>& df, std::size_t effRareDf)
{
  std::unordered_map<std::string, std::vector<std::size_t>> postings;  // rare sym -> baseline frags
  for (std::size_t fi = 0; fi < baseCount; ++fi)
  {
    for (const auto& [sym, cnt] : frags[fi].bag)
    {
      if (static_cast<std::size_t>(df.at(sym)) <= effRareDf)
      {
        postings[sym].push_back(fi);
      }
    }
  }
  std::map<std::pair<std::size_t, std::size_t>, std::size_t> sharedRare;
  for (std::size_t ai : addedIdx)
  {
    std::unordered_map<std::size_t, std::size_t> hits;
    for (const auto& [sym, cnt] : frags[ai].bag)
    {
      const auto it = postings.find(sym);
      if (static_cast<std::size_t>(df.at(sym)) > effRareDf || it == postings.end())
      {
        continue;
      }
      for (std::size_t bi : it->second)
      {
        ++hits[bi];
      }
    }
    for (const auto& [bi, cnt] : hits)
    {
      sharedRare[{ai, bi}] = cnt;
    }
  }
  return sharedRare;
}

// Score the cross candidates, gate, then keep one best baseline match per added
// fragment (so a copied block matching a whole boilerplate family is one row).
std::vector<Pair> scoreCrossPairs(const Options& opt, const std::vector<Fragment>& frags,
                                  const std::map<std::pair<std::size_t, std::size_t>, std::size_t>& sharedRare,
                                  const std::unordered_map<std::string, double>& idf,
                                  std::size_t effMinShared)
{
  std::vector<Pair> reported;
  for (const auto& [pr, shared] : sharedRare)
  {
    // Self-match guard: when file F is edited, the edited block is "added" and
    // the parent baseline still holds F's *pre-edit* version of that same block
    // — a ~1.0 match that is "a function was edited", not copy-paste. Drop
    // same-file matches whose line range overlaps the added block. A
    // non-overlapping same-file block is a legit within-file duplicate, kept.
    const Fragment& fa = frags[pr.first];
    const Fragment& fb = frags[pr.second];
    if (shared < effMinShared
        || (fa.file == fb.file && fa.startLine <= fb.endLine && fb.startLine <= fa.endLine))
    {
      continue;
    }
    Pair p;
    p.a = pr.first;   // added
    p.b = pr.second;  // baseline
    p.sharedRare = shared;
    p.weighted = weightedJaccard(fa, fb, idf);
    p.plain = plainJaccard(fa, fb);
    p.line = lineOverlap(fa, fb);
    if (opt.precise)
    {
      p.lcs = lcsRatio(fa, fb);
    }
    if (gateScore(opt.metric, opt.precise, p) >= opt.simThreshold)
    {
      reported.push_back(p);
    }
  }
  std::sort(reported.begin(), reported.end(), [&opt](const Pair& l, const Pair& r)
            { return gateScore(opt.metric, opt.precise, l) > gateScore(opt.metric, opt.precise, r); });
  std::vector<Pair> best;
  std::unordered_set<std::size_t> seen;
  for (const Pair& p : reported)
  {
    if (seen.insert(p.a).second)
    {
      best.push_back(p);
    }
  }
  return best;
}

// Run diff mode for one commit range. Emits a machine-parseable summary line
// plus the hit list. Returns the number of partial hits reported.
std::size_t runDiffCommit(const Options& opt, const std::string& parent,
                          const std::string& child, const std::string& label)
{
  const fs::path tmp = fs::temp_directory_path()
                       / ("pdup_base_" + child.substr(0, std::min<std::size_t>(child.size(), 12)));
  std::vector<Fragment> frags;  // baseline fragments first, then added fragments.
  collectBaseline(opt, parent, tmp, frags);
  const std::size_t baseCount = frags.size();

  const std::vector<std::size_t> addedIdx = collectAdded(opt, parent, child, frags);

  std::error_code ec;
  fs::remove_all(tmp, ec);

  const std::size_t N = frags.size();
  if (addedIdx.empty() || N < 2)
  {
    std::cout << "commit=" << label << " added_frags=" << addedIdx.size()
              << " partial_hits=0 max_sim=0\n";
    return 0;
  }

  // 3. corpus stats over baseline + added (idf weights the whole comparison set).
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

  // 4-5. cross candidates (added <-> baseline) -> score -> best match per added.
  std::size_t effRareDf = 0;
  std::size_t effMinShared = 0;
  effectiveRecall(opt, N, effRareDf, effMinShared);
  const auto sharedRare = crossCandidates(frags, baseCount, addedIdx, df, effRareDf);
  const std::vector<Pair> best = scoreCrossPairs(opt, frags, sharedRare, idf, effMinShared);

  double maxSim = 0.0;
  for (const Pair& p : best)
  {
    maxSim = std::max(maxSim, gateScore(opt.metric, opt.precise, p));
  }
  const std::string gateName = opt.precise ? "token-LCS" : (opt.metric == "plain" ? "plain" : "weighted");
  std::cout << "commit=" << label << " added_frags=" << addedIdx.size()
            << " partial_hits=" << best.size() << " max_sim=" << maxSim << "\n";
  const std::size_t shown = std::min(best.size(), opt.top);
  for (std::size_t i = 0; i < shown; ++i)
  {
    const Pair& p = best[i];
    const Fragment& fa = frags[p.a];
    const Fragment& fb = frags[p.b];
    std::cout << "  [" << gateName << "=" << gateScore(opt.metric, opt.precise, p) << " " << cloneType(fa, fb) << "] ADDED "
              << fa.file << ":" << fa.startLine << "-" << fa.endLine << "  <->  BASE " << fb.file
              << ":" << fb.startLine << "-" << fb.endLine << "  tokens " << fa.tokenCount << "/"
              << fb.tokenCount << "  weighted=" << p.weighted << " plain=" << p.plain
              << " line=" << p.line;
    if (opt.precise)
    {
      std::cout << " lcs=" << p.lcs;
    }
    std::cout << " shared-rare=" << p.sharedRare << "\n";
    if (opt.precise)
    {
      explainPair(fa, fb, gateScore(opt.metric, opt.precise, p));  // LD.11
    }
  }
  return best.size();
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
    else if (arg == "--rare-df-pct")
    {
      opt.rareDfPct = std::stod(next());
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
    else if (arg == "--exclude")
    {
      opt.excludes.push_back(next());
    }
    else if (arg == "--min-diversity")
    {
      opt.minDiversity = std::stod(next());
    }
    else if (arg == "--keep-calls")
    {
      opt.keepCalls = true;
    }
    else if (arg == "--no-keep-calls")
    {
      opt.keepCalls = false;
    }
    else if (arg == "--no-skip-generated")
    {
      opt.skipGenerated = false;
    }
    else if (arg == "--no-skip-vendored")
    {
      opt.skipVendored = false;
    }
    else if (arg == "--diff")
    {
      opt.diffSpec = next();
    }
    else if (arg == "--repo")
    {
      opt.repo = next();
    }
    else if (arg == "--subpath")
    {
      opt.subpath = next();
    }
    else if (arg == "--clone-baseline")
    {
      opt.cloneBaseline = next();
    }
    else if (arg == "--clone-growth-max")
    {
      opt.cloneGrowthMax = std::stod(next());
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
  // token-LCS lives on a higher scale than bag-overlap: the normalized alphabet
  // (id/lit/keywords/operators) is tiny, so any two C++ bodies share a long
  // subsequence (~0.7 floor). Give precise mode its own default gate unless the
  // user pinned --threshold explicitly.
  if (opt.precise && !thresholdSet)
  {
    opt.simThreshold = 0.80;
  }

  // Default vendor excludes (#064): vendored libraries are real copy-paste but not
  // the project's own code, so they pollute the missing-reuse signal (e.g. two
  // copies of miniz/stb). Seed common spellings; isExcluded matches case-insensitive
  // substrings. User --exclude adds to (does not replace) these.
  for (const char* v : {"third_party", "thirdparty", "3rdparty", "/extern/",
                        "/external/", "/vendor/", "/deps/", "node_modules", "/submodules/"})
  {
    opt.excludes.emplace_back(v);
  }

  // --- diff mode (#054): --diff <sha>|<A>..<B> --repo <path> ---
  if (!opt.diffSpec.empty())
  {
    if (opt.repo.empty())
    {
      std::cerr << "error: --diff requires --repo <path>\n";
      return 2;
    }
    if (!fs::exists(opt.repo))
    {
      std::cerr << "error: repo not found: " << opt.repo << "\n";
      return 2;
    }
    std::string parent;
    std::string child;
    if (!resolveRange(opt.repo, opt.diffSpec, parent, child))
    {
      std::cerr << "error: cannot resolve --diff '" << opt.diffSpec
                << "' (root commit / bad rev?)\n";
      return 2;
    }
    runDiffCommit(opt, parent, child, child.substr(0, 12));
    return 0;
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

  // 1-2. collect fragments from every source file under root.
  std::vector<Fragment> frags;
  std::size_t fileCount = 0;
  std::size_t totalLoc = 0;  // LD.14: scanned source LOC = density denominator
  std::vector<fs::path> files;
  if (fs::is_directory(opt.root))
  {
    for (const auto& e : fs::recursive_directory_iterator(opt.root))
    {
      if (!e.is_regular_file() || !isSourceFile(e.path()))
      {
        continue;
      }
      if (!isExcluded(e.path().string(), opt.excludes))
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
    totalLoc += static_cast<std::size_t>(std::count(src.begin(), src.end(), '\n')) + 1;
    // single collection path (snapshot/baseline/diff all funnel here) → generated-skip guard applies
    const std::string rel = fs::relative(p, fs::is_directory(opt.root) ? opt.root : opt.root.parent_path()).string();
    collectFromSource(src, rel, opt, frags);
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
  // rare-token cutoff. Absolute df caps do not scale: on a 27k-fragment tree
  // df<=50 prunes every candidate, because even distinctive normalized tokens
  // recur in hundreds of fragments. --rare-df-pct makes the cutoff a fraction
  // of N (spike finding #4: rare_df must be relative). Floor keeps small repos
  // from collapsing the cutoff to ~0.
  std::size_t effRareDf = opt.precise ? std::max<std::size_t>(opt.rareDfCap, 8) : opt.rareDfCap;
  if (opt.rareDfPct > 0.0)
  {
    const std::size_t pctCut = static_cast<std::size_t>(static_cast<double>(N) * opt.rareDfPct / 100.0);
    effRareDf = std::max(effRareDf, pctCut);
  }
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
    if (opt.minDiversity > 0.0
        && std::min(frags[pr.first].diversity, frags[pr.second].diversity) < opt.minDiversity)
    {
      continue;  // skeletal fragment (dispatch switch / data table) — idiom-FP floor
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
            << "): " << reported.size() << "\n";
  const auto [cloneLoc, blocks] = cloneLocAndBlocks(reported, frags);
  const double density = totalLoc != 0 ? 100.0 * static_cast<double>(cloneLoc) / static_cast<double>(totalLoc) : 0.0;
  std::cout << "clone density: " << cloneLoc << " / " << totalLoc << " LOC (" << density
            << "%), " << blocks << " fragments in " << reported.size() << " pairs\n";
  std::size_t crossPairs = 0;
  for (const Pair& p : reported)
  {
    if (moduleOf(frags[p.a].file) != moduleOf(frags[p.b].file))
    {
      ++crossPairs;
    }
  }
  std::cout << "cross-module: " << crossPairs << " of " << reported.size()
            << " pairs cross a module boundary\n\n";

  int rc = 0;
  if (!opt.cloneBaseline.empty())
  {
    double prior = 0.0;
    if (readBaselineDensity(opt.cloneBaseline, prior))
    {
      const double delta = density - prior;
      std::cout << "clone growth: " << prior << "% -> " << density << "% (delta "
                << (delta >= 0.0 ? "+" : "") << delta << "%)\n";
      if (delta > opt.cloneGrowthMax + 1e-9)  // epsilon absorbs float round-trip noise
      {
        std::cout << "GATE: clone density grew by " << delta << "% > max " << opt.cloneGrowthMax
                  << "% — fail\n";
        rc = 1;
      }
    }
    else
    {
      std::cout << "clone baseline: established at " << density << "% (no prior — no gate)\n";
    }
    writeBaselineDensity(opt.cloneBaseline, density, cloneLoc, totalLoc, blocks);
    std::cout << "\n";
  }

  const std::size_t shown = std::min(reported.size(), opt.top);
  for (std::size_t i = 0; i < shown; ++i)
  {
    const Pair& p = reported[i];
    const Fragment& fa = frags[p.a];
    const Fragment& fb = frags[p.b];
    std::cout << "[" << gateName << "=" << scoreOf(p) << " " << cloneType(fa, fb) << "]  " << fa.file << ":"
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
      explainPair(fa, fb, scoreOf(p));  // LD.11: matched tokens, similarity, ignored renames/literals
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

  return rc;  // LD.14: non-zero when clone-growth gate tripped
}
