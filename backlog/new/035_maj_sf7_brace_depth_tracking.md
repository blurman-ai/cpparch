# [RULES] SF.7: не отслеживается глубина {} → ложные срабатывания внутри функций и лямбд

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** RULES
**Приоритет:** major
**Сложность:** S
**Блокирует:** —
**Заблокирован:** —
**Related:** #028 (rules_engine_mvp)

## Цель

SF.7 должен репортить только `using namespace` на глобальном уровне или уровне namespace, не внутри тел функций, методов и лямбд.

## Контекст

Прогон на Catch2 (commit `69e0473`) выявил ложные срабатывания. Два конкретных паттерна (проверено вручную):

**Паттерн 1 — `using namespace` внутри тела метода:**
```cpp
// catch_tostring.hpp:270
template<>
struct StringMaker<bool> {
    static std::string convert(bool b) {
        using namespace std::string_literals;  // SF.7 срабатывает — ложное
        return b ? "true"s : "false"s;
    }
};
```

**Паттерн 2 — `using namespace` внутри лямбды в макросе:**
```cpp
// catch_generators.hpp:251
#define GENERATE(...) \
    Catch::Generators::generate(...,
      [](){ using namespace Catch::Generators; return makeGenerators(__VA_ARGS__); })
                    // ^^^ SF.7 срабатывает — ложное, лямбда не глобальный scope
```

Текущая реализация (`sf7_using_namespace.cpp`) — построчный поиск `using namespace` без отслеживания `{}`-глубины. Любая строка с этим паттерном даёт нарушение.

## Решение

Добавить счётчик глубины вложенности `{}` в `scanFile`. Репортить `using namespace` только при `brace_depth == 0`.

```cpp
int braceDepth = 0;
for (each line) {
    braceDepth += count('{') - count('}');
    if (braceDepth == 0 && hasUsingNamespace(line))
        report();
}
```

Ограничения подхода:
- Строки-подсчёт `{}`— не настоящий парсер: строки и комментарии могут содержать фигурные скобки. Для большинства реальных заголовков это достаточно. Полный парсинг — libclang (отдельная тема, v0.2).
- Многострочные выражения (лямбда на нескольких строках) покрываются корректно — счётчик правильно растёт и падает.

## Plan

- [ ] `sf7_using_namespace.cpp`: добавить `braceDepth` счётчик в `scanFile`
- [ ] Проверить: Catch2 → 0 ложных SF.7 в `catch_tostring.hpp` и `catch_generators.hpp`
- [ ] Проверить: fmt, spdlog, abseil — нет регрессий
- [ ] Фикстура: `fixtures/sf7/pass_using_inside_function/` — `using namespace` внутри функции → pass
- [ ] Фикстура: `fixtures/sf7/fail_using_global/` — существующая, остаётся fail
- [ ] Unit-тест для нового паттерна

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/rules/sf7_using_namespace.cpp` | brace_depth tracking |
| `tests/unit/rules/sf7_test.cpp` | тест функционального scope |
| `fixtures/sf7/pass_using_inside_function/` | новая фикстура |
