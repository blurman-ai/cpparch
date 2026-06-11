#include <filesystem>
#include <iostream>
#include <string_view>

#include "archcheck/config/config_loader.h"
#include "archcheck/version.h"

#include "cli/check_command.h"
#include "cli/diff_command.h"
#include "cli/preview_commands.h"

namespace
{

namespace cli = archcheck::cli;

void print_version() { std::cout << "archcheck " << archcheck::kVersionString << '\n'; }

void print_help()
{
  std::cout
      << "archcheck - architecture rules for C++ projects\n"
      << "\n"
      << "Usage:\n"
      << "  archcheck [path]                             (check: run all default rules on path or cwd)\n"
      << "  archcheck --format json [path]               (JSON output)\n"
      << "  archcheck --config <path> [check-path]       (validate .archcheck.yml v1 + apply thresholds; module "
         "rules not yet enforced)\n"
      << "  archcheck --save-baseline <file> [path]      (save current violations as baseline)\n"
      << "  archcheck --baseline <file> [path]           (report only new violations vs baseline)\n"
      << "  archcheck --save-graph-baseline <file> [path] (save include graph snapshot for drift checks)\n"
      << "  archcheck --drift-baseline <file> [path]    (drift gate: DRIFT.1/DRIFT.2 fail the run; "
         "DRIFT.3 + pre-existing findings advisory)\n"
      << "  archcheck --version\n"
      << "  archcheck --help\n"
      << "  archcheck --scan  <path>                     (preview: discover + scan #includes)\n"
      << "  archcheck --graph <path>                     (preview: build dependency graph + SCC stats)\n"
      << "  archcheck --duplication <path>               (report duplicate code; advisory, does not gate CI)\n"
      << "  archcheck --history <path>                   (history analytics: god-file growth; advisory, does not "
         "gate CI)\n"
      << "  archcheck --diff  [--diff-mode=disk|memory] <revspec> [path]\n"
      << "                                               (regression vs git ref; revspec = 'a..b' or '<ref>')\n"
      << "\n"
      << "Default rules (no config required): SF.7, SF.8, SF.9, Lakos.GodHeader, Lakos.ChainLength\n"
      << "Default thresholds: chain_length=10, god_header_fan_in=50 (override via thresholds: in .archcheck.yml)\n"
      << "Drift rules (require --drift-baseline):        DRIFT.1 (shortcut edges), DRIFT.2 (cycle growth) "
         "[gating]; DRIFT.3 (module coupling) [advisory]\n";
}

bool parseDiffMode(std::string_view raw, cli::DiffMode &out)
{
  if (raw == "memory")
  {
    out = cli::DiffMode::Memory;
    return true;
  }
  if (raw == "disk")
  {
    out = cli::DiffMode::Disk;
    return true;
  }
  return false;
}

// Strip a leading `--diff-mode=...` from argv starting at `idx`. Returns
// the new idx on success; -1 on malformed value. Mutates `mode` in place.
int consumeDiffModeFlag(int argc, char *argv[], int idx, cli::DiffMode &mode)
{
  constexpr std::string_view kPrefix = "--diff-mode=";
  while (idx < argc)
  {
    const std::string_view a{argv[idx]};
    if (a.size() <= kPrefix.size() || a.compare(0, kPrefix.size(), kPrefix) != 0)
      return idx;
    if (!parseDiffMode(a.substr(kPrefix.size()), mode))
    {
      std::cerr << "archcheck: invalid --diff-mode value '" << a.substr(kPrefix.size())
                << "' (expected 'disk' or 'memory')\n";
      return -1;
    }
    ++idx;
  }
  return idx;
}

int dispatch_diff(int argc, char *argv[])
{
  cli::DiffMode mode = cli::DiffMode::Memory;
  const int idx = consumeDiffModeFlag(argc, argv, 2, mode);
  if (idx < 0)
    return 2;
  if (idx >= argc)
  {
    std::cerr << "archcheck: --diff requires <revspec> [path]\n";
    return 2;
  }
  const std::string_view revspec{argv[idx]};
  const std::filesystem::path root =
      (idx + 1 < argc) ? std::filesystem::path{argv[idx + 1]} : std::filesystem::current_path();
  return cli::runDiff(revspec, root, mode);
}

int dispatch_drift_baseline(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --drift-baseline requires <file>\n";
    return 2;
  }
  const std::filesystem::path driftFile{argv[2]};
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return cli::runCheck(root, cli::OutputFormat::Text, {cli::BaselineMode::None, {}, driftFile});
}

