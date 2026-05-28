# [RULES][OUT-OF-SCOPE] SF.8: Objective-C заголовки не должны проверяться на C++ include guard

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** RULES
**Приоритет:** minor
**Сложность:** XS
**Примечание:** ObjC — не профиль инструмента (archcheck = C++ only). Решение: документировать как known limitation, не фиксить. Задача заведена для трекинга.
**Блокирует:** —
**Заблокирован:** —
**Related:** #034 (sf8_scan_limit_and_inc_files), #028 (rules_engine_mvp)

## Цель

SF.8 не должен проверять Objective-C заголовки — у них другой механизм включения (`#import`) и другой синтаксис (`@interface`).

## Контекст

Прогон на grpc (commit `05ffa92`) нашёл ложные SF.8 в `examples/objective-c/`:

```objc
// examples/objective-c/helloworld/HelloWorld/AppDelegate.h
#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end
```

ObjC-файлы используют `#import` (который встроенно защищает от двойного включения) и не требуют `#pragma once` / `#ifndef`. Наш сканер проверяет `.h` файл на C++ guard — не находит — репортит нарушение.

## Детектирование ObjC

Простой heuristic: если в первых N строках встречается `#import` или `@interface` / `@implementation` / `@protocol` — файл ObjC, пропустить SF.8.

```cpp
bool isObjcHeader(const std::string& source) {
    // scan first 50 lines
    return contains(source_head, "#import") ||
           contains(source_head, "@interface") ||
           contains(source_head, "@implementation");
}
```

## Plan

- [ ] `sf8_include_guard.cpp`: добавить `isObjcHeader` check, пропускать ObjC-файлы
- [ ] Проверить: grpc → 0 ложных SF.8 на `examples/objective-c/`
- [ ] Unit-тест: ObjC-файл с `@interface` → SF.8 pass

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/rules/sf8_include_guard.cpp` | ObjC detection + skip |
| `tests/unit/rules/sf8_test.cpp` | тест ObjC header |
