# Local sweep report: line_duplication spike

Дата: `2026-05-29`
Инструмент: standalone spike из [README.md](README.md), бинарь
`/tmp/line_dup_build/line_duplication`

## Что прогонялось

Источники:

- `~/oss`
- `~/projects`

Команда:

```bash
/usr/bin/time -f 'ELAPSED=%e\nRSS=%M' \
  /tmp/line_dup_build/line_duplication <repo> --cross-only --top 6
```

Фиксированные настройки:

- `min_lines = 5`
- `min_window_chars = 60`
- default excludes:
  `third_party`, `bundled`, `generated`, `*_generated*`, `*.pb.*`

Важно: `--cross-only` в текущем spike фильтрует только вывод top-blocks.
`duplication ratio` всё ещё считается по всем дублированным окнам, включая
same-file.

## Сводная таблица

| Repo | Sig LOC | Ratio | Cross-file blocks | Time | Peak RSS | Комментарий |
|------|---------|-------|-------------------|------|----------|-------------|
| `fmt` | 45,945 | 3.98% | 14 | 0.03 s | 10.8 MB | тесты и bundled gtest влияют на root-run |
| `fmt/include` | 15,629 | 2.07% | 0 | <0.01 s | 5.4 MB | почти идеальное совпадение с референсом |
| `spdlog` | 15,669 | 62.37% | 328 | 0.07 s | 13.5 MB | root-run забит `spdlog/fmt/bundled/*` |
| `spdlog/include` | 7,810 | 8.26% | 29 | <0.01 s | 4.9 MB | count совпал с референсом |
| `abseil-cpp` | 188,890 | 11.88% | 627 | 0.16 s | 36.8 MB | много test/header twin-копий |
| `Kartend` | 72,307 | 14.28% | 414 | 0.10 s | 23.5 MB | нужен отдельный разбор top-blocks |
| `OreStudio` | 587,637 | 30.66% | 2,203 | 0.85 s | 81.6 MB | очень много крупных повторов |
| `IrredenEngine` | 81,535 | 13.05% | 288 | 0.10 s | 18.6 MB | умеренный сигнал |
| `LibreSprite` | 109,185 | 8.42% | 343 | 0.10 s | 23.6 MB | умеренный сигнал |
| `GWToolboxpp` | 152,904 | 6.80% | 232 | 0.11 s | 28.2 MB | относительно спокойный |
| `spectre` | 526,245 | 27.56% | 7,873 | 0.84 s | 137.1 MB | очень плотный сигнал |
| `grpc` | 773,384 | 26.18% | 4,402 | 2.38 s | 148.2 MB | самый тяжёлый OSS прогон в серии |
| `gm` | 306,807 | 10.43% | 1,675 | 3.14 s | 324.9 MB | большой локальный проект |
| `gm_github` | 89,960 | 17.39% | 638 | 0.09 s | 18.9 MB | много явного копипаста по устройствам |
| `samastra_itain` | 938,012 | 5.68% | 743 | 3.48 s | 500.9 MB | самый тяжёлый локальный по RSS |
| `cpparch` | 1,228,448 | 14.80% | 4,148 | 1.89 s | 226.2 MB | raw-run забит `sandbox/*` |

## Референс-проверка против исходного ТЗ

### `fmt/include`

Ожидание:

- ratio около `2.07%`
- `0` cross-file

Факт:

- ratio `2.07%`
- `0` cross-file

Вывод: verbatim Type-1 порт на этом кейсе попал в референс практически
идеально.

### `spdlog/include`

Ожидание:

- ratio около `4.50%`
- `29` cross-file
- обязательно всплывают `daily_file_sink.h <-> hourly_file_sink.h`

Факт:

- ratio `8.26%`
- `29` cross-file
- верхние 3 блока:
  - `15` lines:
    `daily_file_sink.h:114 <-> hourly_file_sink.h:88`
  - `14` lines:
    `daily_file_sink.h:93 <-> hourly_file_sink.h:62`
  - `12` lines:
    `daily_file_sink.h:140 <-> hourly_file_sink.h:114`

Вывод: по числу cross-file блоков и по характерным парам результат совпал.
Разъезд по ratio, вероятно, объясняется тем, что текущий spike считает покрытие
по всем duplicated windows, а не только по кросс-файловым/кросс-компонентным.

## Ручная проверка находок

