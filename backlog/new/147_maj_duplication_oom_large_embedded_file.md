# [BUG][SCAN] Клон-детектор жрёт ~2ГБ на embedded-data файле (нет size-капа на токенизацию)

**Дата создания:** 2026-06-26
**Дата старта:** 2026-06-26
**Статус:** wip
**Модуль:** SCAN / DUPLICATION
**Приоритет:** major
**Related:** #127 (vendored/generated exclusion), #052/#056 (clone detector), #072 (порт детектора)

## Симптом (дофуд на корпусе)

Корпус-прогон `archcheck --diff` по `leejet/stable-diffusion.cpp`: **каждый** процесс
жрёт 1.0–1.9 ГБ RSS. 8 воркеров по коммитам этой репы разом → ~10 ГБ → **swap в ноль**,
машина на грани трэшинга. Поймано глазами пользователя «archcheck жрёт до фига памяти».

## Корневая причина

В репе 6 файлов `src/tokenizers/vocab/*.hpp` размером **20–43 МБ** — это embedded
vocab/merges-данные токенизаторов (BPE-таблицы gemma/umt5/gpt-oss), зашитые как
`static const unsigned char ...[] = { 0x7b,0x22,... }`. Код они только притворяются.

1. Лежат в `src/…/*.hpp` → vendored/generated-фильтр (#127) их НЕ ловит (нет
   `vendor/`/`third_party/`-маркеров, расширение «исходниковое»).
2. 20–43 МБ < **64 MiB** read-cap (`git_object_file_source.cpp:29`, `disk_file_source.cpp:12`)
   → файл читается целиком.
3. `scanForDuplication` → `phase1TokenizeAndExtract` токенизирует его: 43МБ → ~10M+ токенов
   → token-seq + rawSeq + k-gram fingerprint-индекс (`fragmenter.cpp:107`, `clone_index.cpp:122`)
   = **~1.9 ГБ на процесс**.

Это и баг (missing guard), и оптимизация: 64МБ read-cap нормален для чтения, но абсурдно
высок для токенизации под клон-детект — рукописный исходник ~никогда не >1МБ.

## Фикс (сделан, 2026-06-26)

Кап поставлен в ЕДИНОМ чокпоинте — `lex()` (`token_normalizer.cpp:394`), а НЕ в duplication-
скане: скептик-проверка показала, что complexity (`local_complexity_metrics.cpp:377`) и
flag-arg (`diff_command.cpp:173`) ТОЖЕ идут через `duplication::lex(source)`. Кап только в
duplication дал 3.6ГБ (другие два пожирали). `lex` возвращает пусто при `source.size() >
kMaxLexBytes` (1 MiB, `token_normalizer.h`) → все три lex-скана разом скипают огромные файлы.
Граф-скан не через lex (preprocessor include-scan) — не затронут.

**Замер RSS (один `--diff` на stable-diffusion.cpp):**

| | peak RSS | wall |
|---|---|---|
| до | ~3.6 ГБ | 21 с |
| после | **1.0 ГБ** | **4.4 с** |

3.6× по памяти, 5× по времени. Для 8 воркеров: ~8ГБ вместо ~29ГБ → без swap-смерти.

## Сделано
- [x] Причина локализована (6 vocab `.hpp` 20–43МБ, k-gram индекс + token-векторы).
- [x] Скептик: cap только в duplication = 3.6ГБ (complexity+flag-arg тоже через lex). Перенёс в `lex()`.
- [x] Кап `kMaxLexBytes=1MiB` в `lex()` — единый чокпоинт для всех lex-сканов.
- [x] Регрессионный тест (`duplication_token_normalizer_test.cpp`, #147): >1МБ → пусто, чуть меньше → токенизируется.
- [x] Замер RSS до/после: 3.6ГБ→1.0ГБ, 21с→4.4с. Все 572 теста зелёные, dogfood 0.

## Архитектурный фикс (2026-06-26) — стадия исключения generated-data (Linguist-style, #127)

После ресёрча (Linguist/SonarQube/PMD/clang-tidy: все ИСКЛЮЧАЮТ generated/data up-front, не
процессят) добавлена content-эвристика `hasMinifiedContent` (`file_classification.h`) и вшита
в `AuthoredScope` (excluded + fromFiles): **средняя длина строки > 110** (порог Linguist),
считается на 64КБ-префиксе (не ходим по 43МБ), floor 4КБ. umt5.hpp = одна строка 2.5М символов
→ ловится мгновенно и выпадает из ВСЕХ сканов (граф/bool/clone/complexity), не только из lex.

**Замер (тот же коммит): 3.6ГБ→731МБ (4.9×), 21с→2.75с (7.6×).** Лучше lex-cap-only (1.0ГБ),
т.к. файл теперь не доходит до граф/bool. Dogfood 0 (наш код не задет), 573/573 теста.

- [x] generated-data детектор (minified avg-line>110) → исключение из всех сканов. Тест + фикстура.
- [ ] Остаточный ~730МБ = снапшот всё ещё ЧИТАЕТ контент перед классификацией. Prefix/streaming-read
      excluded-файлов — отдельный рефактор SourceSnapshot (не блокер).
- [ ] (опц.) `.gitattributes linguist-generated`/`-vendored` — бесплатный сигнал из репы, прочитать.
- [ ] bool_field (char-scan на changed-only) — лёгкий, но теперь и он скипает excluded-файлы.
