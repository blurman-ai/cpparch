#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>
#include <vector>

#include "archcheck/scan/duplication/duplication_scanner.h"

using namespace archcheck::scan::duplication;

namespace
{
// Identical >30-token block — copy-pasted into both "main" files of a pair.
const std::string kBlock = R"(
void processData(const std::vector<int>& data) {
    for (int i = 0; i < data.size(); i++) {
        int value = data[i];
        if (value > threshold) {
            results.push_back(value);
            count++;
        }
    }
    return count;
}
)";

// Five distinct filler fragments so document frequency (and thus IDF weight)
// is non-zero — without them a 2-fragment corpus scores every shared token at
// log(N/df)=log(2/2)=0 and no pair ever forms (see #070 IDF note).
std::vector<std::pair<std::string, std::string>> withFillers(const std::string &pathA, const std::string &pathB)
{
  return {
      {pathA, kBlock},
      {pathB, kBlock},
      {"filler1.cpp", R"(void f1(){for(int i=0;i<10;i++){int x=i*2;int y=x+1;int z=y+3;use(x,y,z);log(z);}})"},
      {"filler2.cpp", R"(void f2(){while(go){int a=next();if(a<0)break;int b=a*5;int c=b-2;emit(a,b,c);tick();}})"},
      {"filler3.cpp", R"(void f3(){for(auto&it:items){int p=it.k;int q=p+7;int r=q*q;if(r>cap)drop(p,q,r);}})"},
      {"filler4.cpp", R"(void f4(){int s=0;for(int n=0;n<50;n++){s+=n;if(s%3==0)flush(s);else hold(n);}done(s);})"},
      {"filler5.cpp", R"(void f5(){std::vector<int> v;for(int u=0;u<20;u++){v.add(u*u);trim(v);}sort(v);})"},
  };
}

bool hasPairBetween(const ScanResult &r, const std::string &fileA, const std::string &fileB)
{
  for (const auto &p : r.pairs)
  {
    const std::string &fa = r.fragments[p.a].file;
    const std::string &fb = r.fragments[p.b].file;
    if ((fa == fileA && fb == fileB) || (fa == fileB && fb == fileA))
    {
      return true;
    }
  }
  return false;
}

ScannerOptions baseOpts(bool enablePathGuards)
{
  ScannerOptions opts;
  opts.simThreshold = 0.60;
  opts.enablePathGuards = enablePathGuards;
  return opts;
}
} // namespace

// NOTE: P0.7 (platform-twin) and P0.8 (perf-variant) guards were removed — a
// path guard cannot tell a genuinely-divergent platform body from a byte-identical
// POSIX helper (OS::sleep/truncateFile), and the latter is a real TP. Only the
// generated-file guard (P0.9) remains: generated output has no human to refactor.

TEST_CASE("P0.9 disabled: generated-file pair still forms", "[duplication][path-guards]")
{
  // Sanity: the guard — not low similarity — is what removes the pair.
  const auto files = withFillers("build/proto/message.pb.cc", "build/proto/other.pb.cc");
  const auto result = scanForDuplication(files, baseOpts(false));
  REQUIRE(hasPairBetween(result, "build/proto/message.pb.cc", "build/proto/other.pb.cc"));
}

TEST_CASE("P0.9: generated files are suppressed", "[duplication][path-guards]")
{
  const auto files = withFillers("build/proto/message.pb.cc", "build/proto/other.pb.cc");
  const auto result = scanForDuplication(files, baseOpts(true));
  REQUIRE_FALSE(hasPairBetween(result, "build/proto/message.pb.cc", "build/proto/other.pb.cc"));
}

TEST_CASE("P0.9: platform twins are NO LONGER suppressed (identical = TP)", "[duplication][path-guards]")
{
  // Byte-identical code under different OS dirs is consolidatable duplication.
  // The removed P0.7 used to drop this; now it must be reported.
  const auto files = withFillers("src/hidapi/mac/hid.c", "src/hidapi/win/hid.c");
  const auto result = scanForDuplication(files, baseOpts(true));
  REQUIRE(hasPairBetween(result, "src/hidapi/mac/hid.c", "src/hidapi/win/hid.c"));
}

