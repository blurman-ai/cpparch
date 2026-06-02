#include "archcheck/scan/duplication/similarity.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace archcheck::scan::duplication
{

double weightedJaccard(const Fragment &a, const Fragment &b, const std::unordered_map<std::string, double> &idf)
{
  double num = 0.0;
  double den = 0.0;
  std::unordered_set<std::string> keys;
  for (const auto &[k, v] : a.bag)
  {
    keys.insert(k);
  }
  for (const auto &[k, v] : b.bag)
  {
    keys.insert(k);
  }
  for (const std::string &k : keys)
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

double plainJaccard(const Fragment &a, const Fragment &b)
{
  int num = 0;
  int den = 0;
  std::unordered_set<std::string> keys;
  for (const auto &[k, v] : a.bag)
  {
    keys.insert(k);
  }
  for (const auto &[k, v] : b.bag)
  {
    keys.insert(k);
  }
  for (const std::string &k : keys)
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

double lineOverlap(const Fragment &a, const Fragment &b)
{
  if (a.normLines.empty() || b.normLines.empty())
  {
    return 0.0;
  }
  std::size_t inter = 0;
  const auto &small = a.normLines.size() < b.normLines.size() ? a.normLines : b.normLines;
  const auto &big = a.normLines.size() < b.normLines.size() ? b.normLines : a.normLines;
  for (const std::string &s : small)
  {
    if (big.count(s) != 0)
    {
      ++inter;
    }
  }
  const std::size_t uni = a.normLines.size() + b.normLines.size() - inter;
  return uni > 0 ? static_cast<double>(inter) / uni : 0.0;
}

std::size_t lcsLength(const std::vector<std::string> &a, const std::vector<std::string> &b)
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

double lcsRatio(const Fragment &a, const Fragment &b)
{
  const std::size_t lcs = lcsLength(a.seq, b.seq);
  const std::size_t len_sum = a.seq.size() + b.seq.size();
  return len_sum > 0 ? (2.0 * lcs) / len_sum : 0.0;
}

} // namespace archcheck::scan::duplication
