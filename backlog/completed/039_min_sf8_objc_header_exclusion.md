# [RULES] SF.8: Objective-C заголовки пропускаются (не проверяются на C++ include guard)

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-28 (de-facto — пришло как побочка #034)
**Дата завершения:** 2026-05-29
**Статус:** done
**Модуль:** RULES
**Приоритет:** minor
**Сложность:** XS (по факту: ~30 строк + 2 теста)
**Блокирует:** —
**Заблокирован:** —
**Related:** #034 (sf8_scan_limit_and_inc_files — реализация ObjC-skip приехала рядом с .inc-skip), #028 (rules_engine_mvp)

## Цель

SF.8 не должен проверять Objective-C заголовки — у них другой механизм включения (`#import`, автоматически дедуплицирует) и другой синтаксис (`@interface` / `@implementation`). Без отдельного skip-а правило репортит ложные нарушения на ObjC-файлах в C++/ObjC mixed-проектах (grpc `examples/objective-c/`, любое iOS+C++).

## Контекст

Прогон на grpc (commit `05ffa92`) показал ложные SF.8 в `examples/objective-c/`:

```objc
// examples/objective-c/helloworld/HelloWorld/AppDelegate.h
#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end
```

Изначально задача была заведена как `[OUT-OF-SCOPE]` — "archcheck = C++ only, документируем как known limitation". Решение пересмотрено: skip ObjC дешевле, чем объяснять в FAQ, и сразу убирает шум для mixed-проектов, которых на практике много.

## Сделано

- **2026-05-28**, коммит `4bb56f9` (refactor: extract helpers): `isObjcFile()` добавлен в namespace-anon того же `sf8_include_guard.cpp`, заодно с разбиением `check()` под порог lizard (длина функции). ObjC-skip приехал тихо вместе с .inc-skip из #034.
- **2026-05-29**, коммит `a5ec300` (test(rules/sf8)): добавлены два регрессионных теста под `@interface` и `#import` маркеры. До этого функционал работал, но не был покрыт — рефакторинг `isObjcFile()` мог тихо сломаться.

## Как работает

SF.8 проходит по узлам графа, для каждого заголовка читает исходник и решает, репортить ли отсутствие include guard. Перед проверкой есть три early-exit:

1. **Не заголовок** — `isHeaderFile()` по расширению из `scan/project_files.h`.
2. **`.inc` фрагмент** — `isIncFile()` по расширению (см. #034). Эти файлы намеренно включаются один раз через `#if`, guard был бы вреден.
3. **ObjC-файл** — `isObjcFile()` сканирует первые 60 непустых строк исходника, ищет один из трёх маркеров:
   - `@interface`
   - `@implementation`
   - `#import`

   Первое попадание → файл считается ObjC, пропускаем. См. [src/rules/sf8_include_guard.cpp:27-48](src/rules/sf8_include_guard.cpp#L27-L48).

Если файл прошёл все три фильтра — запускается `hasIncludeGuard()` (поиск `#pragma once` или `#ifndef` в тех же 60 строках). Нет guard → violation.

**Почему 60 строк, а не 50:** покрывает Apache 2.0 boilerplate (~47 строк), типичный для ObjC-runtime файлов. Лимит задан общей константой `kScanLines` — тем же, что использует `hasIncludeGuard`, чтобы поведение «насколько глубоко смотрим» было консистентным.

**Почему любой из маркеров достаточно:**
- `@interface` / `@implementation` — однозначные ObjC-токены, в чистом C++ их не бывает.
- `#import` — формально GCC-расширение для C++ тоже умеет, но в чистом C++ практически не встречается (deprecated с 90-х); риск ложного skip ничтожен против выигрыша на ObjC-файлах с Apache copyright + `#import` в первой содержательной строке.

## Чем управляется

Ничем — реализация полностью встроена, нет ни CLI-флага, ни YAML-настройки. Это сознательно: «zero-config first» (см. CLAUDE.md). Если кому-то понадобится принудительно проверять ObjC-файл — можно будет добавить `--force-sf8-on-objc` или эквивалент, но пока запросов нет.

`kScanLines = 60` — единственный «магический» порог, общий с `hasIncludeGuard`. Кандидат на переезд в `Config` struct по задаче #041 (audit hardcoded strings).

## С чем связана

- **#034** — приёмная задача рядом с `.inc`-skip. ObjC-skip приехал той же логикой «не C++ заголовок → не проверяем», в том же коммите-рефакторинге.
- **#028** — rules engine MVP, в котором SF.8 регистрируется как intrinsic правило v0.1.
- **#041** — потенциально вынесет `kScanLines` в `Config`.

## Диагностика

- Тесты: `tests/unit/rules/sf8_include_guard_test.cpp:89-114` — два кейса (`@interface` и `#import`).
- Прогон на grpc/iOS-mixed проекте: 0 ложных SF.8 на `examples/objective-c/` после фикса (проверка ручная, грпц не закоммичен в fixtures).
- Если правило начинает репортить ObjC-файлы — первое подозрение: маркер сдвинулся за пределы `kScanLines` (например, длинный лицензионный блок > 60 строк). Поднять порог в `sf8_include_guard.cpp:16`.

## Изменённые файлы

| Файл | Изменение | Коммит |
|------|-----------|--------|
| `src/rules/sf8_include_guard.cpp` | `isObjcFile()` + ранний skip в `check()` | `4bb56f9` |
| `tests/unit/rules/sf8_include_guard_test.cpp` | +2 регрессионных теста (`@interface`, `#import`) | `a5ec300` |