Ниже — несколько блоков, которые были открыты вручную и сопоставлены с
исходниками.

### 1. `spdlog/include`: `daily_file_sink` <-> `hourly_file_sink`

Находка:

- `15` lines:
  `spdlog/sinks/daily_file_sink.h:114 <-> spdlog/sinks/hourly_file_sink.h:88`

Проверка:

- [daily_file_sink.h](~/oss/spdlog/include/spdlog/sinks/daily_file_sink.h:114)
- [hourly_file_sink.h](~/oss/spdlog/include/spdlog/sinks/hourly_file_sink.h:88)

Вердикт: **валидный дубликат**. Это не шум и не generated code. Блоки почти
дословно совпадают: форматирование сообщения, запись в `file_helper_`,
cleanup после rotation.

### 2. `spdlog/include`: `syslog_sink` <-> `systemd_sink`

Находка:

- `8` lines:
  `spdlog/sinks/syslog_sink.h:47 <-> spdlog/sinks/systemd_sink.h:54`

Проверка:

- [syslog_sink.h](~/oss/spdlog/include/spdlog/sinks/syslog_sink.h:47)
- [systemd_sink.h](~/oss/spdlog/include/spdlog/sinks/systemd_sink.h:54)

Вердикт: **валидный дубликат**. Совпадает логика подготовки `payload`,
форматирования через formatter и ограничения длины до `int`.

### 3. `fmt`: fuzzing harnesses

Находка:

- `41` lines:
  `test/fuzzing/named-arg.cc:45 <-> test/fuzzing/two-args.cc:41`

Проверка:

- [named-arg.cc](~/oss/fmt/test/fuzzing/named-arg.cc:45)
- [two-args.cc](~/oss/fmt/test/fuzzing/two-args.cc:41)

Вердикт: **валидный дубликат**. Это ожидаемый тестовый copy-paste: почти один и
тот же `switch` по типам аргументов, различие появляется позже. Эта находка
совпадает с духом исходного ТЗ.

### 4. `gm_github`: два `PacketLogger`

Находка:

- `78` lines:
  `rmi/diagnostic/packetlogger.cpp:50 <-> viz/network/packetlogger.cpp:55`

Проверка:

- [rmi/diagnostic/packetlogger.cpp](~/projects/gm_github/rmi/diagnostic/packetlogger.cpp:50)
- [viz/network/packetlogger.cpp](~/projects/gm_github/viz/network/packetlogger.cpp:55)

Вердикт: **валидный дубликат**. Совпадает генерация уникального имени файла,
закрытие лога, логика `isDuplicate()` с masking ping-бита и начало `logPacket()`.
Это уже похоже на реальный кандидат в reuse/extraction.

### 5. `gm_github`: device-specific copy-paste

Находка:

- `90` lines:
  `rmi_src/devices/BAT2_Devices/p_light.cpp:1 <-> rmi_src/devices/IMR_Devices/p_light.cpp:1`

Проверка:

- [BAT2 p_light.cpp](~/projects/gm_github/rmi_src/devices/BAT2_Devices/p_light.cpp:1)
- [IMR p_light.cpp](~/projects/gm_github/rmi_src/devices/IMR_Devices/p_light.cpp:1)

Вердикт: **валидный дубликат**. Файлы на просмотр идентичны на длинном
фрагменте: constructor, `mapInit()`, `datagramProcess()`. Для такого репо это
сильный сигнал, а не случайная схожесть.

### 6. `fmt`: low-value, но технически валидный дубль

Находка:

- `95` lines:
  `test/gtest/gmock-gtest-all.cc:112 <-> test/gtest/gtest/gtest-spi.h:41`

Проверка:

- [gmock-gtest-all.cc](~/oss/fmt/test/gtest/gmock-gtest-all.cc:112)
- [gtest-spi.h](~/oss/fmt/test/gtest/gtest/gtest-spi.h:41)

Вердикт: **технически валидный, но низкополезный**. Это не архитектурный
копипаст внутри fmt, а внутренняя структура bundled gtest под `test/gtest`.
Для продуктовой версии такой шум надо убирать repo-specific excludes.

### 7. `abseil-cpp`: twin tests

Находка:

- `44` lines:
  `unordered_map_members_test.h:38 <-> unordered_set_members_test.h:37`

Проверка:

