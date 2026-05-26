# Code Quality Rules

Источник: адаптировано из gm (`docs/ai/code_quality.md`). Что делать, когда видишь проблемы. SOLID/DRY/YAGNI ты знаешь — здесь о том, **когда и как** их применять.

## Fix Immediately (no asking)

| Issue | Action |
|-------|--------|
| Unused include/import | Delete |
| Commented-out code | Delete (git has history) |
| Unused variable/method | Delete |
| Typo in comment | Fix |
| Formatting in lines you're changing | Fix |

## Report and Suggest (ask first)

| Issue | Threshold | Why ask |
|-------|-----------|---------|
| Long function | >30 lines | May need design decision |
| Large class | >300 lines | May need module split |
| Magic number | Any | Need to agree on name |
| Copy-paste block | >5 lines repeated | Need to decide where to extract |
| SOLID violation | Any | May be intentional tradeoff |
| Missing error handling | — | Need to agree on strategy |

**Format:** "I noticed [issue] in [location]. Want me to fix it in a separate commit?"

## Thresholds

```
Function: max 30 lines (excluding braces)
Class: max 300 lines
Parameters: max 4 (else → struct)
Nesting: max 3 levels
```

Commit: `refactor(module): description` + "No functional changes"

**Never refactor:**
- In the middle of a feature
- Without passing tests
- Code you don't understand

## Anti-AI-Slop Rules

### Before Writing Code

1. **Read the file ENTIRELY first** — don't add code until you understand the existing structure
2. **Ask: can I solve this by DELETING code?** — often the answer is yes
3. **Ask: does this already exist?** — search project before creating new utilities

### Hard Limits Per Commit

| Metric | Limit |
|--------|-------|
| New lines of code | ≤50 (excluding tests/fixtures) |
| New files | ≤2 |
| New classes | ≤1 |
| New abstractions | 0 unless explicitly requested |

If task requires more — split into smaller commits with review between.

### Forbidden Patterns

```cpp
// ❌ Obvious comments
counter++;  // increment counter
if (x > 0)  // check if x is positive

// ❌ Defensive code without reason
if (ptr != nullptr) {  // "just in case"
    if (ptr->isValid()) {  // "for safety"
        if (ptr->canProcess()) {  // "to be sure"

// ❌ Premature abstraction
class IProcessor { };           // only one implementation exists
class ProcessorFactory { };     // creates one type

// ❌ Wrapper that adds nothing
Result doThing() { return actuallyDoThing(); }
```

### Class Design Rules

**NVI (Non-Virtual Interface) — публичные методы не вызывают другие публичные:**

```cpp
// ❌ Плохо — public вызывает public
class Player {
public:
    void Start() {
        Initialize();   // опасно при наследовании
        Run();
    }
    virtual void Initialize() { }
    virtual void Run() { }
};

// ✅ Хорошо — public вызывает private
class Player {
public:
    void Start() {
        DoInitialize();
        DoRun();
    }
private:
    virtual void DoInitialize() { }
    virtual void DoRun() { }
};
```

**Зачем:**
- Безопасность при наследовании — переопределённый метод не сломает логику базового класса.
- Ясный контракт: public — точки входа, private — детали реализации.
- Внешний код не может вызвать метод в неправильный момент.

**Связано с:** Liskov Substitution Principle, Template Method Pattern.

### When Modifying Existing Code

1. **Match existing style exactly** — don't "improve" formatting.
2. **Don't add error handling unless there's a real bug.**
3. **Don't add logging unless debugging a specific issue.**
4. **If you're making the file longer — justify why.**

### Self-Check Before Commit

- [ ] Can I remove any code I just added?
- [ ] Are there comments that explain "what" instead of "why"?
- [ ] Did I add any "just in case" checks?
- [ ] Is every new line necessary for the task?
- [ ] Would a junior dev understand this without the comments?

## cpparch-специфика

- **Каждое правило = один файл = один класс**, реализующий `IRule`. Добавление правила не должно править существующие файлы (OCP).
- **Каждое правило обязано иметь fixtures** — `fixtures/<rule>/pass/` и `fixtures/<rule>/fail_*/`. Без fixtures правило не существует.
- **Каждое дефолтное правило — с атрибуцией** (Core Guidelines / Lakos / Martin). Если ссылку не приложил — это не дефолт, это опция.
- **Cpparch проверяет сам себя** в CI. Любой merge ломающий собственные SF.7/8/9/21/cycles на cpparch — недопустим.
