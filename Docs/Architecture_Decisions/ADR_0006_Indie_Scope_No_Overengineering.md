# ADR-0006 — Indie Scope, No Overengineering

## Status
Accepted

## Context
GrimProtocol — інді RTS для малої команди. Часто engineering teams будують architecture "на майбутнє" — generic frameworks, deep abstractions, manager-of-managers, subsystem hell. Ці інвестиції виправдані для AAA-scale або high-uncertainty domains, але вбивають індікі-команду:

- Production time витрачається на framework code, не gameplay.
- Onboarding час для нового контриб'ютера росте exponentially.
- Refactor cost — фіктивний, бо future requirement, що мав би використати abstraction, ніколи не настає.
- Reading code стає labyrinth.

Тренд в UE community: subsystem-everywhere, GameFeatures-driven content, manager classes для every concern, deeply nested inheritance.

## Decision
**Simple First.**

Порядок пріоритетів при будь-якій новій feature:

1. Working implementation.
2. Gameplay loop integration.
3. Multiplayer synchronization.
4. Readable architecture.

Тільки потім — abstraction, optimization, advanced systems.

### Concrete Forbidden Patterns

1. **Massive subsystem splitting** — new `UGameInstanceSubsystem` / `UWorldSubsystem` створюється тільки коли він явно власник lifecycle ширше за один actor/component, і має ADR-обґрунтування. MVP exception: `UGP_SessionSubsystem` (session lifecycle == GameInstance lifecycle, явно scoped).
2. **Manager-of-managers** — `UUnitManager`, `UCombatManager`, `UEconomyManager`, що тримають `TArray<AActor*>` і дублюють actor responsibilities — заборонені.
3. **ECS-like abstraction** (без UE Mass Entity plugin) — заборонено. RTS scale у MVP не вимагає.
4. **Generic gameplay framework** — abstract "rule engine", "action engine", "behavior pipeline" — заборонено без real use case.
5. **Future-proof architecture** — "якщо одного дня знадобиться X..." — discount це до zero. Якщо знадобиться — refactor дешевше за upfront cost.
6. **Deep inheritance** — більше за 3 levels (Actor → UnitBase → MobileUnit → Worker — це 3, max). Композиція через компоненти — preferred.
7. **Plugin-fragmentation** — proj-owned content не sharded в plugins без real need.

### Concrete Allowed Patterns

- Three runtime modules (GPRuntime, GPGASRuntime, GPUIRuntime) — explicit responsibility split.
- Components per behavior — thin actor + N components.
- Data Assets — все tunable.
- GAS — gameplay state.
- Subsystems — тільки коли lifecycle reasoning виправдовує (Session).
- Interfaces — коли cross-module communication потребує type-erased contract.

### Feature Validation Checklist

Перед додаванням нової механіки відповісти на 14 питань у `/CONTRIBUTING.md` → Feature Validation Checklist. Якщо не проходить — `Backlog` або `Out_Of_Scope`.

### File Size

Soft cap — ~1000 рядків на файл. Якщо файл росте — split responsibilities у component / Data Asset / helper namespace.

## Consequences

### Positive
- Швидкий time-to-playable.
- Низька onboarding cost.
- Predictable refactoring.
- Code readability — junior contributor може зрозуміти core flow за день, не місяць.
- Engineering focus — gameplay, не framework.

### Negative
- Якщо проєкт reaches AAA-scale — потребує refactoring (acceptable risk; refactor cost lower than upfront abstraction cost для unrealized AAA-future).
- Engineers з AAA background можуть "хотіти" будувати abstractions — discipline drift risk.
- Деякі patterns, що були б "elegant" — заборонені (acceptable).

### Risks
- Engineer додає "невелику" abstraction → over time → manager hell. Mitigated: hard bans у `/CONTRIBUTING.md`, PR-blocking review.
- Junior engineer думає "це не enterprise, отже не professional" — psychological resistance. Mitigated: clear pillar communication.

## Alternatives Considered
- **Enterprise from day one** — overengineering, production cost.
- **Mid-engineering** — fuzzy boundary, drift toward enterprise inevitable.
- **Total ad-hoc** — недостатньо disciplined для multiplayer-first goals.

Decision: **Simple First**, з explicit hard bans і feature validation.

## References
- `/CONTRIBUTING.md` → Core Philosophy: Simple First.
- `/CONTRIBUTING.md` → Hard Bans.
- `/CONTRIBUTING.md` → Feature Validation Checklist.
- [ADR-0005](ADR_0005_No_Lyra.md) — concrete instance of this philosophy.
