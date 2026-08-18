# Architecture Decision Records

Зафіксовані архітектурні рішення проєкту. Формат — lightweight ADR.

## Index

- [ADR_0001_Project_Prefix_GP](ADR_0001_Project_Prefix_GP.md) — GP як єдиний project-wide префікс.
- [ADR_0002_Data_Driven_First](ADR_0002_Data_Driven_First.md) — Data First, Logic Second як головна gameplay-конфіг філософія.
- [ADR_0003_GAS_First](ADR_0003_GAS_First.md) — GAS як головне джерело gameplay state і synchronization.
- [ADR_0004_Multiplayer_First](ADR_0004_Multiplayer_First.md) — server-authoritative multiplayer-first з першого дня.
- [ADR_0005_No_Lyra](ADR_0005_No_Lyra.md) — відмова від Lyra patterns і Experience System.
- [ADR_0006_Indie_Scope_No_Overengineering](ADR_0006_Indie_Scope_No_Overengineering.md) — Simple First, no enterprise abstraction.
- [ADR_0007_Building_As_Pawn](ADR_0007_Building_As_Pawn.md) — `Draft`. Будівлі і юніти наслідуються від спільного `AGP_UnitBase : APawn`.
- [ADR_0008_AI_Opponent_AAIController](ADR_0008_AI_Opponent_AAIController.md) — AI opponent implemented as `AAIController` subclass, not `APlayerController`.
- [ADR_0009_Orbital_Delivery_Pillar](ADR_0009_Orbital_Delivery_Pillar.md) — Units, READY buildings, and Wall Packages arrive via orbital drop pods. `AGP_Wall` segments are placed from MainBase inventory. No local production / construction.

## Format

```markdown
# ADR-NNNN — Title

## Status
Accepted | Superseded by ADR-NNNN | Deprecated

## Context
Що відбувається. Які обмеження. Чому це питання взагалі стало.

## Decision
Конкретне рішення.

## Consequences
Позитивні і негативні наслідки. Trade-offs. Edge cases.

## Alternatives Considered
Які альтернативи розглядалися і чому відкинуті.
```

## Rules

- ADR — immutable після Accepted. Зміна — новий ADR, що superseded попередній.
- Кожне нове архітектурне рішення (новий module, новий subsystem, новий gameplay pattern) — починається з ADR draft.
- ADR пишеться **до** імплементації, не після.
