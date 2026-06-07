# Boolean-State Drift: Research Sources & Authority Anchors

**Дата:** 2026-06-07  
**Статус:** draft (собранные источники, требует deep-research для полноты)  
**Задача:** #089

## Гипотеза

Класс, накапливающий boolean-поля состояния (`started`, `running`, `paused`, `failed`, ...), кодирует неявную state machine. N флагов → 2^N теоретических состояний; рост разницы между легальными и представимыми состояниями может служить drift-сигналом.

## Authority Anchors

### C++ Core Guidelines

- **ES.21: "Do not use a variable for two unrelated purposes"** — boolean-флаги, накапливающиеся в одном классе, часто нарушают этот принцип, смешивая разные аспекты состояния.
- **C.130: "Avoid virtual functions for type-dependent operations"** — связано с State Pattern как альтернативой множеству bool-флагов.
- **F.26: "Use T* or owner<T> to designate a single object"** — именование и семантика флагов.

### Lakos (Large-Scale C++ Software Design)

- **Levelization violations & CCD growth** — рост boolean-полей часто коррелирует с нарушением физического дизайна и увеличением CCD.
- **God-headers & God-objects** — классы, накопившие множество bool-полей, часто эволюцируют в "god objects", нарушающие SRP.

### Gang of Four & Martin

- **State Pattern** (GoF) — явная замена множеству boolean-флагов.
- **Make Illegal States Unrepresentable** (Martin, "Your Code as a Crime Scene") — ключевой принцип для detection: если система позволяет rappresentare нелегальные состояния (failed && completed), это дизайн-дефект.

### Недавние research & blogs

- **"Boolean Soup"** — общеупотребительный термин в community для классов с множеством bool-полей без структуры.
- **"State Explosion"** — проблема сочетаний boolean-флагов: N флагов = 2^N состояний, но обработка часто не покрывает все.
- **"Boolean Blindness"** — неспособность типизировать boolean-аргументы (отдельный паттерн, но связан с state-флагами).
- **"Flag Arguments"** (Uncle Bob, Clean Code) — считаются антипаттерном, особенно в таких комбинациях.

## Существующие инструменты & Smell Detectors

### Static Analyzers

- **Cppcheck** — может детектировать некоторые boolean-условия, но не state explosion как таковой.
- **clang-tidy** — есть checks для "too many arguments", но не для boolean-state накопления.
- **SonarQube** — имеет правила на boolean conditions & complexity, но не специфичные для state-флагов.

### Academic work

- Ищем: smell detectors, AST-based state-machine inference, boolean-complexity metrics.
- Области: program verification, model checking (теория), но мало applied к real C++ codebases.

## Примеры (из experience)

### Хрестоматийный bad case

```cpp
class NetworkConnection {
  bool connected;      // ← связано с состоянием
  bool authenticated;  // ← связано с состоянием
  bool encrypted;      // ← связано с состоянием
  bool reconnecting;   // ← связано с состоянием
  
  // 16 теоретических состояний, легальных ~3-4
  // Невозможные комбинации:
  // - connected=false && authenticated=true
  // - encrypted=true && !connected
  // - reconnecting=true && connected=true
};
```

### Хороший case (State Pattern)

```cpp
class NetworkConnection {
  std::unique_ptr<ConnectionState> state;  // ← явная state machine
};
class ConnectionState {
  virtual ~ConnectionState() = default;
  virtual void onConnect(NetworkConnection& ctx);
  virtual void onAuthenticate(NetworkConnection& ctx);
};
// Легальные состояния явно перечислены, невозможные исключены
```

## Следующие фазы (stub'ы)

### Corpus Study
- [ ] Сканировать OSS-корпус на классы с 3+ bool-полями.
- [ ] Ранжировать по числу флагов.
- [ ] Извлечение: libclang AST vs grep/regex (документировать FP/FN).

### State-Likeness Heuristics
- [ ] Именование: state-флаги (`started/running/paused/failed/ready/connected`) vs config (`enable_*/use_*`).
- [ ] Условные выражения: как часто несколько bool-флагов комбинируются?

### Real-world Examples (≥20)
- [ ] Собрать примеры из корпуса с анализом "невозможных состояний".

### Drift Simulation
- [ ] Для каждого примера: N → 2^N, estimate легальных состояний, зазор.

### Tooling Gap
- [ ] Существующие: SonarQube, Cppcheck, clang-tidy, CppDepend, Designite, PMD.
- [ ] Gap: нет специфичных инструментов для state-machine detection в C++.

### Feasibility
- [ ] Candidate-правила: `boolean_state_growth`, `implicit_state_machine`, `state_flag_accumulation`.
- [ ] CI-suitability: semantic-backend (#042) или grep-эвристика?
- [ ] Вердикт: YES / NO / MAYBE.

---

**Статус:** draft, требует deep-research на web-sources для полноты (статьи, доклады, academic papers).
