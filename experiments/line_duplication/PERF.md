# line_duplication perf / field notes

Дата: `2026-05-29`
Бинарь: standalone spike из `experiments/line_duplication/`
Сборка: `Release`, GCC 8.3, build dir `/tmp/line_dup_build`

## Команда

```bash
/usr/bin/time -v /tmp/line_dup_build/line_duplication <repo> --cross-only --top 12
```

Для локально-подчищенных прогонов добавлялись project-local excludes:

```bash
--exclude sandbox
--exclude upgrade
```

## Результаты

| Repo | Extra exclude | Discovered | Eligible | Sig LOC | Dup LOC | Ratio | Blocks | Cross-file | Time | Peak RSS |
|------|---------------|------------|----------|---------|---------|-------|--------|------------|------|----------|
| `gm_github` | — | 949 | 948 | 89,960 | 15,645 | 17.39% | 809 | 638 | 0.16 s | 19 MB |
| `unigine_project` | — | 722 | 721 | 156,274 | 77,788 | 49.78% | 893 | 438 | 0.18 s | 26 MB |
| `unigine_project` | `upgrade` | 208 | 207 | 76,479 | 10,722 | 14.02% | 652 | 226 | 0.08 s | 17 MB |
| `Imitac_model` | — | 97 | 94 | 27,672 | 5,550 | 20.06% | 151 | 28 | 0.03 s | 8 MB |
| `cpparch` | — | 4,768 | 4,619 | 1,228,448 | 181,753 | 14.80% | 9,557 | 4,148 | 1.86 s | 231 MB |
| `cpparch` | `sandbox` | 209 | 86 | 7,949 | 815 | 10.25% | 61 | 30 | 0.62 s | 5 MB |

## Наблюдения

1. Алгоритм быстрый. Даже на сильно зашумлённом `cpparch` с `sandbox/` внутри
   укладывается примерно в `1.9 s / 231 MB`.
2. Default-excludes недостаточно для repo-local snapshot/upgrade-каталогов.
   Это видно на `unigine_project`: версия с `utils/upgrade/*` даёт почти `50%`
   duplication ratio; после `--exclude upgrade` остаётся уже более правдоподобный
   сигнал (`14.02%`).
3. Для `gm_github` выход выглядит предметно уже без дополнительной настройки:
   верхние кросс-файловые блоки похожи на реальный copy-paste, а не на vendored
   шум.
4. Для `Imitac_model` наверх всплывают:
   - явные файловые копии (`gear - копия_...`);
   - `moc/*`, что ожидаемо для Qt-генерации.
   Значит, для продуктовой версии стоит ожидать project-local excludes поверх
   дефолтных.
5. Для `cpparch` без `--exclude sandbox` метрика бесполезна: её забивают
   `sandbox/drift_repos/*`. После исключения `sandbox` появляются уже
   осмысленные короткие дубли в собственных `src/` и `tests/`.

## Smoke checks

- Verbatim cross-file copy с разными пустыми строками и comment-only lines:
  найден 1 блок длиной 8 строк.
- Тривиальные окна из `if/{/}/else/return;`:
  0 блоков.
- Дубли внутри `bundled/`:
  0 блоков, каталог исключён по умолчанию.

## Вывод

Для fast Type-1 слоя идея жизнеспособна:

- runtime низкий;
- память скромная;
- верхние блоки на реальных деревьях читаемы;
- но без project-local excludes отчёт легко тонет в version snapshots,
  sandbox-деревьях и генерации.

Следующий шаг перед продуктовой интеграцией: договориться о механике
repo-specific excludes и только потом переносить этот код в `src/scan`.
