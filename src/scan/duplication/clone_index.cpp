#include "archcheck/scan/duplication/clone_index.h"

#include <algorithm>
#include <cmath>

#include "archcheck/scan/duplication/fragmenter.h"

namespace archcheck::scan::duplication
{

CloneIndex buildIndex(const std::vector<Fragment> &fragments, const IndexOptions &opts)
{
  CloneIndex idx;
  const std::size_t N = fragments.size();

  // 1. Compute document frequency (df) for all tokens
  for (const auto &frag : fragments)
  {
    for (const auto &[sym, _] : frag.bag)
    {
      ++idx.df[sym];
    }
  }

  // 2. Compute IDF weights
  for (auto &[sym, d] : idx.df)
  {
    idx.idf[sym] = std::log(static_cast<double>(N) / static_cast<double>(d));
  }

  // 3. Determine effective rare-token cutoff
  std::size_t effRareDf = opts.rareDfCap;
  if (opts.rareDfPct > 0.0)
  {
    const std::size_t pctCap = static_cast<std::size_t>(N * opts.rareDfPct / 100.0);
    effRareDf = std::max(opts.rareDfCap, pctCap);
  }

  // 4. Build inverted index on rare tokens
  for (std::size_t fi = 0; fi < N; ++fi)
  {
    for (const auto &[sym, cnt] : fragments[fi].bag)
    {
      if (static_cast<std::size_t>(idx.df[sym]) <= effRareDf)
      {
        idx.postings[sym].push_back(fi);
      }
    }
  }

  // 5. Find candidate pairs by cross-matching rare tokens
  for (const auto &[sym, list] : idx.postings)
  {
    for (std::size_t x = 0; x < list.size(); ++x)
    {
      for (std::size_t y = x + 1; y < list.size(); ++y)
      {
        const std::size_t a = list[x];
        const std::size_t b = list[y];
        const auto key = a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        ++idx.sharedRare[key];
      }
    }
  }

  // 6. Filter by minSharedRare and minDiversity
  auto it = idx.sharedRare.begin();
  while (it != idx.sharedRare.end())
  {
    const auto [pair, shared] = *it;
    const auto [a, b] = pair;

    if (shared < opts.minSharedRare)
    {
      it = idx.sharedRare.erase(it);
      continue;
    }

    if (opts.minDiversity > 0.0 && std::min(fragments[a].diversity, fragments[b].diversity) < opts.minDiversity)
    {
      it = idx.sharedRare.erase(it);
      continue;
    }

    ++it;
  }

  return idx;
}

} // namespace archcheck::scan::duplication
