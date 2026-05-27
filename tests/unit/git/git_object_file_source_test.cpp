#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "archcheck/git/git_object_file_source.h"
#include "archcheck/scan/disk_file_source.h"

namespace
{

namespace fs = std::filesystem;

struct TempRepo
{
  fs::path path;
  TempRepo()
  {
    const auto tmpl = (fs::temp_directory_path() / "archcheck-gobjs-XXXXXX").string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    REQUIRE(mkdtemp(buf.data()) != nullptr);
    path = fs::path{buf.data()};
  }
  ~TempRepo()
  {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  TempRepo(const TempRepo &) = delete;
  TempRepo &operator=(const TempRepo &) = delete;
};

int runIn(const fs::path &cwd, const std::string &cmd)
{
  const std::string full = "cd '" + cwd.string() + "' && " + cmd + " >/dev/null 2>&1";
  return std::system(full.c_str());
}

void writeFile(const fs::path &p, std::string_view content)
{
  fs::create_directories(p.parent_path());
  std::ofstream f(p);
  f << content;
}

void seed(const fs::path &p)
{
  REQUIRE(runIn(p, "git init -q -b main") == 0);
  REQUIRE(runIn(p, "git config user.email test@example") == 0);
  REQUIRE(runIn(p, "git config user.name test") == 0);
  REQUIRE(runIn(p, "git config commit.gpgsign false") == 0);
}

void commitAll(const fs::path &p, const std::string &msg)
{
  REQUIRE(runIn(p, "git add . && git commit -qm '" + msg + "'") == 0);
}

} // namespace

TEST_CASE("GitObjectFileSource: list() filters by extension, sorted, POSIX paths", "[git][file_source][unit]")
{
  TempRepo repo;
  seed(repo.path);
  writeFile(repo.path / "a.h", "// a\n");
  writeFile(repo.path / "sub" / "b.cpp", "// b\n");
  writeFile(repo.path / "README.md", "docs\n");        // must be filtered out
  writeFile(repo.path / "config.yaml", "x: 1\n");      // filtered out
  writeFile(repo.path / "build" / "x.cpp", "// x\n");  // excluded dir
  commitAll(repo.path, "init");

  archcheck::git::GitObjectFileSource src(repo.path, "HEAD");
  REQUIRE(src.valid());
  const auto files = src.list();
  REQUIRE(files.size() == 2);
  REQUIRE(files[0].path == "a.h");
  REQUIRE(files[1].path == "sub/b.cpp");
}

TEST_CASE("GitObjectFileSource: read() returns blob content byte-for-byte", "[git][file_source][unit]")
{
  TempRepo repo;
  seed(repo.path);
  const std::string contentA = "// header a\n#include \"b.h\"\n";
  const std::string contentB = "int x = 42;\n// trailing line without newline at EOF";
  writeFile(repo.path / "a.h", contentA);
  writeFile(repo.path / "b.h", contentB);
  commitAll(repo.path, "init");

  archcheck::git::GitObjectFileSource src(repo.path, "HEAD");
  REQUIRE(src.read("a.h") == contentA);
  REQUIRE(src.read("b.h") == contentB);
}

TEST_CASE("GitObjectFileSource: parity with DiskFileSource on the same commit",
          "[git][file_source][unit][parity]")
{
  TempRepo repo;
  seed(repo.path);
  writeFile(repo.path / "a.h", "#include \"b.h\"\n");
  writeFile(repo.path / "b.h", "// b\n");
  writeFile(repo.path / "sub" / "c.cpp", "#include \"../a.h\"\n");
  commitAll(repo.path, "v1");

  archcheck::scan::DiskFileSource disk(repo.path);
  archcheck::git::GitObjectFileSource mem(repo.path, "HEAD");

  const auto diskList = disk.list();
  const auto memList = mem.list();
  REQUIRE(diskList.size() == memList.size());
  for (std::size_t i = 0; i < diskList.size(); ++i)
  {
    REQUIRE(diskList[i].path == memList[i].path);
    REQUIRE(disk.read(diskList[i].path) == mem.read(memList[i].path));
  }
}

TEST_CASE("GitObjectFileSource: read() on non-existent path returns empty",
          "[git][file_source][unit]")
{
  TempRepo repo;
  seed(repo.path);
  writeFile(repo.path / "a.h", "// a\n");
  commitAll(repo.path, "v1");

  archcheck::git::GitObjectFileSource src(repo.path, "HEAD");
  REQUIRE(src.read("does-not-exist.h").empty());
  // sanity: the existing file still works on the same long-lived child
  REQUIRE(src.read("a.h") == "// a\n");
}
