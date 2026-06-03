Проверить, зелёный ли последний прогон CI на GitHub для текущей ветки.

Аргумент (optional): имя ветки или `<run-id>`. Без аргумента — текущая ветка (`git branch --show-current`).

Скилл смотрит на GitHub Actions через `gh`, а не собирает локально. Цель — за один вызов дать ответ «зелено / красно» и, если красно, назвать упавшие джобы с причиной.

## Инварианты

- **CI отражает запушенный HEAD, а не локальный.** Если есть незапушенные коммиты — статус на GitHub относится к более старому состоянию. Это нужно проговорить пользователю, а не молча выдать «зелено».
- **Только чтение.** Скилл ничего не чинит и не коммитит. Нашёл красное — докладывает; чинить — отдельно, по команде.
- **Не запускать сборку локально.** Для локальных гейтов есть свой путь (см. последний шаг) — но это не основной режим.

## Шаги

1. **Определить ветку и сверить с remote:**
   ```bash
   BR=$(git branch --show-current)
   git fetch -q origin "$BR" 2>/dev/null
   git log --oneline "origin/$BR..HEAD"   # незапушенные коммиты
   ```
   Если список непустой — запомнить: CI знает только про `origin/$BR`, локальный HEAD впереди на N коммитов.

2. **Найти последний прогон:**
   ```bash
   gh run list --branch "$BR" --limit 1
   ```
   Взять его run-id и статус (`completed`/`in_progress`, `success`/`failure`/`cancelled`).
   Если передан `<run-id>` аргументом — использовать его.

3. **Если прогон ещё идёт** (`in_progress`/`queued`) — сказать об этом и предложить подождать (можно опросить `gh run watch <id> --exit-status`, но не блокировать надолго без спроса).

4. **Если `success`** — доложить «🟢 CI зелёный на `<sha>` (`<commit subj>`)». Если HEAD впереди remote — добавить предупреждение из шага 1.

5. **Если `failure`/`cancelled`** — разобрать по джобам:
   ```bash
   gh run view <id>              # дерево джоб: видно, какие шаги X
   gh run view <id> --log-failed # логи только упавших шагов
   ```
   Для каждой упавшей джобы дать короткую причину (1–2 строки): какой шаг упал и ключевая строка ошибки. Не вываливать весь лог.

6. **Вывод** — таблица «джоба → статус → причина» и явный вердикт. Если HEAD впереди origin — напомнить, что часть проблем могла уже быть починена локально, и стоит запушить/перепроверить.

## Локальные гейты (по запросу, если CI красный и хочется проверить до пуша)

Те же проверки, что гоняет `.github/workflows/ci.yml`, в порядке «быстрые сначала».

⚠️ Версии инструментов на Astra ≠ CI, локально «чисто» может не значить «зелёный CI»:
- **clang-format**: системный 18.1.8 форматирует иначе, чем CI (18.1.3). Ставить pin:
  `python3 -m pip install --user --quiet 'clang-format==18.1.3'` → `$HOME/.local/bin/clang-format`.
- **cppcheck**: системный 1.86 старше CI-шного, пропускает часть проверок.
- **build/debug**: если сконфигурирован без `-Werror` — пропустит варнинги, на которых CI падает. CI гонит Debug с дефолтным `ARCHCHECK_ENABLE_WARNINGS=ON`.

```bash
CF="$HOME/.local/bin/clang-format"   # 18.1.3, как на CI

# 1. clang-format (как в CI)
find src include tests -name '*.h' -o -name '*.cpp' \
  | xargs "$CF" --dry-run --Werror --style=file

# 2. cppcheck (gate)
cppcheck --enable=warning,performance,portability --inline-suppr \
  --error-exitcode=1 --suppress=missingIncludeSystem --quiet -I include src/ include/

# 3. lizard (gate: NLOC≤30, CCN≤15, args≤5, только production)
lizard --CCN 15 -T nloc=30 --arguments 5 --warnings_only src/ include/

# 4. build + test (Debug, со строгими варнингами как на CI)
cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DARCHCHECK_ENABLE_WARNINGS=ON
cmake --build build/debug
( cd build/debug && ctest --output-on-failure )
```
