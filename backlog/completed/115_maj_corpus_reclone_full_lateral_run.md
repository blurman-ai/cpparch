# [GRAPH] Переклон 297 реп корпуса + полный lateral-прогон + нормированный agentic-разрез

**Дата создания:** 2026-06-12
**Дата старта:** 2026-06-12
**Статус:** done
**Модуль:** GRAPH][SCAN
**Приоритет:** major
**Сложность:** medium
**Исполнитель:** Sonnet/Opus (НЕ Haiku: сеть, объёмы, статистические решения)
**Блокирует:** —
**Заблокирован:** —
**Related:** #111 (lateral corpus run — покрыл 183/481), #109 (lcx corpus)

## Цель

Поднять покрытие lateral-критерия с 183 до ~481 реп корпуса (переклонировав
297 удалённых с диска) и сделать нормированный agentic vs human разрез
(repo fixed effects) — 12 agent-событий на 183 репах для вывода мало.

## Контекст

Решение принято 2026-06-12 («по 2 и 3 надо клонировать, ничего не остаётся»).

- Прогон #111: 98 событий на 183 репах, TP ≈ 87% — критерий валиден; вопрос
  agentic-различий остался открыт из-за мощности.
- Список недостающих: `experiments/ai_repo_run/baselines/manifest.tsv`,
  строки `status == repo_missing` (297 шт). GitHub-имя = `name` с заменой
  первого `_` на `/`.
- Это **переклон существующих членов корпуса**, не отбор новых — ворота
  `experiments/CORPUS_CRITERIA.md` повторно НЕ применять.
- Диск: свободно ~79 ГБ (2026-06-12), ожидаемый объём клонов ~30–40 ГБ —
  влезает, но проверить `df -h` перед стартом и после каждой сотни реп.
- Клонировать в `~/oss/<owner>_<name>` (плоская схема, как
  остальные). НЕ shallow: shallow-границы уже стоили 68 репам off-by-one
  baseline (#111 §1). Если полный клон репы > 2 ГБ — клонировать
  `--filter=blob:none`? НЕТ: partial-клоны тянут блобы по сети при ls-tree
  (наблюдалось в #111 — таймауты GenieTim/pylimer-tools). Полный клон или
  пропуск с пометкой в манифесте.
- Конвейер готов и идемпотентен (`exists` в манифесте пропускается):
  `experiments/ai_repo_run/make_window_baselines.py` →
  `experiments/ai_repo_run/lateral_drift_scan.py`.

## План выполнения

- [ ] Скрипт переклона по манифесту (последовательно или 2–3 потока, retry
      на сетевых ошибках, лог per-repo); репы, исчезнувшие с GitHub
      (404/DMCA), пометить и пропустить.
- [ ] `make_window_baselines.py` — добор baseline для новых клонов.
- [ ] `lateral_drift_scan.py` — полный прогон; ожидание порядка
      98 × (481/183) ≈ 250–300 событий, если плотность сохранится.
- [ ] Eyeball свежей выборки топ-20 из НОВЫХ реп (TP-планка ≥ 70%, как #111).
- [ ] Нормированный agentic-разрез: знаменатель — доля agent-коммитов в репе
      за окно (git log + BOT_HINTS/Co-Authored-By, классификатор уже в
      `lateral_drift_scan.py::classify_commit_author`), repo fixed effects —
      дизайн как у boolean-drift.
- [ ] Дополнить `docs/research/lateral_module_drift_corpus_run.md` (полное
      покрытие + agentic-результат) — не новый док, а апдейт существующего.

## Сделано

- **Переклон (`reclone_missing.py`):** 297 `repo_missing` → полные клоны через
  `gh repo clone` (auth, без rate-limit), 3 потока, retry, cleanup половинчатых.
  Итог: **296 cloned + 1 exists + 1 gone (404: studiocollective/songbird) + 0 failed.**
  Диск: 101 ГБ свободно после (расход ~30 ГБ).
- **Baseline (`make_window_baselines.py`, идемпотентен):** 479 .yml.
  143 новых `ok` + 183 прежних + 151 `ok_empty` + 2 root. `ok_empty` проверен —
  легитимен (окно покрывает C++-историю репы от инцепции; полный клон убрал
  off-by-one #111). Бэкап старого манифеста: `manifest.tsv.bak_before_reclone`.
- **Полный прогон (`lateral_drift_scan.py`):** **495 событий** (CYCLE 153 /
  SDP 58 / NEW 284) на 479 репах, 56 реп с событиями. Подмножество `exists`
  (183 репы) воспроизвело **ровно 98 событий** — детерминизм подтверждён.
  Старый CSV сохранён: `lateral_drift_new.csv.bak_183repos`.
- **Eyeball топ-20 из новых реп (верификация на SHA события, не HEAD):**
  TP 17/20 = **85 %**, CYCLE precision 11/13 = **84.6 %** — планка ≥70 % взята.
  3 FP: 2 CYCLE-мисгрейда (leaf-цель без back-edge) → задача **#117**;
  1 NEW из file-split.
- **Нормированный agentic-разрез (`agentic_normalized.py`, repo fixed effects):**
  сырьё 294 agent / 201 human. Пулинг (на 1k коммитов): agent 9.19 vs human 4.27
  = 2.15×. **Но within-repo (50 mixed-реп): 23 выше agent / 27 выше human,
  sign-test p≈0.67 — НЕЗНАЧИМО.** Пулинг — артефакт композиции: 60 % agent-событий
  даёт одна `makr-code/ThemisDB` (13 024 коммита copilot-swe-agent[bot] из 18 818,
  классификация верна). Вывод: per-commit агентское авторство дрейф НЕ повышает;
  рост — agent-насыщенный хвост распределения реп. Согласуется с velocity-surge.
- **Отчёт дополнен:** `docs/research/lateral_module_drift_corpus_run.md` §8
  (полное покрытие, eyeball, agentic-разрез) + forward-ссылка из §4.

## В работе

- (пусто)

## Следующие шаги

- → **#117** — CYCLE-грейдер: подтверждать back-edge B→A (2 FP из eyeball).
- Грейс-период time-based (30 дней) — крутить, если NEW-шум станет проблемой
  при имплементации DRIFT.4; на advisory-грейде не блокирует.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Полный клон, не shallow и не partial | shallow → off-by-one baseline; partial → сетевые блоб-фетчи при ls-tree (#111) |
| Ворота CORPUS_CRITERIA не применять | Репы уже отобраны в корпус; это восстановление, не расширение |
| Апдейт отчёта #111, не новый док | Один источник истины по lateral-прогону |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/ai_repo_run/reclone_missing.py` | Новый скрипт переклона (296/297) |
| `experiments/ai_repo_run/agentic_normalized.py` | Новый: нормировка + repo fixed effects |
| `experiments/ai_repo_run/baselines/*` | +296 baseline (479 всего) |
| `experiments/ai_repo_run/lateral_drift_new.csv` | Полный прогон (495 событий) |
| `docs/research/lateral_module_drift_corpus_run.md` | §8: полное покрытие + agentic-разрез |

## Как работает

Переклон: `manifest.tsv` (status==repo_missing) → имя `<owner>_<repo>` инвертируется
в slug `<owner>/<repo>` (GitHub-username не содержит `_`, первый `_` — разделитель) →
`gh repo clone` полным клоном. Нормировка: один `git log --all` на репу классифицирует
авторов всех коммитов (те же BOT_HINTS + Co-Authored-By, что в скане), пересекается с
окном из jsonl → знаменатель agent/human коммитов; rate = события/коммиты; repo fixed
effects = within-repo сравнение по 50 mixed-репам (sign-test), что вычищает
between-repo композицию (выброс ThemisDB).

## Дата завершения

2026-06-12

## Примечание о коммите

Артефакты прогона (`*.py`, `*.csv`, `baselines/`) живут в gitignored `experiments/` —
в репозиторий уезжают только обновлённый отчёт `docs/research/...` и файлы задач.
