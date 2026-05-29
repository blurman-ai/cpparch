// Spike #043 — measure libclang cost per translation unit on a real project.
//
// Usage: clang_perf <path/to/compile_commands.json>
//
// Output: one CSV line per TU on stdout (tu_index,wall_ms,inclusions),
// then a final summary line "SUMMARY total_ms=... tus=... median_tu_ms=...".
// Peak RSS is reported separately by /usr/bin/time -v.

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Index.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

struct TuResult
{
  std::string file;
  double wall_ms;
  unsigned inclusions;
};

void inclusion_visitor(CXFile, CXSourceLocation *, unsigned, CXClientData data)
{
  auto *count = static_cast<unsigned *>(data);
  ++(*count);
}

} // namespace

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    std::fprintf(stderr, "usage: %s <compile_commands.json dir>\n", argv[0]);
    return 2;
  }

  // The path argument is the *directory* containing compile_commands.json
  // (libclang requirement), not the file itself.
  std::string db_dir = argv[1];

  CXCompilationDatabase_Error db_err;
  CXCompilationDatabase db = clang_CompilationDatabase_fromDirectory(db_dir.c_str(), &db_err);
  if (db_err != CXCompilationDatabase_NoError)
  {
    std::fprintf(stderr, "failed to load compile_commands.json from %s (err=%d)\n", db_dir.c_str(), db_err);
    return 2;
  }

  CXCompileCommands cmds = clang_CompilationDatabase_getAllCompileCommands(db);
  unsigned cmd_count = clang_CompileCommands_getSize(cmds);
  std::fprintf(stderr, "[spike] %u translation units in DB\n", cmd_count);

  CXIndex idx = clang_createIndex(/*excludeDeclarationsFromPCH=*/0, /*displayDiagnostics=*/0);

  std::vector<TuResult> results;
  results.reserve(cmd_count);

  auto t_total_start = std::chrono::steady_clock::now();

  for (unsigned i = 0; i < cmd_count; ++i)
  {
    CXCompileCommand cmd = clang_CompileCommands_getCommand(cmds, i);
    CXString cx_file = clang_CompileCommand_getFilename(cmd);
    std::string tu_file = clang_getCString(cx_file);
    clang_disposeString(cx_file);

    unsigned argn = clang_CompileCommand_getNumArgs(cmd);
    std::vector<std::string> arg_storage;
    arg_storage.reserve(argn);
    std::vector<const char *> arg_ptrs;
    arg_ptrs.reserve(argn);
    for (unsigned a = 0; a < argn; ++a)
    {
      CXString s = clang_CompileCommand_getArg(cmd, a);
      arg_storage.emplace_back(clang_getCString(s));
      clang_disposeString(s);
    }
    // Drop argv[0] (compiler name) and the source file itself — libclang
    // expects only the flags array; it figures out the source from the
    // last unnamed positional and CXTranslationUnit param.
    for (size_t a = 1; a < arg_storage.size(); ++a)
    {
      if (arg_storage[a] == tu_file)
      {
        continue;
      }
      // Strip output-control flags that libclang doesn't need and that
      // can point at non-existent files.
      if (arg_storage[a] == "-o" && a + 1 < arg_storage.size())
      {
        ++a;
        continue;
      }
      if (arg_storage[a] == "-c")
      {
        continue;
      }
      arg_ptrs.push_back(arg_storage[a].c_str());
    }

    // We need the compile-command's working directory too, otherwise
    // relative -I paths in the DB resolve from the spike's CWD.
    CXString cx_wd = clang_CompileCommand_getDirectory(cmd);
    std::string wd = clang_getCString(cx_wd);
    clang_disposeString(cx_wd);
    // chdir is the simplest way to honour the per-command working dir.
    // The spike is single-threaded; thread-safety isn't a concern.
    if (chdir(wd.c_str()) != 0)
    {
      std::fprintf(stderr, "  [skip] cannot chdir to %s\n", wd.c_str());
      continue;
    }

    auto t0 = std::chrono::steady_clock::now();
    CXTranslationUnit tu =
        clang_parseTranslationUnit(idx, tu_file.c_str(), arg_ptrs.data(), static_cast<int>(arg_ptrs.size()), nullptr, 0,
                                   CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_KeepGoing |
                                       CXTranslationUnit_SkipFunctionBodies // we only need includes/decls, not bodies
        );

    unsigned inclusions = 0;
    if (tu != nullptr)
    {
      clang_getInclusions(tu, &inclusion_visitor, &inclusions);
    }
    auto t1 = std::chrono::steady_clock::now();

    double wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    results.push_back({tu_file, wall_ms, inclusions});

    std::printf("%u,%.2f,%u,%s\n", i, wall_ms, inclusions, tu_file.c_str());
    std::fflush(stdout);

    if (tu != nullptr)
    {
      clang_disposeTranslationUnit(tu);
    }
  }

  auto t_total_end = std::chrono::steady_clock::now();
  double total_ms = std::chrono::duration<double, std::milli>(t_total_end - t_total_start).count();

  // Median per-TU time.
  std::vector<double> tu_times;
  tu_times.reserve(results.size());
  for (const auto &r : results)
  {
    tu_times.push_back(r.wall_ms);
  }
  std::sort(tu_times.begin(), tu_times.end());
  double median_tu_ms = tu_times.empty() ? 0.0 : tu_times[tu_times.size() / 2];

  std::printf("SUMMARY total_ms=%.0f tus=%zu median_tu_ms=%.2f\n", total_ms, results.size(), median_tu_ms);

  clang_CompileCommands_dispose(cmds);
  clang_CompilationDatabase_dispose(db);
  clang_disposeIndex(idx);
  return 0;
}
