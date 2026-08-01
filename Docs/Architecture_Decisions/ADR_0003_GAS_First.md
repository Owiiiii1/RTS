# ADR-0003 — GAS First

## Status
Accepted

## Context
RTS gameplay має чималу множину synchronized state: player resources, unit health/armor/damage, attack cooldowns, build speed modifiers, mining speed modifiers, gameplay buffs/debuffs, ability cost validation, multi-stage abilities (build → construction → complete), effects з durations і stacks.

Можливі підходи:

1. **Ad-hoc replicated UPROPERTY + custom RPC patterns** — швидко для одного class, але:
   - Дубльована логіка cost validation у кожному client/server path.
   - Manual cooldown timers у кожному class.
   - Manual modifier stacking (build speed buffs з різних джерел).
   - Складно debugging — стан розкиданий.

2. **Custom gameplay framework** — write our own attribute / effect system.
   - Velikій upfront cost.
   - Re-inventing well-trodden ground.
   - Production risk.

3. **Gameplay Ability System (GAS)** — UE engine plugin, battle-tested, supports:
   - Replicated Attributes з automatic clamping і modifier stacking.
   - Replicated GameplayEffects (instant, duration, infinite, periodic).
   - Predicted abilities з server reconciliation.
   - GameplayTags як state primitives.
   - Costs, cooldowns, requirements — built-in.

## Decision
**GAS — головне джерело gameplay state і gameplay synchronization.**

Усе, що змінюється під час матчу і потребує network sync, проходить через GAS, якщо немає документованої причини інакше.

### What GAS Owns (Mandatory)

- Player-level attributes (Resource, MaxUnits, CurrentUnits, modifiers) — `UGP_PlayerAttributeSet` on `AGP_PlayerState`.
- Unit-level attributes (Health, Armor, Damage, AttackCooldown, ...) — `UGP_UnitAttributeSet` on `AGP_UnitBase`.
- Resource transactions — `UGameplayEffect` (cost, income).
- Cooldowns — `UGameplayEffect` з duration і tag application.
- Gameplay buffs / debuffs — `UGameplayEffect`.
- Gameplay actions (build, produce) — `UGameplayAbility`.
- Runtime state (Moving, Attacking, Mining, Dead) — Gameplay Tags на ASC.

### What GAS Does Not Own

- Selection (local PlayerController concern).
- Camera (local concern).
- UI state.
- Movement physics (UE built-in MovementComponent stays authoritative for transform).
- Cosmetic VFX / SFX (Multicast RPC).

### ASC Placement

- Player ASC → `AGP_PlayerState` (Mixed replication mode).
- Unit ASC → `AGP_UnitBase` (Minimal replication mode).

## Consequences

### Positive
- Чіткий, відомий pattern для cooldowns, costs, modifiers.
- Replicated state — engine handles.
- Tag-based gameplay state — easy to query, easy to combine.
- Centralized debugging (`showdebug abilitysystem`).
- Future extension cheap (new attribute, new effect — Data Asset + minor code).

### Negative
- GAS learning curve для нових contributors.
- ASC overhead — small per-unit cost; може стати issue з 100+ units (mitigated: RTS-scale у MVP — 10-30 units per player).
- Effect application latency — server-only execution додає 1 RTT для cost feedback (mitigated through UI predictive markers).
- Debugging replicated state потребує `gameplaytags` console commands.

### Risks
- Якщо engineers зловживають GA для non-ability logic — code rot. Mitigated: GAS-discipline у Coding_Rules.md.
- Custom calculations через `UGameplayEffectExecutionCalculation` — складніше debug. Mitigated: keep simple (damage = base * resistance мінімально).

## Alternatives Considered
- **Custom attribute system** — TBD upfront cost, GAS вже existing, well-supported.
- **Hybrid (GAS only для some attributes)** — fragmenting state ownership, code rot.
- **No GAS, plain replicated UPROPERTY** — manual stacking, manual cooldowns, manual prediction. Болюче довгостроково.

## References
- `/CONTRIBUTING.md` → GAS Discipline section.
- `Docs/TDD/02_GAS_Architecture.md` — implementation details.
- UE GAS documentation (engine docs).