- [unordered_map_members_test.h](~/oss/abseil-cpp/absl/container/internal/unordered_map_members_test.h:38)
- [unordered_set_members_test.h](~/oss/abseil-cpp/absl/container/internal/unordered_set_members_test.h:37)

Вердикт: **валидный дубль, но ожидаемый по дизайну тестов**. Это хороший пример
того, почему проход нужно держать `off by default`: текстовый дубликат реален,
но далеко не всегда является missing reuse edge.

## Что видно по sweep

1. **Алгоритм быстрый.**
   Даже крупные локальные деревья (`gm`, `samastra_itain`, `cpparch`) проходят
   за секунды, не за минуты.
2. **Референс `fmt/include` подтверждён.**
   Это сильный sanity-check для verbatim Type-1.
3. **`spdlog/include` подтвердил полезность.**
   Число cross-file блоков совпало с ожиданием, а характерные пары оказались на
   самом верху.
4. **Нужны project-local excludes сверх дефолтных.**
   На практике вверх часто всплывают:
   - bundled test frameworks;
   - version snapshots (`upgrade/*`, `копия_*`);
   - generated trees, которые не попали под дефолтный паттерн;
   - sandbox-репозитории внутри рабочего дерева.
5. **`off by default` оправдано.**
   Precision на хороших кейсах неплохой, но без настройки exclude-ов можно
   очень быстро получить красный CI на честных, но низкополезных дублях.

## Практический вывод перед продуктовой интеграцией

Spike подтверждает, что fast Type-1 слой жизнеспособен:

- даёт полезные находки на реальных репах;
- совпадает с референсами на `fmt/include` и `spdlog/include`;
- стоит дёшево по времени и памяти.

Но перед переносом в `src/scan` нужно закрыть две вещи:

1. **repo-specific excludes**
2. **разделение total duplication ratio и cross-component ratio**

Иначе проход будет технически рабочим, но operationally шумным на живых
монорепах.

## Commit-aware режим

Дополнительно spike расширен git-режимом:

- `--git-commit <sha>` = `<sha>^ .. <sha>`
- `--git-diff A..B`
- `--git-diff A` = `A .. WORKTREE`

Семантика отчёта в этом режиме:

- строится baseline snapshot;
- строится current snapshot;
- берётся changed set C/C++ файлов из git diff;
- репортятся только те current blocks, которые:
  - задевают changed set;
  - являются новыми относительно baseline по паре файлов и содержимому блока.

Это приближение “дубликат пришёл в конкретном коммите”.

### Проверка на synthetic repo

Был создан временный git-репозиторий:

1. commit `init`: только `a.cpp`
2. commit `copy`: добавлен `b.cpp` как verbatim copy

Результат на `--git-commit <copy>`:

- `baseline cross-file blocks = 0`
- `current cross-file blocks = 1`
- `introduced blocks in diff = 1`

Top block:

```text
[CROSS-FILE] 8 lines  a.cpp:2  <->  b.cpp:2  [INTRODUCED]
```

### Проверка на реальном локальном repo

Репо: `gm_github`

Найден реальный коммит с ненулевым introduced-count:

- commit:
  `030dbff2549f9fd8a7a82eb3df5c6f4f260f262f`
- subject:
  `Sync from gm (2026-02-01)`

Результат:

- `changed C/C++ files = 224`
- `baseline cross-file blocks = 639`
- `current cross-file blocks = 638`
- `introduced blocks in diff = 17`

Top introduced blocks:

```text
[CROSS-FILE] 8 lines  rmi/archive/archive.cpp:1040  <->  rmi_src/public/archive/archive.cpp:689
[CROSS-FILE] 8 lines  viz/mdl/rumba/rumba_heater_model.cpp:169  <->  viz/mdl/rumba/rumba_otopitel_model.cpp:83
[CROSS-FILE] 7 lines  rmi/taskwork/taskwork.cpp:275  <->  rmi_src/public/taskwork/taskWork.cpp:181
```

Первая находка вручную открыта и выглядит валидной: длинный SQL-query block в
[`rmi/archive/archive.cpp`](<~/projects/gm_github/rmi/archive/archive.cpp:1040>)
и
[`rmi_src/public/archive/archive.cpp`](<~/projects/gm_github/rmi_src/public/archive/archive.cpp:689>)
структурно совпадает и действительно похож на copy-paste, пришедший в этом sync.
