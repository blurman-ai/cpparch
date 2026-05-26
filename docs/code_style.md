# Руководство по стилю C++20 — cpparch

Источник: адаптировано из проекта gm (`docs/dev/code_style.md`). Цель — унифицированный, читаемый, ревью-дружелюбный C++20 код.

## Основные принципы форматирования

- Кодировка файлов — UTF-8 без BOM, перевод строки Unix (`\n`), длина строки ≤ 120 символов.
- Один публичный тип на файл; вспомогательные детали прячем во внутренний `namespace detail`.
- Любая функция или метод обязаны иметь проверяемые контракты (`[[nodiscard]]`, `noexcept`, явные пред-/постусловия).
- Не оставляем закомментированный код — используем систему контроля версий.

### Отступы и пробелы

- 3 пробела вместо табуляции (`IndentWidth: 3`). Это историческое требование, сохраняем единообразие.
- Скобки по стилю Allman: открывающая `{` на следующей строке, отступ 3 пробела.
- Одно пустое поле между логическими блоками, нет пустой строки сразу после `namespace`.

### Именование

- Макросы и константы препроцессора: `UPPER_SNAKE_CASE`.
- Пространства имён/модули: `lower_snake_case`, иерархия — `cpparch::subsystem`.
- Типы (`class`, `enum class`): `PascalCase`. Интерфейсы начинаются с `I` (`IRule`).
- Raw-структуры (POD) и их хелперы: `lower_snake_case`.
- Свободные функции: `lower_snake_case`.
- Публичные методы классов: `PascalCase` (`GetCount()`); `private`/`protected` методы — `camelCase` (`getElement()`).
- Поля классов: `_name`, compile-time константы: `kName`.
- Шаблонные параметры: `typename TValue`.

### Порядок содержимого файла

1. Комментарий лицензии (если нужен).
2. `#pragma once`.
3. `export module ...;` (для модулей).
4. `#include`/`import` в порядке: стандартная библиотека → сторонние → внутренние.
5. `using`/`constexpr` объявления.
6. Перечисления, структуры, интерфейсы.
7. Классы, функции, свободные утилиты.

### Работа с зависимостями

- Не используем `using namespace` в заголовках (это и наше же правило SF.7).
- Инклюды минимальны; конкретные заголовки, без `stdafx.h`.
- Для модулей описываем явные `export`/`import`; приватный API — в `module:` секции.

## Препроцессор и константы

Макросы — только для условной компиляции или платформенных атрибутов. Числовые константы — `constexpr`/`consteval`/`constinit`.

```cpp
#pragma once

#include <concepts>

namespace cpparch::config
{
inline constexpr std::size_t kMaxIncludeDepth = 10;
inline constexpr std::size_t kGodHeaderFanIn = 30;
} // namespace cpparch::config
```

## Перечисления

`enum class` с явным базовым типом. Старые `enum` — только для C API.

```cpp
enum class Severity : std::uint8_t
{
   info,
   warning,
   error
};
```

## Структуры и POD

`struct` — для POD и лёгких утилит, именуется `lower_snake_case`. Поля инициализируются прямо в объявлении, методы — `constexpr`/`noexcept`.

```cpp
struct source_location
{
   std::string file{};
   std::uint32_t line{};
   std::uint32_t column{};
};
```

## Абстрактные интерфейсы

Только чисто виртуальные функции и виртуальный деструктор. `[[nodiscard]]` и `noexcept` для явных контрактов.

```cpp
class IRule
{
public:
   virtual ~IRule() = default;

   [[nodiscard]] virtual std::string_view Id() const noexcept = 0;
   [[nodiscard]] virtual std::vector<Violation> Check(const ComponentGraph& graph) const = 0;
};
```

## Классы и реализации

- Конструкторы `explicit`.
- Список инициализации — построчно, в порядке объявления полей.
- Ресурсы — RAII через стандартную библиотеку.
- Правило пятёрки — явно `= default` или `= delete`.

## Функции и методы

- `constexpr`/`consteval`/`constinit` — где значение известно во время компиляции.
- `noexcept` — по умолчанию, если функция не бросает.
- Сложные возвращаемые значения — `auto` + trailing return type.
- Выходные параметры заменяем возвращаемыми значениями (`std::optional`, `std::pair`, `std::expected`).

## Управляющие конструкции

`if constexpr` для шаблонного выбора, range-based for для коллекций, `std::ranges` для композиции.

### Braceless conditionals

Тело из одной строки — фигурные скобки не обязательны.

```cpp
if (value < 0)
   return;

for (auto& item : items)
   item.update();
```

**Symmetric bracing**: если одна ветка многострочная — скобки нужны **везде**.

```cpp
// ✅ обе однострочные — скобки не нужны
if (ready)
   process();
else
   wait();

// ✅ else многострочная — скобки везде
if (ready)
{
   process();
}
else
{
   log_error();
   wait();
}

// ❌ асимметрия
if (ready)
   process();
else
{
   log_error();
   wait();
}
```

### Компактный switch

Простые однострочные действия — выравниваем по столбцам:

```cpp
switch (severity)
{
  case Severity::info:    formatInfo(out, v);    break;
  case Severity::warning: formatWarning(out, v); break;
  case Severity::error:   formatError(out, v);   break;
}
```

## C++20

- **Concepts** — для требований к шаблонным параметрам.
- **Ranges / views** — для композиции преобразований.
- **`std::span` / `std::string_view`** — буферы и строки без копирования.
- **`std::optional` / `std::variant`** — вместо флагов и неполных структур.
- **`consteval`/`constexpr`/`constinit`** — compile-time расчёты и гарантированная инициализация.
- **Атрибуты**: `[[nodiscard]]`, `[[maybe_unused]]`, `[[likely]]`, `[[unlikely]]`, `[[deprecated("reason")]]`.

## Комментарии

- Публичные API — Doxygen (`///` или `/** */`).
- Комментарии объясняют *почему*, а не *что*. Над блоком, не в конце строки.
- Сложные алгоритмы — ссылки на спецификацию (Lakos, Core Guidelines, Martin).

## Инструменты

- `clang-format` ≥ 16, в CI. Ключевые параметры: `IndentWidth: 3`, `ColumnLimit: 120`, `BreakBeforeBraces: Allman`.
- `clang-tidy` — профили `modernize-*`, `cppcoreguidelines-*`, `readability-*`.
- `cppcheck --enable=warning,style,performance` перед релизами.

## Дополнительно для cpparch

Поскольку cpparch — сам инструмент проверки архитектуры, **код cpparch обязан проходить cpparch** (dogfooding):

- Никаких циклов в include-графе.
- Никаких `using namespace` в `.h` (SF.7).
- Каждый `.h` self-contained, с `#pragma once` (SF.8, SF.11).
- Нет анонимных namespace в заголовках (SF.21).
- Каждое правило — отдельный файл, отдельный класс, регистрация через статическую таблицу (OCP).
