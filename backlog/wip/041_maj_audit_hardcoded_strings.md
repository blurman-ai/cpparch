# [RULES][CONFIG] Аудит захардкоженных строк и констант

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-30
**Статус:** wip
**Модуль:** RULES, CONFIG
**Приоритет:** major
**Сложность:** M-L (рефактор дефолтов в Config + walkup + merge; override порогов через YAML = расширение формата phase 2)
**Блокирует:** —
**Заблокирован:** —
**Related:** docs/config_format.md (§Discovery & default resolution — модель зафиксирована), completed/051_maj_config_loader_v1.md (phase-1 loader), completed/v1_maj_config_format_minimal_contract.md

## Цель

Инвентаризировать все захардкоженные строки и константы в коде (паттерны файлов, пороговые значения, тексты нарушений), классифицировать каждую и вынести «переопределяемые» в `Config` или константы с явным намерением.

## Контекст

Под маркой «zero-config» в код просачиваются контекстные строки трёх видов:

1. **Паттерны файлов** — расширения `.h`, `.hpp`, `.inc`, пути к системным заголовкам — могут не совпадать с реальным проектом пользователя.
2. **Пороговые значения** — `chain_length=10`, `god_header_fan_in=50` — вшиты в реализацию правил (`kDefaultThreshold` в `.h` каждого правила), но должны быть переопределяемы через `.archcheck.yml`. (NB: `brace_depth=1` из исходной формулировки — это механика парсинга SF.7, не настройка; не выносим.)
3. **Тексты нарушений** — violation message содержит конкретный контекст («using namespace в заголовочном файле») — это нормально, но формат должен быть консистентен.

«Zero-config» означает «хорошие дефолты», а не «неизменяемые магические числа». Пользователь с нестандартным проектом не должен патчить исходники.

### Целевая архитектура: Embedded Defaults + File Override

Паттерн применяемый clang-tidy, rustfmt, clippy — именно то что нужно для single static binary:

```
Hardcoded Config defaults  →  merge  ←  .archcheck.yml (если есть)
                                ↓
                          Final Config  →  rules
```

1. Все дефолты живут в именованных полях `Config` struct — один источник правды
2. При запуске ищем `.archcheck.yml` вверх по дереву от CWD
3. Найденный файл мержится **поверх** дефолтов (пользователь пишет только то, что меняет)
4. `Config` передаётся в каждое правило через интерфейс `IRule`

Проблема не в том, что дефолты вшиты в бинарь — так и должно быть. Проблема в том, что они **разбросаны** по реализациям правил вместо единой `Config` struct.

## План выполнения

- [ ] Grep по `src/` и `include/`: собрать все строковые литералы и числовые константы связанные с правилами
- [ ] Составить инвентарную таблицу: строка / файл:строка / категория
- [ ] Категории:
  - `FIXED` — константа по стандарту (например, `#pragma once` — это буквально синтаксис C++)
  - `DEFAULT` — правильный дефолт, разбросан по коду; нужно перенести в `Config` struct
  - `INLINE_OK` — violation message, встроено нормально, но нужна консистентность формата
- [ ] Создать / расширить `Config` struct: одно поле на каждый `DEFAULT`, с дефолтным значением
- [ ] Убедиться что `IRule::check()` получает `Config` (или subset) — не читает глобальные константы
- [ ] Реализовать поиск и мерж `.archcheck.yml` поверх дефолтов
- [ ] Документировать все дефолты в `README.md` в секции «Default thresholds» и в `archcheck --help`

## Сделано

- **Инвентарь захардкоженных констант** (2026-05-30):

  | Что | Значение | Где сейчас | Категория |
  |-----|----------|-----------|-----------|
  | include chain length | `10` | `include/archcheck/rules/lakos_chain_length.h:11` (`kDefaultThreshold`) | DEFAULT |
  | god-header fan-in | `50` | `include/archcheck/rules/lakos_god_headers.h:14` (`kDefaultThreshold`) | DEFAULT |
  | project-расширения | `.c .cc .cpp …` | `src/scan/project_files.cpp:16` (`kProjectExtensions`) | DEFAULT |
  | header-расширения | `.h .hpp …` | `src/scan/project_files.cpp:19` (`kHeaderExtensions`) | DEFAULT |
  | `#pragma once` / guard-синтаксис | — | `src/rules/sf8_include_guard.cpp` | FIXED |
  | `.inc` = single-include | — | `src/rules/sf8_include_guard.cpp:52` | FIXED |
  | SF.7 brace-depth | `1` | `src/rules/sf7_using_namespace.cpp` | FIXED (парсинг) |
  | тексты violation | — | все правила | INLINE_OK |

- **Дизайн зафиксирован** в `docs/config_format.md` §«Discovery & default resolution» и в памяти проекта (`project_config_discovery_defaults`): embedded-defaults + file-override; walkup от CWD до FS-root (первый файл выигрывает); `--config` отключает walkup; merge поверх дефолтов.

## В работе

- (пусто)

## Следующие шаги

> ⚠️ **Scope:** override порогов через YAML = блок `thresholds:`, а это **v1 phase 2** по `docs/config_format.md`. Phase-1 loader (#051) знает только `version`/`modules`/`rules`. Эта задача добавляет `thresholds:` (additive/MINOR, `version: 1` сохраняется) + рефактор дефолтов + walkup.

1. Завести поля в `Config` struct под DEFAULT-константы (`thresholds.chain_length`, `thresholds.god_header_fan_in`, extensions) с in-code дефолтами.
2. Передавать `Config` в правила (через `IRule`/конструкторы) вместо чтения `kDefaultThreshold`.
3. Реализовать walkup-поиск `.archcheck.yml` от CWD до FS-root; `--config` отключает.
4. Парсинг блока `thresholds:` в loader + merge поверх дефолтов; обновить `docs/config_format.md` (перенести `thresholds:` из «not in phase 1» в реализованное).
5. Fixtures: `pass/` + `fail_*/` на override порога; документировать дефолты в README + `--help`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Дефолты остаются в бинаре, не требуют файла | Zero-config: без `.archcheck.yml` работает с первого запуска |
| Все `DEFAULT` в единой `Config` struct | Единый источник правды; clang-tidy/rustfmt/clippy используют ту же схему |
| Мерж `.archcheck.yml` поверх дефолтов | Пользователь пишет только отличия, файл минимален |
| Поиск конфига вверх по дереву (walkup) | Стандарт для CLI-инструментов; удобно из поддиректорий проекта |
| Тексты нарушений остаются inline | Они часть семантики правила, а не настройки |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| ... | ... |
