# DRIFT.1 fixture — LibreSprite PR #581

A minimal reproducible graph slice for the single real DRIFT.1 hit
from the 2026-05-29 dogfood run (see `docs/research/ai_drift_cases.md` and
`docs/milestones.md` §"Run 10").

- **Repo:** `LibreSprite/LibreSprite`
- **PR:** #581 (macOS / menu-search / toolbar badges)
- **Before / After SHA:** `60eed0f` → `276fdbd`
- **Source:** commit `0aa57ad` "Add keyboard shortcut badges to toolbar icons"
  (Co-Authored-By: Claude Opus 4.5)
- **Skeptic framing:** the edge is present in upstream `aseprite/aseprite`,
  but the LibreSprite fork at `60eed0f` did not carry it. Verified via `git show` + `git log`,
  verdict CONFIRMED (see the "Verification" section in `ai_drift_cases.md`).

`baseline.graph.yml` captures the before-state: `toolbar.cpp` includes only
`toolbar.h`. The files in the directory are the after-state: a direct include
`#include "app/pref/preferences.h"` was added. DRIFT.1 should catch exactly one
new edge `app/ui/toolbar.cpp -> app/pref/preferences.h`.