TEST_CASE("P0.9: real copy-paste in ordinary paths is kept", "[duplication][path-guards]")
{
  // Control: ordinary paths, guard must NOT over-suppress.
  const auto files = withFillers("src/app/cmd_move_mask.cpp", "src/app/cmd_scroll.cpp");
  const auto result = scanForDuplication(files, baseOpts(true));
  REQUIRE(hasPairBetween(result, "src/app/cmd_move_mask.cpp", "src/app/cmd_scroll.cpp"));
}

namespace
{
// Two more distinct >30-token blocks so a file can hold several fragments.
const std::string kBlock2 = R"(
void renderFrame(const Buffer& buf) {
    for (int i = 0; i < buf.height; i++) {
        Row row = buf.rows[i];
        if (row.dirty) {
            blit(row.pixels, row.width);
            markClean(row);
        }
    }
    return;
}
)";

const std::string kBlock3 = R"(
int accumulate(const std::vector<Sample>& xs) {
    int total = 0;
    for (const auto& s : xs) {
        if (s.valid) {
            total += s.weight * s.value;
            audit(s.id);
        }
    }
    return total;
}
)";

std::vector<std::pair<std::string, std::string>> fillersOnly()
{
  return {
      {"filler1.cpp", R"(void f1(){for(int i=0;i<10;i++){int x=i*2;int y=x+1;int z=y+3;use(x,y,z);log(z);}})"},
      {"filler2.cpp", R"(void f2(){while(go){int a=next();if(a<0)break;int b=a*5;int c=b-2;emit(a,b,c);tick();}})"},
      {"filler3.cpp", R"(void f3(){for(auto&it:items){int p=it.k;int q=p+7;int r=q*q;if(r>cap)drop(p,q,r);}})"},
      {"filler4.cpp", R"(void f4(){int s=0;for(int n=0;n<50;n++){s+=n;if(s%3==0)flush(s);else hold(n);}done(s);})"},
      {"filler5.cpp", R"(void f5(){std::vector<int> v;for(int u=0;u<20;u++){v.add(u*u);trim(v);}sort(v);})"},
  };
}
} // namespace

TEST_CASE("P0.2: whole-file clone is counted and its pairs suppressed", "[duplication][whole-file]")
{
  // twinA and twinB share BOTH their functions → whole-file clone.
  auto files = fillersOnly();
  files.push_back({"src/twinA.cpp", kBlock + "\n\n" + kBlock2});
  files.push_back({"src/twinB.cpp", kBlock + "\n\n" + kBlock2});

  const auto result = scanForDuplication(files, baseOpts(true));

  REQUIRE(result.wholeFileClones == 1);
  REQUIRE_FALSE(hasPairBetween(result, "src/twinA.cpp", "src/twinB.cpp"));
}

TEST_CASE("P0.2: partial file overlap is NOT a whole-file clone", "[duplication][whole-file]")
{
  // twinC and twinD share only ONE of two functions → real copy-paste, kept.
  auto files = fillersOnly();
  files.push_back({"src/twinC.cpp", kBlock + "\n\n" + kBlock2});
  files.push_back({"src/twinD.cpp", kBlock + "\n\n" + kBlock3});

  const auto result = scanForDuplication(files, baseOpts(true));

  REQUIRE(result.wholeFileClones == 0);
  REQUIRE(hasPairBetween(result, "src/twinC.cpp", "src/twinD.cpp"));
}

TEST_CASE("P0.2: whole-file guard disabled keeps the pairs", "[duplication][whole-file]")
{
  auto files = fillersOnly();
  files.push_back({"src/twinA.cpp", kBlock + "\n\n" + kBlock2});
  files.push_back({"src/twinB.cpp", kBlock + "\n\n" + kBlock2});

  ScannerOptions opts = baseOpts(true);
  opts.enableWholeFileGuard = false;

  const auto result = scanForDuplication(files, opts);

  REQUIRE(result.wholeFileClones == 0);
  REQUIRE(hasPairBetween(result, "src/twinA.cpp", "src/twinB.cpp"));
}
