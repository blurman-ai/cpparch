# DRIFT.1 fixture — LibreSprite PR #581

Минимальный воспроизводимый срез графа под единственный реальный DRIFT.1 hit
из 2026-05-29 dogfood-прогона (см. `docs/research/ai_drift_cases.md` и
`docs/milestones.md` §«Прогон 10»).

- **Repo:** `LibreSprite/LibreSprite`
- **PR:** #581 (macOS / menu-search / toolbar badges)
- **Before / After SHA:** `60eed0f` → `276fdbd`
- **Источник:** commit `0aa57ad` "Add keyboard shortcut badges to toolbar icons"
  (Co-Authored-By: Claude Opus 4.5)
- **Скептик-фрейминг:** edge присутствует в upstream `aseprite/aseprite`,
  но LibreSprite-форк на `60eed0f` его не нёс. Проверено `git show` + `git log`,
  вердикт CONFIRMED (см. секцию «Верификация» в `ai_drift_cases.md`).

`baseline.graph.yml` фиксирует before-состояние: `toolbar.cpp` включает только
`toolbar.h`. Файлы в директории — after-состояние: добавлен прямой include
`#include "app/pref/preferences.h"`. DRIFT.1 должен поймать ровно одно
новое ребро `app/ui/toolbar.cpp -> app/pref/preferences.h`.
