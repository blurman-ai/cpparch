# [BUILD] Dogfood: cppcheck + clang-tidy + lizard в CI

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** BUILD
**Приоритет:** major
**Сложность:** S (день)
**Блокирует:** —
**Заблокирован:** ~~#002 (github_actions_ci)~~ — разблокировано
**Related:** #005 (sarif_reporter_spec)

## Цель

Не дать коду archcheck гнить — в каждом PR прогонять три дешёвые внешние тулзы поверх clang-tidy: **cppcheck**, **lizard**, и второй проход clang-tidy с расширенным набором. Полный отчёт от archcheck на самом себе — отдельной таской, когда правила появятся.

## Контекст

Из исследования 2026-05-26: cppcheck ловит то, что clang-tidy пропускает (UB, утечки на другом анализаторе), lizard — пороги сложности (CCN ≤ 15, функция ≤ 30 строк — совпадает с `docs/code_quality.md`). Оба бесплатные, быстрые, низкий шум. scan-build и `-fanalyzer` — позже, в nightly.

## План выполнения

- [x] Отдельный job `static-analysis` в `ci.yml`, параллельно сборке (один runner, ubuntu-24.04)
- [x] **cppcheck:** `cppcheck --enable=warning,performance,portability --inline-suppr --error-exitcode=1 src/ include/` — падает на находках. Локально на нашем коде: 0 находок.
- [x] **lizard:** `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` + grep на `warning:` / `!!!` — падает на превышении (lizard сам по себе всегда exit 0). Локально на нашем коде: 0 находок.
- [x] **clang-tidy второй проход** с `.clang-tidy-strict` (bugprone, performance, modernize, cppcoreguidelines, readability, cert, misc, hicpp, llvm). Запускается с `continue-on-error: true` — **warning-only**, копит baseline, не валит сборку.
- [x] Сохранять отчёты (`lizard-report.txt`, `clang-tidy-strict-report.txt`) как artifacts на 14 дней
- [x] **Не** включать IWYU — мы сами в этой нише
- [x] **Не** включать flawfinder — устаревает, для нас почти всё мимо

## Сделано

- **2026-05-26** — Завёл `.clang-tidy-strict` (cert-*, hicpp-*, misc-*, llvm-* поверх обычного набора). Шумные чеки (`llvm-header-guard`, `llvm-include-order`, `hicpp-signed-bitwise`, `cert-err58-cpp`, `cppcoreguidelines-pro-bounds-*`) выключены явно.
- **2026-05-26** — В `.github/workflows/ci.yml` добавлен job `static-analysis` (параллелен `build`, ubuntu-24.04):
  - apt install: `cppcheck`, `clang-tidy-18`, `g++-13`, `ninja-build`, `cmake`; pip install: `lizard`.
  - Cache `build/debug/_deps` по отдельному ключу `deps-Linux-static-...` (изолирован от build-cache, иначе race condition при параллельном write).
  - `cmake configure` для генерации `compile_commands.json` (нужен clang-tidy).
  - **cppcheck**: gate (`--error-exitcode=1`).
  - **lizard**: gate с grep-проверкой на `warning:`/`!!!` (lizard сам всегда exit 0).
  - **clang-tidy strict**: `continue-on-error: true`, печатает report в лог в группе `clang-tidy strict report`, копит baseline.
  - Upload `lizard-report.txt` + `clang-tidy-strict-report.txt` как artifact на 14 дней.
- **2026-05-26** — **Локальная верификация**: cppcheck + lizard прогнаны на `src/`, `include/`, `tests/` — оба зелёные (exit 0, нет находок).
  - clang-tidy локально не проверен — машина имеет clang-tidy 11, который не поддерживает `--config-file=` (это с 12+). В CI используется clang-tidy-18.

## В работе
- (пусто)

## Следующие шаги

1. cppcheck job — самый дешёвый, начать с него
2. lizard job
3. clang-tidy strict как warning-only

## Ключевые решения

| Решение | Причина |
|---------|---------|
| cppcheck + lizard как gate, clang-tidy strict как warning | Минимум шума на старте, иначе бросим |
| scan-build/-fanalyzer — позже | Дороже по времени, в nightly когда стабилизируется CI |

## Изменённые файлы

