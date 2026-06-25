# archcheck в CI — гайд по встраиванию

Точка входа для тех, кто встраивает archcheck в свой пайплайн. Два сценария:

1. **Анализ репы целиком** — гейт на каждый push в защищённую ветку.
2. **Diff на PR** — гейт только на регрессию, которую внесла конкретная PR.

Их можно использовать вместе или по отдельности. Ниже — что выбрать, готовые
сниппеты и контракт вывода. Глубокая механика `--diff` живёт в
[ci_integration.md](ci_integration.md), формат baseline-файла — в
[baseline_format.md](baseline_format.md).

## TL;DR — какой режим брать

| Хочу…                                                        | Команда                                  | Гейтит            |
|--------------------------------------------------------------|------------------------------------------|-------------------|
| Поймать архитектурные нарушения во всём дереве               | `archcheck <path>`                       | циклы (SF.9)      |
| Не падать на легаси-долге, ловить только новое               | `archcheck --baseline <file> <path>`     | новые нарушения   |
| Поймать дрейф графа против зафиксированного снапшота          | `archcheck --drift-baseline <file> <path>` | DRIFT.1/2/4.CYCLE |
| Поймать регрессию, которую внесла PR                         | `archcheck --diff <revspec> <path>`      | new/grown cycle, new god-header |

`<path>` по умолчанию — текущая директория. `compile_commands.json` для
shipped-правил (SF.7/8/9, Lakos, DRIFT) **не нужен** — работает быстрый
include-сканер.

## Контракт: exit codes

Одинаков во всех режимах (см. [CLAUDE.md](../CLAUDE.md) §«Exit codes»):

| Code | Значение                                   | Что делает CI |
|------|--------------------------------------------|---------------|
| `0`  | гейт чист (advisory-находки могут быть)     | зелёный       |
| `1`  | gated-нарушение                            | красный       |
| `2`  | config / IO / git ошибка, невалидный ввод  | красный       |
| `3`  | внутренняя ошибка                          | красный       |

**Ключевой принцип — advisory-first.** Exit `1` означает именно gated-находку
(цикл / дрейф / регрессия), а не «что-то вообще нашлось». SF.7/8, длина цепочки,
god-header, добавленные рёбра, рост NCCD, SATD и пр. — печатаются в отчёт, но
job **не** валят. Это намеренно: гейт узкий, чтобы не получить «5000 нарушений
на первом прогоне». Долг подмораживается через `--baseline`.

## Установка в CI

### Рекомендуемый путь: pinned release binary

Скачать готовый бинарь из GitHub Release — секунды вместо 3-4 минут сборки.
**Пинить по тегу и проверять checksum**; mutable `latest` в CI не использовать
(supply-chain risk + невоспроизводимость).

```yaml
- name: Install archcheck
  env:
    ARCHCHECK_VERSION: v0.1.0        # пин по тегу, не latest
    GH_TOKEN: ${{ github.token }}    # gh release download требует токен
  run: |
    asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64-static.tar.gz"
    cd /tmp
    gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset"
    gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset.sha256"
    sha256sum -c "$asset.sha256"     # упадёт, если бинарь подменили
    tar -xzf "$asset"
    sudo install -m 0755 archcheck /usr/local/bin/archcheck
    archcheck --version
```

Дальше в job вызывается просто `archcheck …` (в `PATH`). Платформа пока одна —
Linux x86_64.

#### Какой asset брать

Каждый релиз публикует **два** Linux x86_64 asset'а (у каждого свой `.sha256`).
Сниппеты выше и ниже по умолчанию берут **static** — он работает везде:

| Asset | Когда | glibc |
|-------|-------|-------|
| `archcheck-<v>-linux-x86_64-static.tar.gz` ← **дефолт** | любой дистрибутив, в т.ч. **Astra 1.7**, Debian 10, RHEL 8, `ubuntu-22.04` | не нужен (полностью статический) |
| `archcheck-<v>-linux-x86_64.tar.gz` | только свежие runners (`ubuntu-24.04`/`latest`), если важен размер (~1.3 vs 1.7 MB) | **нужен ≥ 2.38** |

Static собран с `-static` — libc вшита, зависимости от glibc нет вообще,
запускается на любом ядре Linux ≥ 3.2. Совместимость со старым glibc (2.28)
проверяется в CI smoke-job на контейнере `debian:10` (= база Astra 1.7).
archcheck форкает `git` и работает только с ФС (без NSS/`getpwnam`/`getaddrinfo`),
поэтому обычные подводные камни статической glibc тут не срабатывают.

