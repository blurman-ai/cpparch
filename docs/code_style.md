# Руководство по стилю C++20 — archcheck

База — **LLVM coding standards**. Два осознанных отступления:
**Allman braces** вместо attach и **`name_`** (Google C++ Style trailing
underscore) для нестатических полей. Любые другие отличия от LLVM-style —
это баг этого документа или `.clang-format`, поправьте PR-ом.

`.clang-format` в корне репо авторитативен для machine-checkable правил;
этот документ — для смысловых решений, которые форматтер не выразит.

## Почему Allman

Allman braces — одно из двух осознанных отступлений от LLVM-style в этом
репозитории. Причины: (а) более чёткое визуальное разделение объявления и
тела функции/класса; (б) исторически сложившееся локальное предпочтение
мейнтейнеров. Это stylistic-выбор, не технический — пожалуйста, не
открывайте PR на перевод в Attach.

## Почему `name_` на полях

Мы маркируем нестатические поля класса trailing underscore (`name_`, не
`_name`, не `m_name`, не bare). Причины: (а) ускоряет чтение тела функции —
отличить поле от локальной переменной или параметра можно глазом, без
перечитывания сигнатуры или скролла к объявлению класса; (б) это Google C++
Style — mainstream-конвенция в C++ OSS, не авторская придумка; (в)
технически безопаснее leading underscore (`_name`), который зарезервирован
стандартом в ряде контекстов. Это второе и последнее осознанное отступление
от чистого LLVM-style в этом репозитории.

## Базовые параметры форматирования

- Кодировка — UTF-8 без BOM, перевод строки Unix (`\n`).
- `IndentWidth: 2` (LLVM default, не 3 и не 4).
- `ColumnLimit: 120` (более терпимо чем LLVM-default 80; узкие колонки в современных IDE-ах не нужны).
- `BreakBeforeBraces: Allman` (наша девиация — см. выше).
- Один публичный тип на файл; вспомогательные детали — в анонимном namespace или `namespace detail`.
- Не оставляем закомментированный код — у нас есть git history.

## Именование

| Категория | Конвенция | Пример |
|-----------|-----------|--------|
| Namespaces | `lower_snake_case` | `archcheck::scan` |
| Types (`class`, `struct`, `enum class`, `enum`, `using`-aliases типов) | `PascalCase` | `DependencyGraph`, `IncludeKind`, `ProjectFile` |
| Methods + free functions | `lowerCamelCase` | `scanIncludes`, `addNode`, `computeScc` |
| Local variables + parameters | `lowerCamelCase` | `lineNo`, `sourceFile` |
| Fields (нестатические поля класса) | `lowerCamelCase` с **trailing underscore** (Google C++ Style) | `graph_`, `lineCount_` |
| Compile-time constants (`constexpr`, `consteval`) | `kPascalCase` | `kMaxIncludeDepth` |
| Preprocessor macros и `#define` | `UPPER_SNAKE_CASE` | `ARCHCHECK_FIXTURES_DIR` |
| Template parameters | `PascalCase` (без префикса `T`) | `Allocator`, `Predicate` |

**Что мы НЕ делаем:**
- Не префиксуем интерфейсы `I` (`IRule`). В C++ это C#/MFC-патрон, не наша школа. Используйте просто `Rule`.
- Не префиксуем поля `_` (leading underscore). Префикс с маленькой буквой формально легален, но `_Uppercase` и `__anything` зарезервированы стандартом, и многие линтеры по умолчанию ругаются на `_name`. **Используем trailing underscore `name_`** — см. «Почему `name_` на полях» выше.
- Не различаем `struct` и `class` именованием. Используйте `struct` для passive data carrier-ов (все поля public, нет инвариантов), `class` — для всего остального.

### Совместимость с реальной LLVM-кодовой базой

Канонический LLVM coding standard говорит `lowerCamelCase` для функций; в
реальной кодовой базе (LLDB, исторические части Clang) встречается и
`snake_case`. Для нового кода LLVM-комьюнити фактически дрейфует в
`lowerCamelCase` — мы фиксируем этот target и не оглядываемся на исторические
куски LLVM.

## Порядок содержимого файла

1. `#pragma once` (заголовок) / включения (cpp).
2. `#include`/`import` в порядке: стандартная библиотека → сторонние → внутренние (`"archcheck/..."`).
3. `using` объявления, не более одного-двух, в анонимном namespace в .cpp.
4. Объявления типов и функций.

`#include`-блоки разделены пустыми строками; clang-format с
`IncludeBlocks: Regroup` делает это автоматически.

## Препроцессор и константы

Макросы — только для условной компиляции, платформенных атрибутов или
прокидывания путей через build-систему (см. `ARCHCHECK_FIXTURES_DIR`).
Числовые константы — `constexpr` / `consteval`.

