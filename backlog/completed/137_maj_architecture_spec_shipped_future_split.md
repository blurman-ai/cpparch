# [DOCS][SPEC] Развести shipped и future в architecture-spec

**Дата создания:** 2026-06-23
**Дата старта:** 2026-06-23
**Статус:** completed
**Модуль:** DOCS / SPEC
**Приоритет:** major
**Сложность:** M
**Блокирует:** доверие к `docs/architecture-spec.md` как к главному продуктово-архитектурному документу
**Заблокирован:** —
**Related:** #006 (spec_refactor), #045 (docs_sync_roadmap_mvp_spec), #051 (config_loader_v1), #139 (core_gate_advisory_positioning), future/#005 (sarif_reporter_spec), future/#042 (clang_semantic_backend), future/#050 (sf21_anonymous_namespace)

> **Порядок:** идёт ПОСЛЕ #139. Таксономию слоёв (core gate / structural advisory / hygiene advisory / history analytics) определяет #139; эта задача лишь применяет её к spec и разводит shipped/future. Не определять слои здесь заново — иначе spec и README разойдутся в терминах.

## Цель

Сделать `docs/architecture-spec.md` честным документом: shipped-поведение описывается как shipped, а будущие флаги/режимы/правила не маскируются под текущий CLI-контракт.

## Контекст

Сейчас `architecture-spec.md` одновременно играет роль product spec, roadmap и quasi-reference, из-за чего внутри него накопились ложные текущие обещания.

- TL;DR и раздел `Что делает` подают YAML module rules как ядро продукта, хотя `docs/MVP.md`
  и `--help` прямо говорят: в v0.1 они только парсятся и валидируются, но не enforce'ятся:
  [docs/architecture-spec.md#L32-L40](../../docs/architecture-spec.md#L32-L40),
  [docs/architecture-spec.md#L136-L150](../../docs/architecture-spec.md#L136-L150),
  [docs/MVP.md#L33-L36](../../docs/MVP.md#L33-L36),
  [src/main.cpp#L25-L29](../../src/main.cpp#L25-L29).
- Раздел `Команды` показывает флаги и команды, которых в бинаре нет:
  `--with-clang`, `--compile-commands`, `--format sarif`, `--metrics`:
  [docs/architecture-spec.md#L455-L479](../../docs/architecture-spec.md#L455-L479).
- Раздел `Управление правилами` обещает `--disable=...` и inline suppression-комментарии,
  но в коде таких контрактов нет:
  [docs/architecture-spec.md#L237-L242](../../docs/architecture-spec.md#L237-L242).
- Технологический стек в spec остаётся «ryml или yaml-cpp», «Catch2 или GoogleTest»,
  хотя shipped-репо уже давно фиксирован на `ryml` + `Catch2`:
  [docs/architecture-spec.md#L510-L517](../../docs/architecture-spec.md#L510-L517),
  [CMakeLists.txt#L46-L64](../../CMakeLists.txt#L46-L64).

Проблема не в самом future scope, а в том, что граница между `есть` и `будет` размыта.

## План выполнения

- [ ] Разделить в spec продуктовый current-state и roadmap-примеры: текущие команды оставить в reference, будущие увести в v0.2+/appendix.
- [ ] Переписать TL;DR и `Что делает`, чтобы v0.1 описывался как zero-config intrinsic rules + validated config surface, а не как enforced YAML architecture DSL.
- [ ] Явно пометить неподдержанные сегодня флаги/форматы (`--with-clang`, SARIF, `--metrics`, suppressions) как future-only.
- [ ] Синхронизировать tech-stack и терминологию spec с реальным репо (`ryml`, `Catch2`, flat `src/rules/`, фабрика `rule_set.cpp`).
- [ ] Перечитать `Stability contract`: не обещает ли он интерфейсы, которых пока физически нет.

## Сделано

- `docs/architecture-spec.md` поднят до v2.4.
- TL;DR и `Что делает` описывают shipped v0.1 как zero-config physical design gate + advisory regression layer.
- YAML module-rule enforcement, libclang, SARIF, `--metrics`, `--output`, rule toggles and suppressions явно помечены future-only.
- CLI reference в spec сверена с фактическим `--help`.
- Tech stack синхронизирован с репозиторием: `ryml`, Catch2, CMake/Ninja, fast preprocessor backend.
- Roadmap переставлен: v0.2 содержит runtime enforcement YAML module rules и semantic backend.

## В работе

- (пусто)

## Следующие шаги

- Нет. Следующий spec update должен начинаться с проверки `archcheck --help` и `CHANGELOG.md`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Не выпиливать future scope, а жёстко маркировать его | roadmap полезен, вредна именно неясная граница между `now` и `later` |
| Сверять команды только с реальным `--help` | spec не должен придумывать текущий CLI поверх бинаря |
| Править и продуктовые разделы, и technology section | иначе останется скрытый конфликт «что обещаем» vs «на чём реально собрано» |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `docs/architecture-spec.md` | развести shipped/future, переписать команды и product sections |
| `docs/MVP.md` | при необходимости уточнить ссылки на spec/roadmap |
| `README.md` | если после правки spec выявится новый внешний рассинхрон |