int dispatch_save_graph_baseline(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --save-graph-baseline requires <file>\n";
    return 2;
  }
  const std::filesystem::path file{argv[2]};
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return cli::runSaveGraphBaseline(root, file);
}

int dispatch_baseline(std::string_view arg, int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: " << arg << " requires <file>\n";
    return 2;
  }
  const cli::BaselineMode mode = (arg == "--save-baseline") ? cli::BaselineMode::Save : cli::BaselineMode::Load;
  const std::filesystem::path file{argv[2]};
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return cli::runCheck(root, cli::OutputFormat::Text, {mode, file, {}});
}

int dispatch_config(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --config requires <path>\n";
    return 2;
  }
  archcheck::config::Config config;
  try
  {
    config = archcheck::config::load(argv[2]);
  }
  catch (const archcheck::config::ConfigError &e)
  {
    std::cerr << "archcheck: " << e.what() << '\n';
    return 2;
  }
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return cli::runCheck(root, cli::OutputFormat::Text, {}, config);
}

int dispatch_format(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: --format requires a value (text|json)\n";
    return 2;
  }
  const std::string_view fmt{argv[2]};
  if (fmt != "json" && fmt != "text")
  {
    std::cerr << "archcheck: unknown format '" << fmt << "' (expected text|json)\n";
    return 2;
  }
  const auto format = (fmt == "json") ? cli::OutputFormat::Json : cli::OutputFormat::Text;
  const std::filesystem::path root = (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::current_path();
  return cli::runCheck(root, format);
}

bool requireDirectory(const std::filesystem::path &path)
{
  if (std::filesystem::is_directory(path))
    return true;
  std::cerr << "archcheck: not a directory: " << path.string() << '\n';
  return false;
}

int dispatch_with_path(std::string_view arg, int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "archcheck: " << arg << " requires <path>\n";
    return 2;
  }
  if (!requireDirectory(argv[2]))
    return 2;
  if (arg == "--scan")
    return cli::runScan(argv[2]);
  if (arg == "--duplication")
    return cli::runDuplication(argv[2]);
  if (arg == "--history")
    return cli::runHistory(argv[2]);
  return cli::runGraph(argv[2]);
}

bool dispatchBuiltins(std::string_view arg)
{
  if (arg == "--version" || arg == "-V")
  {
    print_version();
    return true;
  }
  if (arg == "--help" || arg == "-h")
  {
    print_help();
    return true;
  }
  return false;
}

// Modes taking a single <path> argument, dispatched through dispatch_with_path.
bool isPathPreviewMode(std::string_view arg)
{
  return arg == "--scan" || arg == "--graph" || arg == "--duplication" || arg == "--history";
}

int dispatch(int argc, char *argv[])
{
  const std::string_view arg{argv[1]};
  if (dispatchBuiltins(arg))
    return 0;
  if (arg == "--baseline" || arg == "--save-baseline")
    return dispatch_baseline(arg, argc, argv);
  if (arg == "--drift-baseline")
    return dispatch_drift_baseline(argc, argv);
  if (arg == "--save-graph-baseline")
    return dispatch_save_graph_baseline(argc, argv);
  if (isPathPreviewMode(arg))
    return dispatch_with_path(arg, argc, argv);
  if (arg == "--diff")
    return dispatch_diff(argc, argv);
  if (arg == "--format")
    return dispatch_format(argc, argv);
  if (arg == "--config")
    return dispatch_config(argc, argv);
  if (!arg.empty() && arg[0] != '-')
  {
    const std::filesystem::path root{argv[1]};
    return requireDirectory(root) ? cli::runCheck(root, cli::OutputFormat::Text) : 2;
  }
  std::cerr << "archcheck: unknown argument '" << arg << "'\n";
  print_help();
  return 2;
}

} // namespace

int main(int argc, char *argv[])
{
  try
  {
    if (argc < 2)
      return cli::runCheck(std::filesystem::current_path(), cli::OutputFormat::Text);
    return dispatch(argc, argv);
  }
  catch (const std::exception &e)
  {
    std::cerr << "archcheck: internal error: " << e.what() << '\n';
    return 3;
  }
  catch (...)
  {
    std::cerr << "archcheck: internal error\n";
    return 3;
  }
}
