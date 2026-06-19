# [RULES][CLI] First-run noise floor: ChainLength/GodHeader флудят на header-heavy либах

**Дата создания:** 2026-06-19
**Дата старта:** 2026-06-19
**Статус:** wip — основной фикс (advisory в check-mode) сделан + проверен; остаток = `--diff` mass-rename guard (узкий)
**Модуль:** RULES / GRAPH / CLI
**Приоритет:** major (go-public quality — первый контакт)

## Сделано (2026-06-19) — решение «advisory в check-mode»

Выбор пользователя: **gating (exit 1) в check-mode только за цикл (SF.9)**; ChainLength,
GodHeader, SF.7/SF.8 — reported, но advisory (exit 0). Это рекомендация §7 отчёта
(«gate = циклы; god-header и прочее advisory») и зеркало модели `--diff`/drift.

- `src/cli/check_command.cpp`: добавлен `reportCheckGate` (по образцу `reportDriftGate`)
  — exit 1 только если есть `SF.9`; печатает «N advisory finding(s) … not gated …
  use --baseline». Заменил `return all.empty() ? 0 : 1;`.
- Тест `cli_smoke_e2e_test.cpp`: новая цикл-фикстура (gating exit 1) + кейс «SF.7
  репортится, но exit 0»; json-тест → exit 0.
- **Верификация (re-run sanity):** abseil 219 наход. → **exit 0**, spdlog 40 → **0**,
  fmt → **0**; curl → **exit 1** (цикл `curl.h↔multi.h` гейтит — TP, как и надо).
  547/547 тестов, dogfood 0, lizard/format чисты. НЕ коммичено (параллельные сессии).
- ⚠️ **Контракт exit-кодов сдвинулся** (check-mode: 1 = только циклы, не «любые
  нарушения») — нужно записать в CHANGELOG (v0.1, pre-tag, допустимо).

**Остаток:** только `--diff` mass-rename guard (ниже) — узко, не блокирует go-public.
**Блокирует:** анонс / выход к публике (первый чужой репо не должен тонуть в шуме)
**Заблокирован:** —
**Related:** #127 (vendored exclusion — снимает ЧАСТЬ шума, но abseil не vendored), #126 (SF.9 component collapse), #057 (cheap graph signals), MVP.md §«--baseline с первого дня»

## Доказательство (first-run sanity, 2026-06-19)

`archcheck <repo>` (check-mode, без `--baseline`) на известных C++-репах:

| Репа | Всего | Разбивка | exit |
|------|-------|----------|------|
| fmt | 1 | SF.8 ×1 | — |
| nlohmann_json | 2 | ChainLength ×2 | — |
| spdlog | 40 | **ChainLength ×39**, SF.8 ×1 | 1 |
| curl | 14 | ChainLength ×5, **GodHeader ×8**, SF.9 ×1 (TP) | 1 |
| **abseil** | **219** | **ChainLength ×211**, GodHeader ×8 | 1 |

**Вывод:** шум первого запуска — это **ChainLength** (порог цепочки 10) и вторично
**GodHeader** (fan-in 50). Циклы НЕ шумят (abseil/spdlog — 0 циклов; curl-цикл
`curl.h↔multi.h` — настоящий TP). Это и есть «5000 нарушений на первом запуске», от
которого спека защищается `--baseline` — но **наивный первый `archcheck <repo>`** (а
именно так человек щупает инструмент) выдаёт abseil = 219, exit 1, и его спишут.

## Почему это go-public блокер

Первое, что делает скептик с HN: `archcheck` на своём/известном репе **без флагов**.
Если ответ — «219 нарушений, в основном include chain depth 11 > 10» — он закрывает
вкладку. Узкая бесспорная ценность (цикл/копипаст, введённый PR) утонет в ChainLength-шуме.

## Что решить (это дизайн, не правится наспех)

1. **ChainLength порог 10 — слишком агрессивен для современного header-heavy C++.**
   abseil/spdlog/nlohmann深 цепочки — норма, не долг. Варианты:
   - поднять дефолтный порог (15? 20? — замерить распределение по корпусу);
   - сделать ChainLength **advisory** (не gating/exit-1) в check-mode — оставить
     gating только за cycle/god-header (как в `--diff`);
   - оставить порог, но в check-mode без `--baseline` печатать явную подсказку
     «N existing findings — run with --baseline to gate only new drift».
2. **GodHeader на config/logging-хабах** (`curl_setup.h` fan-in 309) — структурно-но-
   легитимно (см. историю showcase #003, снят как слабый). fan-in-only прокси.
   Вариант: allowlist известных широких хедеров / понизить severity до advisory.
3. **First-run UX:** возможно, дефолтный `archcheck <repo>` без `--baseline` должен
   сам предлагать `--baseline` («это снимок всего долга; для CI-гейта возьми --diff»).

## Цикл-гейт mass-rename (узкий остаток, отдельно)

В check-mode циклы чисты. В `--diff` ~19% cycle-fires — артефакты массовых
include-rewrite/move (coal 252 файла, allwpilib 2477). Это **отдельный** `--diff`-guard:
подавлять `grown_cycles`, когда коммит = массовый rename (>N rename-рёбер). Не для
check-mode; вынести в #131-проверку или сюда подпунктом. `.tmpl/_impl`-идиома уже
исключена (#088/#126) — её НЕ трогать (curl-цикл и JANA2-жемчужина — настоящие TP).

## Проверка (фикстуры обязательны)

- [ ] Решение по ChainLength зафиксировано (порог / advisory / baseline-подсказка).
- [ ] Фикстуры: header-heavy цепочка глубины 12 → поведение по выбранному варианту.
- [ ] Re-run sanity: abseil/spdlog/curl → шум упал до защитимого; exit-1 только на
      реальных gating-сигналах (или с явной first-run-подсказкой).
- [ ] Цикл/копипаст-TP (curl-цикл, JANA2) НЕ подавлены.

## Самопроверка

Не «занизить пороги, чтобы числа выглядели тихо» — это обратный обман. Цель — чтобы
**оставшиеся** срабатывания были defensible, а existing-долг шёл через `--baseline`,
как и задумано. Замерить распределение цепочек по корпусу перед сдвигом порога.
