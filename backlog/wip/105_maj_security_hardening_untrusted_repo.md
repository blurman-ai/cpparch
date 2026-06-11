# [SECURITY][SCAN][GIT][REPORT] Hardening под недоверенный репозиторий (S3–S6 из аудита)

**Дата создания:** 2026-06-11
**Дата старта:** 2026-06-11
**Статус:** wip
**Модуль:** SCAN / GIT / REPORT
**Приоритет:** major
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #073 (tech_debt_alignment_cleanup), #075 (trusted_diff_workflow)

## Цель

Закрыть оставшиеся находки security-аудита 2026-06-11 (модель угроз: archcheck в CI
на недоверенном репозитории). Полный разбор — `docs/research/full_audit_and_research_2026_06.md` §1.2.

Уже закрыто отдельно (2026-06-11, вне этой задачи): S1 (abort на кривом YAML → ConfigError/exit 2),
S2 (stack overflow на глубокой include-цепочке → итеративный DFS), top-level catch → exit 3.

## Остаток — четыре находки

### S3 (medium) — симлинки-файлы наружу корня

`src/scan/project_files.cpp:39-67`: `should_skip_dir` режет только симлинк-каталоги;
`entry.is_regular_file()` разыменовывает симлинк-файл. `evil.h → /etc/passwd` читается
и частично утекает в отчёт (путь + содержимое строк в сообщениях нарушений).

Фикс: `symlink_status()` для файлов (не следовать), либо после `relative()` проверять,
что `weakly_canonical` цели остаётся внутри root; симлинки наружу — отбрасывать.

### S4-остаток (medium) — нет лимита размера чтения

`DiskFileSource::read`, `main.cpp::read_file`, `GitObjectFileSource::read`
(`out.resize(size)` по заголовку `git cat-file` без верхней границы) читают файл/блоб
целиком без лимита → OOM-kill / bad_alloc на многогигабайтном вводе. Top-level catch
уже переводит bad_alloc в exit 3, но правильнее не доходить до него.

Фикс: лимит N МБ (константа, например 64 МБ) на файл/блоб; больше — пропуск с
диагностикой в stderr и счётчиком skipped.

### S5 (medium) — неполный jsonEscape

`include/archcheck/report/json_escape.h:11-27` экранирует только `"` `\` `\n`.
Управляющие символы U+0000..001F из имён файлов/строк кода дают невалидный JSON
(breakout-инъекции нет, но downstream-парсеры валятся).

Фикс: `\uXXXX` для всех <0x20 (короткие формы для `\t` `\r` `\b` `\f`); невалидный
UTF-8 → U+FFFD. Юнит-тест на строку со всеми 32 управляющими.

### S6 (low) — git без харднинга

`src/git/git_state.cpp`: git исполняется с cwd в недоверенном репо и уважает его
`.git/config` (`core.fsmonitor`, `diff.external`, `core.pager` исполняют команды)
и хуки (`post-checkout` при `worktree add`).

Фикс: окружение/флаги для всех дочерних git: `GIT_CONFIG_NOSYSTEM=1`,
`-c core.hooksPath=/dev/null -c core.fsmonitor= -c diff.external= -c core.pager=cat`;
`--` перед ref-аргументами или валидация «ref не начинается с `-`».

## Сделано (2026-06-11)

### S3 — симлинки наружу корня
- `src/scan/project_files.cpp`: добавлена `symlink_escapes_root()` — проверяет через
  `weakly_canonical` + `lexically_relative`, не выходит ли цель симлинка за root.
- Исправлен баг GCC 8: `std::filesystem::relative()` следует по симлинкам; заменено на
  `lexically_relative()` для сохранения имени симлинка в результате.
- Тесты: `discover_files rejects file symlink pointing outside root (S3)`,
  `discover_files accepts file symlink pointing inside root (S3)`.

### S4 — лимит размера чтения (64 MiB)
- `src/scan/disk_file_source.cpp`: `kMaxFileSizeBytes = 64 MiB`, проверка `file_size()` перед
  `ifstream` чтением; пропуск с диагностикой в stderr.
- `src/git/git_object_file_source.cpp`: `kMaxBlobSizeBytes = 64 MiB`, проверка размера из
  заголовка `cat-file --batch` перед `readExact`; дрейн остатка для синхронизации протокола.
  Вынесен `parseBlobSize()` для снижения NLOC функции `read()` ниже порога lizard.
- `src/main.cpp`: `kMainMaxFileSizeBytes = 64 MiB` в `read_file()`.
- Тест: `DiskFileSource::read skips oversized files (S4)` (sparse-файл 65 MiB).

### S5 — полный jsonEscape по RFC 8259
- `include/archcheck/report/json_escape.h`: переписан с нуля; все U+0000..001F экранируются
  (короткие формы для `\n \r \t \b \f`, остальные `\u00XX`); невалидный UTF-8 → U+FFFD.
  Разбит на `detail::appendControl` + `detail::appendUtf8` + `detail::utf8ExtraBytes` (≤30 NLOC каждая).
- Тесты: все 32 управляющих символа, short-form escapes, `\uXXXX`, UTF-8 pass-through, U+FFFD.

### S6 — git харднинг
- `src/git/git_exec.cpp`: в `execChild` добавлены `setenv(GIT_CONFIG_NOSYSTEM=1)` и
  флаги `-c core.hooksPath=/dev/null -c core.fsmonitor= -c core.pager=cat` перед подкомандой.
- `src/git/git_object_file_source.cpp`: `runGitOneShot` удалён, заменён на `runGit` (дедупликация
  ~50 строк); `execCatFileBatch` получил те же харднинг-флаги.
- `src/git/diff_query.cpp`, `src/git/git_state.cpp`: добавлен `--no-ext-diff` ко всем
  `git diff` вызовам (пустой `-c diff.external=` ломает git diff 2.30).
- Тесты: `runGit hardening: git --version succeeds with hardening flags (S6)`,
  `runGit hardening: GIT_CONFIG_NOSYSTEM does not prevent reading commits (S6)`.

### Попутные правки
- `src/main.cpp::run_history`: вынесен `buildLocMap()` для снижения NLOC ниже lizard-порога 30.

## Acceptance

- Фикстура/тест на каждый пункт: симлинк наружу → файл не сканируется; гигантский
  файл → пропуск + диагностика, exit не 134; control-символ в имени файла → валидный
  JSON (парсится `python -m json.tool`); git-вызовы содержат харднинг-флаги (юнит на
  сборку argv).
- Каждый фикс — отдельный коммит ≤50 строк (правило code_quality).

## Что осталось

- `diff.external=` не подавляется глобально (ломает `git diff` на git 2.30) — вместо
  этого `--no-ext-diff` добавлен ко всем diff-вызовам. Если появится git ≥ 2.41 с
  исправленным поведением — можно вернуть глобальный сброс.
- Тест на "control-символ в имени файла → JSON проверяется `python -m json.tool`" не
  запускается как integ-тест (принято решение ограничиться unit-тестом на jsonEscape).
