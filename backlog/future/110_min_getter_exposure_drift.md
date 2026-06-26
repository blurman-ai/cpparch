# [SCAN] Getter exposure drift — a new getter as an encapsulation-drift signal

**Creation date:** 2026-06-12
**Start date:** —
**Status:** new
**Module:** SCAN][RULES
**Priority:** minor
**Difficulty:** unknown
**Target release:** v0.3+
**Blocks:** —
**Blocked by:** —
**Related:** #090 (boolean_state_drift_metric, future), #095 (config_bag_growth), #042 (clang_semantic_backend — precise accessor detection)

## Goal

Research and formalize "adding a new public getter to an existing class"
as a per-commit signal of encapsulation drift (encapsulation erosion), modeled on boolean-drift.

## Context

User's idea (2026-06-12, corpus-drift session): a new getter is also drift, but
"outward" — the mirror of boolean-drift:

- **a bool field** = state flows **inward** into a struct (config-bag, flags);
- **a getter** = state flows **outward** from a class: an internal field becomes observable,
  clients begin to depend on the representation, the invariant "only the class itself touches the field"
  dies. Each getter is a weakening of the contract (constraint decay in the terms of Dente et al.).

The mechanics of degradation: getter → client code builds logic on someone else's state (Feature
Envy) → chains `a.getB().getC().x()` (a Law of Demeter violation) → the class gradually
turns into a struct with ceremonies, behavior gets smeared across clients.

The metric family is the same as #093–#100: a cheap textual per-commit scan without semantics.

## Lit review (done 2026-06-12, web recon)

**Main finding: C.131 contains a ready Enforcement section** — *"Flag multiple `get` and
`set` member functions that simply access a member without additional semantics"*. That is, the
rule is presented not as our own invention, but as a **temporal implementation of the official
enforcement from Core Guidelines**. The triviality criterion from C.131: flag only getters
indistinguishable from a public field; exceptions — maintaining the class invariant and conversion
internal→interface type.

Attribution (all VERIFIED against primary sources, except where marked):

| Source | What it gives the rule |
|---|---|
| [Core Guidelines C.131](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#rh-get) | Enforcement section + triviality criterion + exceptions. Nuance: issue isocpp#776 — pushback from practitioners → an argument for advisory |
| [Core Guidelines C.9](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#rc-private) | "Minimize exposure of members... simplifies maintenance" — the headline attribution |
| Parnas, CACM 1972 | Accessing/modifying procedures — part of a module, "not shared by many modules as is conventionally done"; exposed information narrows the space of future changes — literally constraint decay |
| Riel 1996, Heuristic 3.3 | "Beware of classes that have many accessor methods... related data and behavior are not being kept in one place" — the static form of our metric |
| Fowler, [GetterEradicator](https://martinfowler.com/bliki/GetterEradicator.html) / [TellDontAsk](https://martinfowler.com/bliki/TellDontAsk.html) | "Encapsulation = hiding design decisions" + Data Class smell. **Boundary**: Fowler himself is against a war on all query methods → flag only trivial ones and only the delta |
| [Hyrum's Law](https://www.hyrumslaw.com/) | The mechanism of irreversibility: a getter once exposed becomes a contract — it can't be removed |
| Law of Demeter + Guo et al., WCRE 2011 (FOUND) | LoD violations ↔ bug-proneness (Eclipse, logistic regression); JPL Pathfinder: integrating LoD-violating parts is an order of magnitude more expensive |
| Khomh et al., EMSE 2012 (FOUND) | The ClassDataShouldBePrivate antipattern → statistically significant change/fault-proneness (54 releases of ArgoUML/Eclipse/Mylyn/Rhino) |
| Zoller/Schmolitzky 2012, Vidal et al. 2016 (FOUND) | Over-exposure is mass: ~20% of methods / 25% of classes more public than necessary (>3.6 MLOC Java) |
| Romano/Pinzger, ICSM 2011 (FOUND) | Fat interfaces: low usage cohesion of an interface predicts churn — the closest support for "growth of the public surface → instability" |
| Izurieta/Bieman, ICST 2008 / SQJ 2012 (FOUND) | Design grime: the decay of a design = accumulation of small couplings over time — the evolutionary frame of the whole drift category |
| [arXiv 2510.03029](https://arxiv.org/html/2510.03029v1) | **AI argument**: encapsulation smells in LLM code +118–165% against the reference (4 LLMs, 1000 tasks, Designite). A counterexample for honesty: Molison ESEM 2025 — on Python/SonarQube LLM code is no worse |

**Gap in the literature = our novelty**: work on *adding* accessors over time
(the per-commit delta as a predictor) wasn't found — neither in MSR nor in API evolution. The static
base exists, the temporal version exists nowhere.

### Answers to the questions for the literature

- [x] Empirics "accessors ↔ defects": yes — Guo 2011 (LoD→bugs), Khomh 2012 (data exposure→faults), over-exposure 20–25%
- [x] A citable justification for a default rule: C.131 Enforcement (ready-made!) + C.9 + Parnas + Riel 3.3
- [x] Evolutionary work: directly NONE (gap=novelty); the closest — Romano/Pinzger (fat interfaces→churn), grime
- [x] AI angle: encapsulation smells +118–165% (arXiv 2510.03029) — hang the argument on this, not on "AI writes badly in general"

## Execution plan

- [x] Lit review — done, see above
- [ ] Regex prototype of a per-commit scan (analogous to `bool_history_scan.py`):
      added lines in existing headers of the form
      `Type getX() const { return x_; }` / `const T& x() const noexcept;` /
      declaration `getX() const;` + naming convention.
      **Scope strictly per C.131: only trivial ones** (body = `return field;`)
- [ ] FP classes: computed getters (DO NOT flag — a C.131 exception), invariant maintenance,
      type conversion, interface implementations, generated code, override
- [ ] Corpus run: getter-add rate agentic vs human within mixed repos
      (design like boolean-drift, repo fixed effects); check the prediction of
      arXiv 2510.03029 (+118–165% encapsulation smells) on the C++ corpus
- [ ] The share of "bare" getters (`return field;`) vs computed among the new ones
- [ ] If there is a signal → metric design doc: advisory "class X gained N trivial getters
      over M commits" (a pattern like config-bag growth #095); don't propose a gate

## Done

- 2026-06-12: lit review (12 sources, 7 VERIFIED) — there is a basis for the rule,
  including a ready-made Enforcement formulation from C.131

## In progress

- (empty)

## Next steps

1. Scan prototype — after closing the active wave #093–#103.

## Key decisions

| Decision | Reason |
|---------|---------|
| future folder, v0.3+ | User explicit: "history for the future" |
| Textual scan, not clang | A family of cheap drift metrics; clang precision — separately in #042 |
| Advisory, not gate | Fowler against GetterEradicator absolutism; issue isocpp#776; a probabilistic signal |
| Only trivial getters (body `return field;`) | The C.131 criterion; computed/invariant ones — legitimate |
| Only the delta (new ones), not a static count | A static count = Riel 3.3, already someone else's; the temporal version — our novelty |

## Changed files

| File | Change |
|------|-----------|
| — | — |