| Файл | Изменение | Commit |
|------|-----------|--------|
| `.clang-tidy-strict` | новый — bugprone + performance + modernize + cppcoreguidelines + readability + cert + misc + hicpp + llvm | `1f06811` |
| `.github/workflows/ci.yml` | новый job `static-analysis` параллельно с `build` | `1f06811` |
| `.github/workflows/ci.yml` | bump actions/upload-artifact@v4 → v7 (Node 24) | `0a2a619` |

## Как работает

CI workflow теперь имеет два независимых job-а, бегущих параллельно:

1. **`build`** (gcc-13 / clang-18 matrix) — конфигурация, компиляция, ctest, smoke бинаря. Зелёный = код собрался и тесты прошли.
2. **`static-analysis`** (single runner) — три тулзы поверх того же `compile_commands.json`:
   - **cppcheck** — `--enable=warning,performance,portability`, `--inline-suppr`, `--error-exitcode=1`. Падает при любой находке этих категорий.
   - **lizard** — пороги из `docs/code_quality.md`: `CCN ≤ 15`, функция ≤ 30 строк, ≤ 5 аргументов. Lizard сам по себе всегда exit 0; шаг grep-ает отчёт на `warning:` / `!!!` и явно фейлится если найдено.
   - **clang-tidy strict** — с `.clang-tidy-strict` (extended check set), `continue-on-error: true`. Не валит build, печатает отчёт в лог в `::group::`, копит baseline для последующего ужесточения. Output также уходит в artifact.

**Cache.** Static-analysis job делит cmake configure с build (нужен для compile_commands.json), но кэширует FetchContent в отдельном scope `deps-Linux-static-...` чтобы избежать write-race с build job-ом, которые могут параллельно тащить ryml/Catch2.

**Artifacts.** `static-analysis-reports` (lizard + clang-tidy-strict отчёты) хранятся 14 дней. Доступны по ссылке справа от run-а на странице Actions.

**Что НЕ включено:**
- IWYU — мы сами в этой нише.
- flawfinder — устарел, near-zero signal для наших паттернов.
- scan-build / `-fanalyzer` — слишком медленные для per-PR, кандидаты для nightly workflow когда дойдёт.

## Чем управляется

| Параметр | Где меняется |
|---|---|
| Пороги lizard | `--CCN`, `--length`, `--arguments` в шаге workflow. Совпадает с `docs/code_quality.md` — менять синхронно. |
| Категории cppcheck | `--enable=...` в шаге workflow. Чтобы добавить style: `warning,performance,portability,style`. |
| clang-tidy strict checks | `.clang-tidy-strict` — `Checks:` ключ. |
| Шумные чеки в strict | List `-check-name` в `.clang-tidy-strict`. По мере стабилизации — переносить в обычный `.clang-tidy` как hard error. |
| Retention отчётов | `retention-days: 14` в upload-artifact шаге. |

## С чем связана

- **#002 (github_actions_ci)** — статический анализ живёт в том же workflow `.github/workflows/ci.yml`, параллельным job-ом. При изменении основного build job — проверять, что static-analysis cache не конфликтует.
- **#004 (project_skeleton)** — статический анализ зависит от `.clang-tidy` (обычный, в build) и `.clang-tidy-strict` (новый, в static-analysis). Оба настроены под layout из #004.
- **Будущая задача `nightly_deeper_analysis`** — для `scan-build`, `-fanalyzer`, ASan/UBSan на полноценных проектах. Не блокер, заводить когда появится боль от false negative-ов от current set.
- **`docs/code_quality.md`** — пороги lizard прямо из этого документа. Если меняем пороги в одном месте — сразу синхронизируем второе.

## Диагностика

```bash
# Локальная воспроизводимость (на машине разработчика):
cppcheck --enable=warning,performance,portability --inline-suppr \
  --error-exitcode=1 --suppress=missingIncludeSystem --quiet \
  -I include src/ include/

lizard --CCN 15 --length 30 --arguments 5 --warnings_only \
  src/ include/ tests/

# clang-tidy strict требует CMake-configure (для compile_commands.json)
# и clang-tidy >= 12 (поддержка --config-file=):
cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
clang-tidy --config-file=.clang-tidy-strict -p build/debug \
  $(find src/ -name '*.cpp')

# Артефакт CI:
# https://github.com/blurman-ai/cpparch/actions  ->  выбрать run  ->
# справа "Artifacts" -> static-analysis-reports
```

Если cppcheck/lizard падает локально, а в CI зелено — обычно это разница версий тулзов (apt cppcheck на Astra 1.7 — 1.86, на ubuntu-24.04 — 2.13+; lizard через pip — одинаковый везде).