Dynamic собран на `ubuntu-24.04` (`-static-libstdc++ -static-libgcc`:
libstdc++/libgcc вшиты, но остаётся зависимость от glibc). **Проверено: на
glibc < 2.38 падает с `version 'GLIBC_2.38' not found`** — т.е. на `ubuntu-22.04`
и любом не-bleeding-edge дистрибутиве. Берите его, только если runner заведомо
`ubuntu-24.04`/`latest` и важны лишние ~400 КБ. Чтобы переключиться — уберите
`-static` из суффикса asset'а в сниппете:
`asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64.tar.gz"`.

### Fallback / developer path: сборка из исходников

Если нужной платформы нет среди release-asset'ов или вы хотите собрать из своего
коммита (на любом glibc подойдёт static-asset, так что fallback нужен в основном
для не-x86_64 или из конкретного коммита):

```yaml
- name: Build archcheck
  run: |
    cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    # бинарь: ./build/src/archcheck
```

Зависимости тянутся FetchContent-ом (нужен интернет на первый билд; кэшируется
через `actions/cache` на `build/_deps`).

---

## Сценарий 1. Анализ репы целиком

Самый простой гейт: на каждый push прогнать дерево, упасть на циклах.

```yaml
name: archcheck (full scan)
on:
  push:
    branches: [main, master]

jobs:
  archcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4         # fetch-depth не важен — снапшот не нужен
      - name: Install archcheck           # см. «Установка в CI» — pinned release
        env:
          ARCHCHECK_VERSION: v0.1.0
          GH_TOKEN: ${{ github.token }}
        run: |
          asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64-static.tar.gz"
          cd /tmp
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset"
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset.sha256"
          sha256sum -c "$asset.sha256"
          tar -xzf "$asset"
          sudo install -m 0755 archcheck /usr/local/bin/archcheck
      - name: Run archcheck
        run: archcheck src/               # путь к коду проекта
```

Что увидите в логе:

```
a.h: [SF.9] cycle: a.h → b.h → a.h
1 violation(s) (SF.9: 1)
```

или (только advisory — job всё равно зелёный, exit 0):

```
a1.h: [Lakos.ChainLength] include chain depth 11 exceeds threshold 10
1 violation(s) (Lakos.ChainLength: 1)
note: 1 advisory finding(s) reported, not gated ... Use --baseline to track existing debt.
```

### Легаси-проект: подморозить долг через baseline

Если на первом прогоне сыпется существующий долг — зафиксируйте его один раз,
дальше гейт ловит только **новые** нарушения (аналог ArchUnit `FreezingArchRule`):

```bash
# один раз, локально или в отдельном job — закоммитить файл в репо
archcheck --save-baseline archcheck-baseline.json src/
```

```yaml
- name: Run archcheck (new violations only)
  run: archcheck --baseline archcheck-baseline.json src/
```

Формат файла — [baseline_format.md](baseline_format.md). Он ревьюится в PR'ах
как обычный код.

### Дрейф графа против снапшота

Альтернатива baseline, когда хочется приколоть к репо *проверенный снимок графа*
и ловить его дрейф (новые рёбра, выросшие циклы, нарушения SDP):

```bash
archcheck --save-graph-baseline graph.json src/   # один раз, в репо
```

```yaml
- run: archcheck --drift-baseline graph.json src/
```

Гейтят `DRIFT.1` (новое ребро в established-цикл), `DRIFT.2`, `DRIFT.4.CYCLE`;
`DRIFT.3`, `DRIFT.4.NEW`, `DRIFT.4.SDP` — advisory.

---

## Сценарий 2. Diff на PR (канонический)

Гейтит только то, что **внёс данный PR**: новый/выросший цикл, новый god-header.
PR, которая делает архитектуру строже, проходит зелёной даже на грязном легаси.

archcheck не парсит `.diff` — он сравнивает **два графа зависимостей** (baseline
vs current), материализуя оба состояния дерева через `git worktree`. Поэтому на
вход идёт git-revspec, а не патч.

