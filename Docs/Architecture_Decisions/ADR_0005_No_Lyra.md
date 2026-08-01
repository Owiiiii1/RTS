# ADR-0005 — No Lyra Architecture

## Status
Accepted

## Context
Lyra Starter Game — Epic's sample, що демонструє modular gameplay architecture, Experience System, GameFeatures-driven content injection. Lyra — рекомендований Epic'ом референс для AAA-scale production.

Lyra promises:
- Modular gameplay via GameFeatures plugins.
- Experience System for swapping gameplay rule sets.
- Highly decoupled architecture.
- Asynchronous content load для seamless transitions.

Lyra costs:
- Heavy abstraction layers (Experience, ActionSet, AbilitySet, PawnData, GameFeatureAction).
- Many indirection levels — складно debug.
- Plugin-driven content split — overhead для small team.
- Учення кривої — нові contributors витрачають значний час на розуміння framework перед написанням gameplay code.
- Designed для multi-team AAA workflow, не для small indie team.

GrimProtocol context:
- Інді-команда.
- MVP scope: 2-player RTS, ~10-30 units, single map.
- One faction.
- One resource type.
- No live ops requirement.
- No modding requirement.

## Decision
**Lyra architecture, patterns, Experience System, GameFeatures — заборонені.**

Конкретно:
- No `UExperience*` classes.
- No `UAbilitySet` / `UPawnData` indirection layers.
- No `UGameFeatureAction*` injection patterns.
- No `GameFeatures` plugin usage (`+GameFeaturePlugin=...` у DefaultGame.ini — заборонено).
- No `ULyra*` copy-paste.

Замість Lyra:
- Three runtime modules з explicit dependency.
- Gameplay tags як state.
- Data Assets як content.
- GAS direct usage.
- Component-first composition.

Якщо одного дня проєкт виросте до AAA-scale з multiple gameplay modes / live ops / modding — це буде окремий ADR з explicit migration plan. До того — no Lyra.

## Consequences

### Positive
- Plain, readable code для невеликої команди.
- Faster onboarding new contributors.
- Меньше indirection — простіше debug.
- Architecture мapps directly до RTS gameplay potreб.
- Production cost — низький.

### Negative
- No "free" modding / GameFeatures injection (acceptable: not у MVP scope).
- Якщо проєкт раптом виросте — refactor required (acceptable: ADR can be superseded).
- Engineers, що приходять з Lyra experience, мають "розучитися" Lyra patterns.

### Risks
- Hidden creep — engineers додають "just a bit of GameFeatures" → architecture decay. Mitigated: hard ban у `/CONTRIBUTING.md`, PR-blocking.

## Alternatives Considered
- **Adopt full Lyra** — overkill для indie scope.
- **Partial Lyra (Experience System only)** — partial adoption — worst of both worlds: complexity без full benefit.
- **Custom modular framework** — re-invent Lyra subset, same cost.

## References
- `/CONTRIBUTING.md` → Hard Bans.
- `Docs/TDD/01_Module_Architecture.md` — actual module split.
- [ADR-0006](ADR_0006_Indie_Scope_No_Overengineering.md) — повна scope discipline.
