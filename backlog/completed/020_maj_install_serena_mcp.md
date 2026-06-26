# [DEVX][TOOLING] Install and integrate Serena MCP for semantic operations on C++ code

**Created:** 2026-05-27
**Started:** 2026-05-27
**Completion date:** 2026-05-27
**Status:** completed
**Module:** DEVX, TOOLING
**Priority:** major
**Difficulty:** S (0.5 day)
**Blocks:** #019 (step 3/3 — method renames lower_snake_case → lowerCamelCase)
**Blocked by:** —
**Related:** #019 (cpp_style_realign), all future tasks requiring AST-rename / semantic refactor

## Goal

Install and connect the **Serena MCP server** to Claude Code so that the agent
has access to AST-aware operations on C++ code (rename symbol first and foremost).
This unblocks #019 step 3/3 (mass method/function renames) and all subsequent work
requiring safe named refactorings at the AST level rather than text.

## Context

While trying to finish #019 step 3/3 (rename methods/functions
`lower_snake_case` → `lowerCamelCase`) it turned out:

1. `clang-rename` (the native LLVM tool for AST-rename) is **not packaged** in
   the AstraLinux/Debian repos. It's not in the `clang-tools` package.
2. The LLVM-prebuilt tarball with `clang-rename` weighs **1.94 GB** —
   disproportionate for a single binary.
3. `clang-tidy 11` has `readability-identifier-naming --fix`, but that is a
   mass rewrite by style rules, not a pinpoint rename — risk of
   touching more than needed.
4. **`clangd`** (the LSP server) is in the repos (clangd-11/13/15/19) and can do
   AST-aware rename via `textDocument/rename`. That is the right backend.

**Serena (oraios/serena)** is an MCP server that wraps LSP into
agent-friendly tools. It supports clangd for C++. It gives the agent the commands
`rename_symbol`, `find_references`, `goto_definition`, etc. based on
LSP, without the agent having to speak LSP-JSON-RPC itself.

Alternatives were considered (a Python wrapper script over clangd, a generic
lsp-mcp, clang-tidy identifier-naming auto-fix) and rejected in favor of
Serena: less home-grown code, reused across future tasks,
the mainstream choice in the MCP ecosystem at the time of writing.

## Execution plan

