// Background corpus. Not a contrast case — its only job is to give the idf
// model a realistic spread of structural tokens (for/while/if/switch) so that
// rarity weighting behaves like it would on a real tree, not on 5 fragments in
// isolation. A single switch here makes "switch"/"case" non-rare, which is the
// whole point of weighting in case D.
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace filler
{

double mean(const std::vector<double>& xs)
{
  if (xs.empty())
  {
    return 0.0;
  }
  double s = 0.0;
  for (double x : xs)
  {
    s += x;
  }
  return s / static_cast<double>(xs.size());
}

std::size_t countMatches(const std::string& text, char target)
{
  std::size_t hits = 0;
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    if (text[i] == target)
    {
      ++hits;
    }
  }
  return hits;
}

void normalizeWeights(std::map<std::string, double>& weights)
{
  double total = 0.0;
  for (const auto& kv : weights)
  {
    total += kv.second;
  }
  if (total <= 0.0)
  {
    return;
  }
  for (auto& kv : weights)
  {
    kv.second /= total;
  }
}

int categorize(int code)
{
  switch (code / 100)
  {
    case 1: return 0;
    case 2: return 1;
    case 3: return 2;
    case 4: return 3;
    case 5: return 4;
    default: return -1;
  }
}

bool isSorted(const std::vector<int>& xs)
{
  for (std::size_t i = 1; i < xs.size(); ++i)
  {
    if (xs[i - 1] > xs[i])
    {
      return false;
    }
  }
  return true;
}

std::string repeat(const std::string& unit, int times)
{
  std::string out;
  int n = 0;
  while (n < times)
  {
    out += unit;
    ++n;
  }
  return out;
}

}  // namespace filler