```cpp
#pragma once

#include <cstddef>

namespace archcheck::config
{

inline constexpr std::size_t kMaxIncludeDepth = 10;
inline constexpr std::size_t kGodHeaderFanIn = 30;

}  // namespace archcheck::config
```

## Перечисления

`enum class` с явным базовым типом, когда это релевантно. Старые `enum` —
только для C-API.

```cpp
enum class Severity : std::uint8_t
{
  Info,
  Warning,
  Error,
};
```

## Структуры

`struct` — для passive data carrier-ов. Все поля public, инициализированы
по месту, без инвариантов. **Поля struct — без trailing underscore**: они
часть публичного data-интерфейса, не internal state класса (это совпадает
с Google C++ Style).

```cpp
struct SourceLocation
{
  std::string file;
  std::uint32_t line = 0;
  std::uint32_t column = 0;
};
```

## Классы и интерфейсы

Чисто-виртуальные базы — обычный `class` (никакого `I`-префикса).
Виртуальный деструктор обязателен. Контракты — через атрибуты
(`[[nodiscard]]`, `noexcept`).

```cpp
class Rule
{
public:
  virtual ~Rule() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::vector<Violation> check(const ComponentGraph& graph) const = 0;
};
```

Для конкретных классов:

- Конструкторы — `explicit`, если не copy/move.
- Список инициализации построчно, в порядке объявления полей.
- Правило пятёрки — явно `= default` или `= delete`.
- Ресурсы — RAII через стандартную библиотеку.
- Нестатические поля — с trailing underscore (`name_`).

```cpp
class FileScanner
{
public:
  explicit FileScanner(const std::filesystem::path& root) : root_(root) {}

  [[nodiscard]] std::size_t scannedCount() const noexcept { return scannedCount_; }
  void scan();

private:
  std::filesystem::path root_;
  std::size_t scannedCount_ = 0;
};
```

## Функции и методы

- `[[nodiscard]]` на любой возврат, который имеет смысл проверять.
- `noexcept` по умолчанию, если функция не бросает.
- Возврат вместо output-параметров: `std::optional`, `std::pair`, `std::tuple`, `std::expected` (в v0.2+).
- Сложные возвращаемые типы — `auto` + trailing return type.

## Управляющие конструкции

`if constexpr` для шаблонного выбора, range-based `for` для коллекций,
`std::ranges` для композиции.

### Braceless conditionals

Тело из одной строки — фигурные скобки не обязательны:

```cpp
if (value < 0)
  return;

for (auto& item : items)
  item.update();
```

**Symmetric bracing**: если одна ветка `if/else` многострочная — скобки
нужны **везде**:

```cpp
// OK — обе ветки однострочные
if (ready)
  process();
else
  wait();

// OK — else многострочная → скобки везде
if (ready)
{
  process();
}
else
{
  logError();
  wait();
}

// НЕЛЬЗЯ — асимметрия
if (ready)
  process();
else
{
  logError();
  wait();
}
```

### Компактный switch

Однострочные действия — выравниваем по столбцам:

```cpp
switch (severity)
{
  case Severity::Info:    formatInfo(out, v);    break;
  case Severity::Warning: formatWarning(out, v); break;
  case Severity::Error:   formatError(out, v);   break;
}
```

## C++20

- **Concepts** для требований к шаблонным параметрам.
- **Ranges / views** для композиции преобразований.
- **`std::span` / `std::string_view`** — буферы и строки без копирования.
- **`std::optional` / `std::variant`** вместо флагов и неполных структур.
- **`consteval` / `constexpr` / `constinit`** для compile-time расчётов.
- **Атрибуты:** `[[nodiscard]]`, `[[maybe_unused]]`, `[[noreturn]]`, `[[likely]]`, `[[unlikely]]`.

## Комментарии

- Публичные API — Doxygen (`///` или `/** */`).
- Комментарии объясняют **почему**, не **что**. Над блоком, не в конце строки.
- Сложные алгоритмы — ссылки на спецификацию (Lakos, Core Guidelines, Martin).

## Инструменты

- `clang-format` ≥ 16 в CI. Параметры — см. корневой `.clang-format`.
- `clang-tidy` — профили `modernize-*`, `cppcoreguidelines-*`, `readability-*`, `bugprone-*`, `performance-*`.
- `lizard --CCN 15 --length 30 --arguments 5 --warnings_only` локально перед каждым пушем.

## archcheck-specifika

archcheck — сам инструмент проверки архитектуры, поэтому **dogfooding
обязателен**. Код archcheck обязан проходить archcheck в CI:

- Никаких циклов в include-графе (SF.9).
- Никаких `using namespace` в `.h` (SF.7).
- Каждый `.h` self-contained, с `#pragma once` (SF.8, SF.11).
- Нет анонимных namespace в заголовках (SF.21).
- Каждое правило — отдельный файл, отдельный класс, регистрация через статическую таблицу (OCP). Добавление нового правила не должно править существующие файлы правил.
