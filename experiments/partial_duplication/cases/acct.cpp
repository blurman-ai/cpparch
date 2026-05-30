// Original procedures. The diverged copies live in acct_copy.cpp.
#include <cstddef>
#include <vector>

namespace acct
{

int offsetFor(std::size_t i);
extern int total;

// ORIGINAL of contrast cases A and B.
int computeScore(const std::vector<int>& xs, int base)
{
  int acc = base;
  for (std::size_t i = 0; i < xs.size(); ++i)
  {
    acc = acc + xs[i] * 2;
    acc = acc + offsetFor(i);
    acc = acc + (xs[i] + base);
    total = total + acc;
  }
  return acc + base;
}

// Unrelated helper — should never pair with computeScore.
void formatRow(std::vector<int>& cells, int pad)
{
  while (cells.size() < 8)
  {
    cells.push_back(pad);
  }
  cells.front() = cells.back();
}

}  // namespace acct
