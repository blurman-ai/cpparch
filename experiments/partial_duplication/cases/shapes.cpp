// CASE C — two functions that share a control SHAPE (a for-loop with an inner
// if) but are not copies of each other: different operations, different token
// mix. Weighted overlap must stay below threshold — similar form is not a copy.
#include <cstddef>
#include <string>
#include <vector>

namespace shapes
{

// String accumulation with conditional append.
std::string joinNonEmpty(const std::vector<std::string>& parts, char sep)
{
  std::string result;
  for (std::size_t i = 0; i < parts.size(); ++i)
  {
    if (parts[i].empty())
    {
      continue;
    }
    if (!result.empty())
    {
      result.push_back(sep);
    }
    result.append(parts[i]);
  }
  return result;
}

// Numeric min-tracking — same loop+if skeleton, different work entirely.
int smallestPositive(const std::vector<int>& xs)
{
  int best = -1;
  for (std::size_t i = 0; i < xs.size(); ++i)
  {
    if (xs[i] <= 0)
    {
      continue;
    }
    if (best < 0 || xs[i] < best)
    {
      best = xs[i];
    }
  }
  return best;
}

}  // namespace shapes
