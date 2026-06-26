# [RULES] SF.8: Objective-C headers are skipped (not checked for a C++ include guard)

**Date created:** 2026-05-28
**Date started:** 2026-05-28 (de facto — came in as a side effect of #034)
**Date completed:** 2026-05-29
**Status:** done
**Module:** RULES
**Priority:** minor
**Difficulty:** XS (in fact: ~30 lines + 2 tests)
**Blocks:** —
**Blocked by:** —
**Related:** #034 (sf8_scan_limit_and_inc_files — the ObjC-skip implementation landed alongside the .inc-skip), #028 (rules_engine_mvp)

## Goal

SF.8 must not check Objective-C headers — they have a different inclusion mechanism (`#import`, automatically deduplicates) and a different syntax (`@interface` / `@implementation`). Without a separate skip the rule reports false violations on ObjC files in C++/ObjC mixed projects (grpc `examples/objective-c/`, any iOS+C++).

## Context

A run on grpc (commit `05ffa92`) showed false SF.8 in `examples/objective-c/`:

```objc
// examples/objective-c/helloworld/HelloWorld/AppDelegate.h
#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end
```

Initially the task was filed as `[OUT-OF-SCOPE]` — "archcheck = C++ only, we document it as a known limitation". The decision was reconsidered: skipping ObjC is cheaper than explaining it in a FAQ, and it immediately removes noise for mixed projects, of which there are many in practice.

## Done

- **2026-05-28**, commit `4bb56f9` (refactor: extract helpers): `isObjcFile()` added to the anon namespace of the same `sf8_include_guard.cpp`, together with splitting `check()` under the lizard threshold (function length). The ObjC-skip arrived quietly together with the .inc-skip from #034.
- **2026-05-29**, commit `a5ec300` (test(rules/sf8)): added two regression tests for the `@interface` and `#import` markers. Before that the functionality worked but wasn't covered — the `isObjcFile()` refactor could have silently broken.

## How it works

SF.8 walks the graph nodes, for each header reads the source and decides whether to report a missing include guard. Before the check there are three early-exits:

1. **Not a header** — `isHeaderFile()` by extension from `scan/project_files.h`.
2. **`.inc` fragment** — `isIncFile()` by extension (see #034). These files are deliberately included once via `#if`, a guard would be harmful.
3. **ObjC file** — `isObjcFile()` scans the first 60 non-empty lines of the source, looking for one of three markers:
   - `@interface`
   - `@implementation`
   - `#import`

   First hit → the file is considered ObjC, we skip it. See [src/rules/sf8_include_guard.cpp:27-48](src/rules/sf8_include_guard.cpp#L27-L48).

If a file passed all three filters — `hasIncludeGuard()` runs (looking for `#pragma once` or `#ifndef` in the same 60 lines). No guard → violation.

**Why 60 lines and not 50:** covers the Apache 2.0 boilerplate (~47 lines), typical of ObjC-runtime files. The limit is set by the shared constant `kScanLines` — the same one `hasIncludeGuard` uses, so that the "how deep do we look" behavior is consistent.

**Why any one of the markers is enough:**
- `@interface` / `@implementation` — unambiguous ObjC tokens, they don't occur in pure C++.
- `#import` — formally GCC also has an extension for C++, but in pure C++ it's practically never seen (deprecated since the 90s); the risk of a false skip is negligible against the gain on ObjC files with an Apache copyright + `#import` in the first meaningful line.

## What governs it

Nothing — the implementation is fully built in, there's no CLI flag, no YAML setting. This is deliberate: "zero-config first" (see CLAUDE.md). If someone needs to force-check an ObjC file — a `--force-sf8-on-objc` or equivalent could be added, but there are no requests yet.

`kScanLines = 60` is the only "magic" threshold, shared with `hasIncludeGuard`. A candidate to move into the `Config` struct under task #041 (audit hardcoded strings).

## What it's connected to

- **#034** — the host task alongside the `.inc`-skip. The ObjC-skip arrived by the same "not a C++ header → don't check it" logic, in the same refactoring commit.
- **#028** — rules engine MVP, in which SF.8 is registered as an intrinsic v0.1 rule.
- **#041** — may move `kScanLines` into `Config`.

## Diagnostics

- Tests: `tests/unit/rules/sf8_include_guard_test.cpp:89-114` — two cases (`@interface` and `#import`).
- Run on a grpc/iOS-mixed project: 0 false SF.8 on `examples/objective-c/` after the fix (manual check, grpc not committed into fixtures).
- If the rule starts reporting ObjC files — first suspicion: the marker moved past `kScanLines` (e.g. a long license block > 60 lines). Raise the threshold in `sf8_include_guard.cpp:16`.

## Changed files

| File | Change | Commit |
|------|-----------|--------|
| `src/rules/sf8_include_guard.cpp` | `isObjcFile()` + early skip in `check()` | `4bb56f9` |
| `tests/unit/rules/sf8_include_guard_test.cpp` | +2 regression tests (`@interface`, `#import`) | `a5ec300` |
