# [CONFIG][DIFF] Порог diff_max_added_lines: глушить LCX-advisory на bulk-импортах

**Дата создания:** 2026-06-12
**Дата старта:** 2026-06-12
**Статус:** completed (дубль — реализовано под #117 из-за ID-клэша)
**Дата завершения:** 2026-06-12 (фича), закрыто как дубль 2026-06-13
**Модуль:** CONFIG / CLI(diff)
**Приоритет:** minor
**Сложность:** small
**Related:** #109 (корпус: мега-дропы дают сотни механически-честных находок)

> **РАЗРЕШЕНИЕ ДУБЛЯ (2026-06-13):** эта задача и `completed/117_min_diff_max_added_lines.md`
> описывают одну фичу — порог `thresholds.diff_max_added_lines`. Реализация поехала
> под номером #117 (см. тот файл: `collectDiffAdvisories` + `git::totalAddedLines` +
> табличный парсер + skip-пометка, проверено на hpsx64 +419K). Весь DoD ниже закрыт.
> Сверено по коду 2026-06-13: `config.h:diffMaxAddedLines=10000`,
> `config_loader.cpp` ключ `diff_max_added_lines`, `diff_command.cpp` гейт + пометка.
> Файл оставлен в истории как след ID-клэша #117/#118.

## Цель

Коммиты-«мега-дропы» (bulk-импорт чужих исходников: «extract sources» +419К
строк → 241 находка; vsomeip +21.9К → 19) — не авторская эволюция кода, их
LCX-сигналы — шум по объёму. По умолчанию в конфиге вводится максимум
добавленных строк диффа; при превышении local-complexity advisory
пропускается с явной пометкой. Порог переопределяется в `.archcheck.yml`.

## Решения

- Дефолт **10 000 добавленных строк**: калибровка по корпусу #109 — честные
  крупные фичи ниже (sbbs SQLite-класс +5.6К, webf +1.6К), bulk-дропы выше
  (vsomeip +21.9К, hpsx64 +419К, nvdajp +3.37M).
- Ключ `thresholds: diff_max_added_lines` (та же phase-2 секция, что
  chain_length / god_header_fan_in).
- Считаются added-строки по `git diff --numstat` (уже собирается для
  test-co-evolution — ноль новых git-вызовов).
- Глушится ТОЛЬКО complexity-advisory (SATD/test-co-evolution остаются):
  скип печатается явно, молча не исчезает.

## Definition of Done

- Thresholds.diffMaxAddedLines + парсер ключа + фикстуры pass/fail.
- Сумматор added-строк — тестируемый хелпер (unit-тест).
- Скип с пометкой в текстовом выводе --diff.
- Гейты зелёные, dogfooding чист.
