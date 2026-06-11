# [SECURITY][SCAN][GIT][REPORT] Hardening под недоверенный репозиторий (S3–S6 из аудита)

**Дата создания:** 2026-06-11
**Дата старта:** —
**Статус:** new
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

## Acceptance

- Фикстура/тест на каждый пункт: симлинк наружу → файл не сканируется; гигантский
  файл → пропуск + диагностика, exit не 134; control-символ в имени файла → валидный
  JSON (парсится `python -m json.tool`); git-вызовы содержат харднинг-флаги (юнит на
  сборку argv).
- Каждый фикс — отдельный коммит ≤50 строк (правило code_quality).