```yaml
name: archcheck (PR diff)
on:
  pull_request:
    branches: [main, master]
  merge_group:                    # нужно для GitHub Merge Queue; имя job должно совпадать

jobs:
  archcheck-pr-diff:
    runs-on: ubuntu-latest
    permissions:
      pull-requests: write        # для sticky-комментария
    steps:
      - uses: actions/checkout@v4   # shallow (depth 1) — историю НЕ тянем
      - name: Fetch baseline snapshot   # один срез base, depth 1 (не вся история)
        run: |
          base="${{ github.base_ref }}"
          [ -z "$base" ] && base="${{ github.event.merge_group.base_ref }}"
          base="${base#refs/heads/}"
          git fetch --no-tags --depth=1 origin "+refs/heads/$base:refs/remotes/origin/$base"
          echo "ARCHCHECK_BASE=origin/$base" >> "$GITHUB_ENV"
      - name: Install archcheck   # pinned release — см. «Установка в CI»
        env:
          ARCHCHECK_VERSION: v0.1.0
          GH_TOKEN: ${{ github.token }}
        run: |
          asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64-static.tar.gz"
          cd /tmp
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset"
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset.sha256"
          sha256sum -c "$asset.sha256"
          tar -xzf "$asset"
          sudo install -m 0755 archcheck /usr/local/bin/archcheck
      - name: Run archcheck --diff
        id: archcheck
        run: |
          archcheck --diff "$ARCHCHECK_BASE..HEAD" . \
            | tee archcheck-diff.txt
        continue-on-error: true    # дать дойти до публикации отчёта
      - name: Publish to Step Summary
        if: always()
        run: cat archcheck-diff.txt >> "$GITHUB_STEP_SUMMARY"
      - name: Post sticky PR comment
        if: always() && github.event_name == 'pull_request'
        uses: marocchino/sticky-pull-request-comment@v2
        with:
          header: archcheck-diff
          path: archcheck-diff.txt
      - name: Fail if regression
        if: steps.archcheck.outcome == 'failure'
        run: exit 1
```

Готовый файл — [.github/workflows/example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).

**Канонический revspec для PR:** `origin/<base>..HEAD`. Форма
`<ref>` (без правой стороны) означает «baseline = ref, current = рабочее дерево».

**Истории git не нужно — нужен только срез base-дерева.** `archcheck --diff`
сравнивает два дерева (`git diff` = tree-vs-tree) и материализует одно дерево
через `git worktree`; ни `rev-list`, ни `merge-base` он не вызывает. Поэтому
вместо тяжёлого `fetch-depth: 0` (вся история — на больших репах это десятки
секунд впустую) берём **depth=1 дофетч одного base-рефа**. `actions/checkout` по
умолчанию даёт shallow HEAD; шаг «Fetch baseline snapshot» дотягивает base одним
снапшотом. `fetch-depth: 0` остаётся простым, но тяжёлым fallback'ом — см.
[ci_integration.md](ci_integration.md#почему-shallow-base-fetch).

> ⚠️ Base **обязан** дофетчиться. Если base-ref не резолвится, archcheck не падает,
> а сравнивает с пустым деревом (всё «добавлено») → ложный gate. Не пропускайте
> шаг дофетча и пиньте правильный base.

Каналы публикации в PR (Step Summary, sticky-comment, Check-run), merge queue,
submodules, edge-cases — подробно разобраны в [ci_integration.md](ci_integration.md).

---

## Машиночитаемый вывод

`--format=json` даёт стабильную схему (`version: 1`) с top-level `gate`,
`gating`, `advisory` — для своих интеграций вместо парсинга текста:

```bash
archcheck --format json src/
archcheck --diff --format=json "origin/main..HEAD" .
```

`stdout` — отчёт (text или JSON). `stderr` — диагностика (git-ошибки, не-репо).
`exit code` — гейт. SARIF / GitHub Code Scanning — опциональный канал v0.2.

## Локальная проверка перед push

```bash
git fetch origin main
archcheck --diff origin/main..HEAD .   # как в CI
archcheck --diff HEAD~1 .              # против предыдущего коммита (current = worktree)
archcheck src/                          # полный скан дерева
```

## Связанные доки

- [ci_integration.md](ci_integration.md) — глубокая механика `--diff`: git-worktree,
  revspec, merge queue, submodules, каналы публикации, edge-cases.
- [baseline_format.md](baseline_format.md) — формат baseline/graph-baseline файлов.
- [README.md](../README.md) — питч, полный список правил и порогов.
- `archcheck --help` — авторитетный список режимов и дефолтов.
