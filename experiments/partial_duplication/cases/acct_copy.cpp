// Diverged copies of acct::computeScore. Both were renamed (every identifier
// differs) so a line-based pass, even with Type-2 normalization, sees almost
// nothing. The token-overlap pass must still pair them with computeScore.
#include <cstddef>
#include <vector>

namespace billing
{

int offsetFor(std::size_t k);
extern int grand;

// CASE A — copy of computeScore with every "+" flipped to "-" and all
// identifiers renamed. Same procedure, diverged operator in every line.
int computeDelta(const std::vector<int>& ys, int seed)
{
  int sum = seed;
  for (std::size_t k = 0; k < ys.size(); ++k)
  {
    sum = sum - ys[k] * 2;
    sum = sum - offsetFor(k);
    sum = sum - (ys[k] - seed);
    grand = grand - sum;
  }
  return sum - seed;
}

// CASE B — copy of computeScore with identifiers renamed AND two lines
// inserted (a guard var and an early-skip). Insertion must not break the match.
int computeScoreV2(const std::vector<int>& zs, int origin)
{
  int agg = origin;
  int guard = 0;
  for (std::size_t m = 0; m < zs.size(); ++m)
  {
    if (zs[m] < guard) { continue; }
    agg = agg + zs[m] * 2;
    agg = agg + offsetFor(m);
    agg = agg + (zs[m] + origin);
    guard = guard + agg;
  }
  return agg + origin;
}

}  // namespace billing
