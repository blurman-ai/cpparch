# [CI][RELEASE] Prebuilt archcheck binary for downstream CI consumers

**Дата создания:** 2026-06-25
**Дата старта:** 2026-06-25
**Статус:** completed
**Модуль:** CI / RELEASE / DOCS
**Приоритет:** major
**Сложность:** M
**Блокирует:** быстрые downstream-интеграции `archcheck` в чужих CI без сборки из исходников
**Заблокирован:** —
**Related:** #136 (CLI/docs contract), #138 (release readiness), docs/ci_usage.md

## Цель

Дать пользователям `archcheck` нормальный install path для CI: скачать pinned
prebuilt binary из GitHub Release, проверить версию/checksum и сразу запускать
`archcheck`, не тратя 3-4 минуты каждого workflow run на `git clone cpparch` +
`cmake` + `FetchContent` + build.

## Контекст

В `leadline` `archcheck` был встроен по текущему гайду `docs/ci_usage.md` через
сборку из исходников внутри downstream workflow. Это работает, но первый реальный
run показал цену: job `Archcheck` занял около 4 минут, почти полностью на сборку
инструмента. Сам binary маленький: локальный Linux x86_64 release `archcheck` около
1.6 MB и зависит только от стандартных системных библиотек (`libstdc++`, `libm`,
`libgcc_s`, `libc`).

Для CLI-инструмента, который используется как gate в других репозиториях, сборка из
исходников должна быть fallback/developer path, а не основной recommended CI path.
Основной path: versioned release asset.

## Что сделать

- Добавить release workflow в `cpparch`, который на tag `vX.Y.Z` собирает
  `archcheck` для `ubuntu-24.04` / Linux x86_64.
- Упаковать binary в asset с предсказуемым именем, например:
  `archcheck-${version}-linux-x86_64.tar.gz`.
- Публиковать checksum рядом с binary (`.sha256`) и документировать проверку.
- В asset включить минимум:
  - `archcheck`;
  - `LICENSE`;
  - короткий `README`/install note, если полезно.
- Проверить, что скачанный asset исполняется на clean GitHub-hosted
  `ubuntu-24.04` runner и печатает корректный `archcheck --version`.
- Обновить `docs/ci_usage.md`:
  - recommended path: скачать pinned release binary;
  - fallback path: собрать из исходников;
  - запретить `latest` как CI default, рекомендовать pin по tag + checksum.
- Обновить `.github/workflows/example_archcheck_pr.yml`, чтобы install step
  использовал release binary, а build-from-source остался комментарием/fallback.

## Acceptance

