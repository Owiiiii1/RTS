# GP-0102 Define Core Gameplay Loop

## Goal

Описати MVP loop (orbital delivery model): select worker -> mine raw Ferronite -> deliver to MainBase containers -> ship container to orbit (OrbitalFerronite + FerroniteScore) -> order orbital drops (units / Logistics Hub / turrets) -> pressure enemy base -> match ends by delivery quota or timer.

## Inputs

- [`../../GDD/02_Core_Gameplay_Loop.md`](../../GDD/02_Core_Gameplay_Loop.md)
- [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md)
- [`../../GDD/06_Resources.md`](../../GDD/06_Resources.md)
- [`../../TDD/07_Resource_Architecture.md`](../../TDD/07_Resource_Architecture.md)
- [`../../TDD/10_Data_Assets.md`](../../TDD/10_Data_Assets.md)

## Code Allowed

No.

## Scope

Документація loop rules, Data Asset needs, tags, attributes, UI requirements.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Deliverables

- Core loop steps.
- Required player decisions.
- Resource flow rule: two-state Ferronite (Planetary raw in containers -> Orbital spendable via shipping); spend OrbitalFerronite on orbital drops.
- Data requirements: `DA_GP_Resource_Ferronite`, `DA_GP_Building_LogisticsHub` (orbital-dr