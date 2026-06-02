#include "archcheck/scan/duplication/clone_index.h"

#include <algorithm>
#include <cmath>

#include "archcheck/scan/duplication/fragmenter.h"

namespace archcheck::scan::duplication
{

namespace
{
void computeDocumentFrequency(const std::vector<Fragment> &fragments, std::unordered_map<std::string, int> &df)
{
  for (const auto &frag : fragments)
  {
    for (const auto &[sym, _] : frag.bag)
    {
      ++df[sym];
    }
  }
}

void computeIdfWeights(const std::size_t fragmentCount, std::unordered_map<std::string, int> &df,
                       std::unordered_map<std::string, double> &idf)
{
  for (auto &[sym, d] : df)
  {
    idf[sym] = std::log(static_cast<double>(fragmentCount) / static_cast<double>(d));
  }
}

std::size_t getEffectiveRareDf(const std::size_t fragmentCount, const IndexOptions &opts)
{
  std::size_t effRareDf = opts.rareDfCap;
  if (opts.rareDfPct > 0.0)
  {
    const std::size_t pctCap = static_cast<std::size_t>(fragmentCount * opts.rareDfPct / 100.0);
    effRareDf = std::max(opts.rareDfCap, pctCap);
  }
  return effRareDf;
}

void buildRareTokenIndex(const std::vector<Fragment> &fragments, std::size_t effRareDf,
                         const std::unordered_map<std::string, int> &df,
                         std::unordered_map<std::string, std::vector<std::size_t>> &postings)
{
  for (std::size_t fi = 0; fi < fragments.size(); ++fi)
  {
    for (const auto &[sym, cnt] : fragments[fi].bag)
    {
      if (static_cast<std::size_t>(df.at(sym)) <= effRareDf)
      {
        postings[sym].push_back(fi);
      }
    }
  }
}

void findCandidatePairs(const std::unordered_map<std::string, std::vector<std::size_t>> &postings,
                        std::map<std::pair<std::size_t, std::size_t>, std::size_t> &sharedRare)
{
  for (const auto &[sym, list] : postings)
  {
    for (std::size_t x = 0; x < list.size(); ++x)
    {
      for (std::size_t y = x + 1; y < list.size(); ++y)
      {
        const auto key = std::make_pair(std::min(list[x], list[y]), std::max(list[x], list[y]));
        ++sharedRare[key];
      }
    }
  }
}

bool shouldKeepPair(const std::pair<std::size_t, std::size_t> &pair, std::size_t shared,
                    const std::vector<Fragment> &fragments, const IndexOptions &opts)
{
  if (shared < opts.minSharedRare)
  {
    return false;
  }
  if (opts.minDiversity > 0.0)
  {
    const auto [a, b] = pair;
    if (std::min(fragments[a].diversity, fragments[b].diversity) < opts.minDiversity)
    {
      return false;
    }
  }
  return true;
}

void filterCandidatePairs(std::map<std::pair<std::size_t, std::size_t>, std::size_t> &sharedRare,
                          const std::vector<Fragment> &fragments, const IndexOptions &opts)
{
  auto it = sharedRare.begin();
  while (it != sharedRare.end())
  {
    if (shouldKeepPair(it->first, it->second, fragments, opts))
    {
      ++it;
    }
    else
    {
      it = sharedRare.erase(it);
    }
  }
}
} // namespace

CloneIndex buildIndex(const std::vector<Fragment> &fragments, const IndexOptions &opts)
{
  CloneIndex idx;
  const std::size_t N = fragments.size();

  computeDocumentFrequency(fragments, idx.df);
  computeIdfWeights(N, idx.df, idx.idf);
  std::size_t effRareDf = getEffectiveRareDf(N, opts);
  buildRareTokenIndex(fragments, effRareDf, idx.df, idx.postings);
  findCandidatePairs(idx.postings, idx.sharedRare);
  filterCandidatePairs(idx.sharedRare, fragments, opts);

  return idx;
}

} // namespace archcheck::scan::duplication
