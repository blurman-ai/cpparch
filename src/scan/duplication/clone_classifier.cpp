#include "archcheck/scan/duplication/clone_classifier.h"

#include <algorithm>

namespace archcheck::scan::duplication
{

std::vector<DiffOp> diffTokens(const std::vector<std::string> &a, const std::vector<std::string> &b)
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

const char *cloneType(const Fragment &a, const Fragment &b)
{
  if (a.seq != b.seq)
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

} // namespace archcheck::scan::duplication
