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

## Когда это станет решаемым

При переходе на:
- Ubuntu 22.04+ / Debian Bookworm (lcov 1.16+) — добавить `--exclude-unreachable-branches`
- или `pip3 install gcovr` + переписать `check_coverage.sh` под gcovr

## LCOV_EXCL_* маркеры уже в коде

В `src/git/git_state.cpp` и `src/git/git_object_file_source.cpp` расставлены
`// LCOV_EXCL_START` / `// LCOV_EXCL_STOP` вокруг post-fork child-process кода —
это единственное законное место для таких аннотаций (код физически не виден gcov
в родительском процессе). Остальные throw-ветви аннотировать смысла нет до смены lcov.