- [x] Есть GitHub Release asset для Linux x86_64 с `archcheck` — **v0.1.0 выпущен**
      (https://github.com/blurman-ai/cpparch/releases/tag/v0.1.0), tarball 1.3 MB.
- [x] Есть checksum asset (`.sha256`) и пример проверки checksum в CI snippet
      (`ci_usage.md`, `example_archcheck_pr.yml`, release notes).
- [x] `docs/ci_usage.md` показывает быстрый install path через pinned release.
- [x] `example_archcheck_pr.yml` больше не требует CMake build в happy path
      (release-download в happy path, build-from-source — закомментированный fallback).
- [x] В отдельном smoke job скачанный release asset запускается
      (`--version`/`--help`/`.`) — job `smoke` в `release.yml`, зелёный на v0.1.0.
- [x] Downstream workflow может заменить build step на download/install без
      изменения `archcheck --diff` контракта (бинарь в `/usr/local/bin`, вызов `archcheck` без пути).

## Прогресс (2026-06-25)

Сделано (всё локально верифицировано, не закоммичено):
- `.github/workflows/release.yml` — новый. Триггер на тег `v[0-9]+.[0-9]+.[0-9]+(-*)`,
  Release-сборка на ubuntu-24.04 (`-static-libstdc++ -static-libgcc`), version-check
  бинаря против тега, упаковка `archcheck-X.Y.Z-linux-x86_64.tar.gz` (+LICENSE,
  README.install) + `.sha256`, `gh release create`, отдельный smoke-job на чистом runner.
  rc/alpha/beta-теги → prerelease.
- `docs/ci_usage.md` — секция «Установка в CI»: recommended pinned release + fallback
  build-from-source; оба сценария (full scan / PR diff) переведены на release-install;
  явный запрет mutable `latest`.
- `.github/workflows/example_archcheck_pr.yml` — happy path = release download+checksum,
  build-from-source закомментирован как fallback.
- `docs/dev/git_workflow.md` — шаг 7 release-процесса: GitHub Release теперь автоматический.
- `.claude/commands/release.md` — новая команда `/release X.Y.Z`: bump→changelog→build
  gate→commit→annotated tag→push (тег запускает release.yml). Подтверждение перед push.

Локальная верификация:
- Release-сборка с `-static-libstdc++ -static-libgcc` ОК; `ldd` = только libm/libc/ld
  (libstdc++/libgcc_s ушли), `--version` = `archcheck 0.1.0`.
- End-to-end pack→sha256→verify(ЦЕЛ)→extract→`archcheck --version`→`archcheck empty`(exit 0) — OK.

Закоммичено + запушено в master:
- 3bd3c07 — release pipeline + /release command.
- b79b96b — version-check vs tag numeric core (rc-теги не падают).
- 167bbf0 — fix checksum verify в release notes (rename-форма падала у потребителя;
  воспроизвёл обе формы локально: rename → exit 1, original-names → OK).

Dry-run на реальных GitHub-runner'ах (rc1, затем rc2 после фикса) — оба полностью
зелёные, после проверки оба прибраны (тег+release удалены). На чистом ubuntu-24.04:
checksum `...tar.gz: OK`, `archcheck 0.1.0`, `Usage:`, `No violations found.`.
Версия бинаря (статик-линк, ldd=libm/libc/ld) совпала с тегом. Asset ~1.3 MB gz.

Боевой релиз **v0.1.0 выпущен** (fed39d9 → tag v0.1.0):
https://github.com/blurman-ai/cpparch/releases/tag/v0.1.0, prerelease=false, 4 asset'а.

### Astra 1.7 / старый glibc (по ходу задачи)

Находка (скептик-проверка): динамический ubuntu-24.04 asset требует **glibc ≥ 2.38**
и НЕ запускается на Astra 1.7 (glibc 2.28) — `version 'GLIBC_2.38' not found`.
Решение (выбор юзера — «два asset'а»): второй asset собирается с `-static`
(libc вшита, зависимости от glibc нет; archcheck форкает git и не трогает
NSS/getpwnam → статическая glibc безопасна). Релиз теперь публикует оба:
- `archcheck-X.Y.Z-linux-x86_64.tar.gz` — динамический, glibc ≥ 2.38, меньше;
- `archcheck-X.Y.Z-linux-x86_64-static.tar.gz` — статический, любой Linux ≥ 3.2 (Astra/Debian10/RHEL8).

Доказано в CI: job `smoke-oldglibc` в контейнере `debian:10` (glibc 2.28 = база
Astra) скачивает static-asset и запускает — `ldd ... 2.28`, `archcheck 0.1.0`,
`No violations found.`, exit 0. Локально на этой машине (тоже Astra 1.7, glibc 2.28)
static-бинарь тоже работает (`ldd` → «не является динамическим…»).

Коммиты: c4350b9 (glibc-нота в доке), fed39d9 (static-asset + debian:10 smoke).
Dry-run rc3 — все 3 job'а зелёные, прибран.

Осталось (внешнее, на стороне пользователя):
- Проверить на `leadline`: заменить build-from-source на release-download
  (static-asset под Astra-раннер, если он там), full scan = `No violations found`.

## Итог

**Статус:** completed
**Дата завершения:** 2026-06-25

Release-пайплайн (`release.yml`) на тег `vX.Y.Z` собирает и публикует два Linux
x86_64 asset'а (dynamic glibc≥2.38 + fully-static) с `.sha256` и smoke-job'ами
(чистый runner + `debian:10` для старого glibc). `v0.1.0` выпущен и живой.
`docs/ci_usage.md` + `example_archcheck_pr.yml` дают готовый install path:
download → `sha256sum -c` → `install /usr/local/bin`, **по умолчанию static**
(работает на любом glibc, в т.ч. Astra 1.7 / ubuntu-22.04). `/release` команда
автоматизирует bump→changelog→tag→push.

**Изменённые файлы (финальный заход, commit 4bc33aa):**
- `docs/ci_usage.md` — дефолт install-сниппетов переведён на static-asset.
- `.github/workflows/example_archcheck_pr.yml` — пример тоже на static.
- `README.md` — указатель на `docs/ci_usage.md`.

Ранее (этой задачей): `release.yml`, `/release`, glibc-нота, static-asset +
debian:10 smoke — коммиты 3bd3c07, b79b96b, 167bbf0, c4350b9, fed39d9, 3bd9b14, 8a9d810.

**Внешний follow-up (вне cpparch):** проверить на `leadline` замену
build-from-source на release-download (static-asset под Astra-раннер).

## Не делать в этой задаче

- Не делать package manager интеграции (`apt`, Homebrew, pipx, npm) — это отдельный
  distribution layer после первого binary release.
- Не делать Docker action как основной path. Docker может быть follow-up, но для
  одного маленького CLI binary release asset проще, быстрее и прозрачнее.
- Не использовать mutable `latest` в документации как рекомендуемый CI input.

## Возможный CI snippet для потребителя

```yaml
- name: Install archcheck
  env:
    ARCHCHECK_VERSION: v0.1.0
  run: |
    gh release download "$ARCHCHECK_VERSION" \
      --repo blurman-ai/cpparch \
      --pattern "archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64.tar.gz" \
      --output /tmp/archcheck.tar.gz
    gh release download "$ARCHCHECK_VERSION" \
      --repo blurman-ai/cpparch \
      --pattern "archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64.tar.gz.sha256" \
      --output /tmp/archcheck.tar.gz.sha256
    cd /tmp
    sha256sum -c archcheck.tar.gz.sha256
    tar -xzf archcheck.tar.gz
    sudo install -m 0755 archcheck /usr/local/bin/archcheck
    archcheck --version
```

## Проверка

- На `cpparch`: release workflow dry-run/manual test на temporary tag.
- На `leadline`: заменить локально build-from-source install step на release download
  и убедиться, что full scan остаётся `No violations found`.

## Риски

- ABI/glibc совместимость: собирать на `ubuntu-24.04` удобно для текущих runners, но
  это не самый широкий Linux baseline. Для v0.1 достаточно, но в docs явно указать
  supported runner image.
- Supply chain: binary должен быть pinned по tag/checksum; mutable latest запрещён
  как default.
- Release process не должен зависеть от downstream репозитория.
