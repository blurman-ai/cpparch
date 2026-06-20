# Coverage constraints — archcheck

## Текущие пороги

`scripts/check_coverage.sh` проверяет три метрики только по `src/`:

| Метрика   | Порог | Реальное (2026-05) |
|-----------|-------|--------------------|
| Lines     | 90%   | ~96%               |
| Functions | 90%   | ~95%               |
| Branches  | 40%   | ~63%               |

## Почему branches застрял на 63%, и что с этим делать нельзя

### Причина: gcov + C++ exceptions + lcov 1.13

Astra Linux 1.7 поставляет **lcov 1.13** (Debian Buster).

В C++ каждый вызов функции, которая потенциально бросает исключение, порождает два пути в CFG:
- нормальный возврат → ветвь «covered»
- throw arc → ветвь «uncovered»

Это касается `std::string::push_back`, `operator<<`, `vector::reserve` и т.п.
Физически добраться до этих ветвей в тестах невозможно — OOM не симулируется.

**lcov ≥ 1.15** имеет флаг `--exclude-unreachable-branches`, который убирает этот шум.  
**lcov 2.x** (текущий апстрим, v2.4) имеет `--filter exception`.  
Обе версии требуют Perl-зависимостей (`DateTime`, `Capture::Tiny`), которых нет в
стандартном репозитории Astra Linux 1.7.

### Альтернатива: gcovr

`gcovr` (Python, `pip3 install gcovr`) имеет `--exclude-throw-branches`.
Подходит для CI на Ubuntu 22.04+ / Debian Bookworm, но требует переписывания
`check_coverage.sh` под gcovr-формат.

### Что было проверено

- Скачан lcov 2.4 (Perl-скрипты с GitHub), установлены `libcapture-tiny-perl`,
  `libdatetime-perl`. Запущен с `--filter exception` — флаг существует, но
  на данных от GCC 8.3 эффекта не оказал: branches остались на 61–62%
  (gcov 8.x не размечает throw-arcs так, чтобы lcov 2.4 их распознал).
- Итог: без обновления toolchain (gcc ≥ 10, lcov ≥ 1.15 или gcovr)
  преодолеть ~65% branches не выйдет механически.

## Что поднять можно уже сейчас

`MIN_LINES=90` и `MIN_FUNCTIONS=90` — уже проходят, подняты.  
`MIN_BRANCHES` остаётся на `40` до смены окружения.

## Где гоняется

| Окружение | Тулза | Запуск |
|-----------|-------|--------|
| Локально (Astra, lcov 1.13) | lcov | `scripts/check_coverage.sh`, шаг 6 в `/commit` и `/autocommit` |
| CI (Ubuntu 24.04, gcc-13) | gcovr | job `coverage` в `.github/workflows/ci.yml` (жёсткий гейт) |

CI намеренно НЕ зовёт `check_coverage.sh`: тот написан под lcov 1.13, а apt
Ubuntu 24.04 ставит lcov 2.x, который валится на throw-arc'ах и несовпавших
`--remove` паттернах. gcovr — единый pip-пакет с нативным `--fail-under-*` и
`--exclude-throw-branches`. **Пороги в обоих местах держать одинаковыми (90/90/40);
эта таблица — источник правды.** Branches на gcovr выше lcov-овых ~63% (throw-arc'и
вырезаны `--exclude-throw-branches`), так что порог 40% с большим запасом.

## Когда lcov-ветку станет решаемым поднять

При переходе локального окружения на:
- Ubuntu 22.04+ / Debian Bookworm (lcov 1.16+) — добавить `--exclude-unreachable-branches`
- или `pip3 install gcovr` + переписать `check_coverage.sh` под gcovr (унифицировать с CI)

## GCC 8 / C++20 stdlib gaps (Astra Linux 1.7)

### Суть проблемы

Astra Linux 1.7 поставляет **GCC 8.3**. Проект объявлен как C++20 (`-std=c++20`),
и GCC 8 принимает этот флаг — но реализует далеко не весь стандарт. Часть C++20
stdlib-фич появилась только в GCC 10 (libstdc++ 10).

В CI (Ubuntu 24.04, GCC 13) и на любом современном Linux эти фичи работают нормально.
Проблема возникает только при локальной сборке на Astra.

### Известные недостающие фичи

| Фича | Доступна с | Обходной путь |
|------|-----------|---------------|
| `std::string_view::starts_with(x)` | GCC 10 | `s.find(x) == 0` |
| `std::string_view::ends_with(x)` | GCC 10 | `s.rfind(x) == s.size() - x.size()` |
| `std::string::starts_with(x)` | GCC 10 | то же |
| `std::string::ends_with(x)` | GCC 10 | то же |

Полный список C++20 library features по компиляторам:
https://en.cppreference.com/w/cpp/compiler_support/20

### Маркер для быстрого поиска при обновлении Astra

**Правило:** каждый обходной путь сопровождается комментарием-тегом `// GCC8-COMPAT:`.

```cpp
// GCC8-COMPAT: starts_with() requires GCC 10; replace when Astra upgrades
if (path.find(prefix) == 0) // cppcheck-suppress stlIfStrFind
```

При переходе на новую Astra (или когда Astra обновит toolchain до GCC 10+):

```bash
grep -rn "GCC8-COMPAT" src/ include/ tests/
```

Найдёт все места, которые нужно привести в порядок.

### Текущие экземпляры

| Файл | Строка | Обходной путь | Целевой код |
|------|--------|---------------|-------------|
| `src/scan/include_resolver.cpp` | строка с `path.find(prefix) == 0` | `find()==0` + cppcheck-suppress | `path.starts_with(prefix)` |

### Отдельный случай: `(void)::read()` на GCC 13

Это **не** GCC 8 проблема — обратная: в CI (GCC 13, Ubuntu 24.04)
`(void)`-cast не подавляет `-Werror=unused-result` для `read()`.
Решение — `[[maybe_unused]] auto n = ::read(...)`.
Метка `GCC8-COMPAT:` не используется, это CI-специфика.

## LCOV_EXCL_* маркеры уже в коде

В `src/git/git_state.cpp` и `src/git/git_object_file_source.cpp` расставлены
`// LCOV_EXCL_START` / `// LCOV_EXCL_STOP` вокруг post-fork child-process кода —
это единственное законное место для таких аннотаций (код физически не виден gcov
в родительском процессе). Остальные throw-ветви аннотировать смысла нет до смены lcov.