- [x] Install **clangd-19** via apt (`sudo apt install clangd-19`). Make an `update-alternatives` symlink `clangd -> clangd-19` so CLI calls work without the version
- [x] Generate `build/debug/compile_commands.json` (the `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` flag in cmake; already enabled in the current checkout) — clangd reads it
- [x] **Added beyond plan:** symlink `compile_commands.json` → `build/debug/compile_commands.json` at the repo root. Without it clangd can't find the compile flags, references/rename don't work
- [x] Install Serena via **uvx** (the PyPI `serena` is the wrong package; the right way is via `uvx --from git+https://github.com/oraios/serena serena ...`)
- [x] Register Serena in the Claude Code MCP config. Used the new format: `claude mcp add-json -s user serena '{...}'` → entry in `~/.claude.json`. The old `~/.claude/mcp_servers.json` no longer exists in the current Claude Code version
- [x] No need to restart Claude Code — Serena was picked up in the current session
- [x] **Added beyond plan:** re-register with `--context ide-assistant` (instead of the default `desktop-app`) and `--enable-web-dashboard false --enable-gui-log-window false` — optimization for a CLI agent
- [x] **Added beyond plan:** `.serena/` in `.gitignore` — Serena itself creates `.serena/project.yml` on first activation, no need to version it
- [x] Smoke test: rename of one identifier in `tests/`. **Partial:** the rename renamed only the definition, not the use-site. Reverted via Edit, the file is clean (`git diff` empty). Not a setup bug — it's a gotcha with the clangd background-index, **recorded in `docs/dev/serena_setup.md` → "Gotchas"**. The full smoke-rename is deferred until the index warms up (see below)
- [x] Document in `docs/dev/serena_setup.md` — what was installed, how it's configured, limitations, uninstall
- [x] **Smoke-test reroll (new session 2026-05-27):** `rename_symbol DependencyGraph/successors → successorsSmoke` via Serena → 12 changes (declaration + definition + 10 use-sites in `src/graph/algorithms.cpp`, `baseline.cpp`, `diff.cpp`, `tests/unit/graph/dependency_graph_test.cpp`). Grep confirmed: the only remaining `successors` are in string literals `TEST_CASE("successors ...")`, which is correct (AST doesn't touch strings). **Revert** via reverse rename broke (1 change — only in the .h, clangd cached the old location map after the first rename) — this is a **new variant of Gotcha 1**: a repeated rename in a row within one session before reindexing. Reverted via `git checkout -- src/ include/ tests/`, working copy clean. Documented in `docs/dev/serena_setup.md` — Gotcha 1, point 5 + recommendation to finish the incomplete dep via `replace_content` regex
- [x] **Permission:** added `"mcp__serena__*"` to `~/.claude/settings.json` → `permissions.allow` so Serena calls don't require confirmation (user request 2026-05-27)

## Done

- clangd-19 19.1.7 (1.astra2) installed from the Astra repos, symlink via `update-alternatives` (priority 190)
- Symlink `compile_commands.json` → `build/debug/compile_commands.json` at the repo root (clangd requirement for cross-file references; not obvious from the Serena README)
- Serena MCP registered in `~/.claude.json` (user scope) with the flags `--project-from-cwd --context ide-assistant --enable-web-dashboard false --enable-gui-log-window false`
- Serena was picked up in the current session without restarting Claude Code, `Status: ✓ Connected`
- In the current session Serena already works: `get_symbols_overview`, `find_symbol`, `get_diagnostics_for_file` return correct data; clang-tidy warnings come through via LSP
- `.serena/` added to `.gitignore`
- `docs/dev/serena_setup.md` — full instructions (install from scratch, flags, two gotchas, usage, uninstall)

## In progress

- (empty; task closed)

## Next steps

1. Return to **#019 step 3/3** — mass rename `lower_snake_case` → `lowerCamelCase` via Serena. Workflow per rename: `find_symbol` → (optionally `find_referencing_symbols` to warm up) → `rename_symbol` → `grep` validation → if an incomplete dep remains, `replace_content` regex `\bold\b → new` → build + ctest + lizard. Each logical group (scan functions / graph functions / DependencyGraph methods / struct fields) — one commit, SHA in `.git-blame-ignore-revs`

## Key decisions

| Decision | Reason |
|---------|---------|
| Use Serena, not our own Python LSP driver | Less home-grown code; reused across future semantic-refactor tasks; mainstream choice in the MCP ecosystem |
| clangd-19, not older versions | The newer the better rename works on C++20 templates / concepts. 19 is the latest available in the Astra repos |
| Don't download the LLVM-prebuilt tarball (2 GB) | Disproportionate for a single binary; the clangd-19 deb package is an order of magnitude lighter |
| Don't use clang-tidy identifier-naming --fix | It's a mass rewrite by rules, not a pinpoint rename — risk of touching more than needed |
| Smoke test in `tests/` before the live run | Cheap to check that Serena actually understands scope/AST and isn't doing a text-based replace |
| Symlink `compile_commands.json` at the root (not a third-party symlink in `build/`) | clangd looks in `<project>/` and `<project>/build/`. In our layout the file lives in `build/debug/` — a bridge is needed. A symlink at the root is the least magic, clear to any developer |
| MCP registration scope = `user`, not `project` | To avoid spawning `.mcp.json` in the repo. `--project-from-cwd` removes the "Serena doesn't know which project we mean" problem — it finds it by the CWD of the current Claude Code session |
| `--context ide-assistant` instead of the default `desktop-app` | desktop-app implies a GUI chat and extended summarization. `ide-assistant` is the correct context for CLI agents (Claude Code, Codex, Gemini) per the Serena docs |
| `--enable-web-dashboard false` | Claude Code is a CLI; the browser dashboard would only try to open in the background and clutter the process |
| Smoke test failed as a rename but counted as diagnostic validation | clangd warnings come through Serena → the tool works. The full rename — after index warm-up, not a blocker for the setup task |

## Changed files

| File | Change |
|------|-----------|
| `~/.claude.json` | new entry — Serena MCP (user scope) |
| `docs/dev/serena_setup.md` | new — full instructions |
| `.gitignore` | + `.serena/` |
| `compile_commands.json` (repo root) | new symlink → `build/debug/compile_commands.json` (in gitignore) |
| `.serena/` (auto-generated by Serena) | new locally, not committed |
| (system) `clangd-19` via apt | new |
| (system) Serena cache in `~/.cache/uv/` | new |
| (system) `/etc/alternatives/clangd` → `clangd-19` | new (update-alternatives priority 190) |
