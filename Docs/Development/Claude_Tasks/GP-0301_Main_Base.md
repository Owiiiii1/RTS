# GP-0301 Main Base

## Goal

Описати main base як MVP anchor і win/lose object.

## Inputs

- [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md)
- [`../../GDD/08_Win_Lose_Conditions.md`](../../GDD/08_Win_Lose_Conditions.md)
- [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md)
- [`../../TDD/10_Data_Assets.md`](../../TDD/10_Data_Assets.md)

## Code Allowed

No.

## Scope

Base identity, owner/team, health, damage, destruction, match-flow relationship.

## Required Skill Pass

- `gp-mechanics-validator`

## Deliverables

- Main base gameplay role.
- `DA_GP_Building_MainBase` fields.
- Tags.
- Attribute mapping.
- Destruction and match result rule.

## Validation

- Base receives damage.
- Base destruction affects match end.
- Health is server-authoritative.

## Stop Condition

Зупинитися після base spec.

## Output

- Design spec: section **"Detailed Main Base Rules (GP-0301)"** у [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md).
- Decisions:
  - Production: FIFO queue 5 slots, cancel any slot з refund.
  - Rally point: configurable via RMB (ground / Ferronite deposit auto-mine / building).
  - Repair: Worker `GP.Ability.Repair` (channel, costs Ferronite per tick).
  - Destruction: economic loss, NOT auto-lose. Match continues; score race до 10-min timer per GDD/08.
- DataAsset schema enumerated: `MaxProductionQueue=5`, `bSupportsRallyPoint=true`, `DefaultRallyOffset`, `bSelfRegen=false`, `RepairAbility=UGP_GA_Repair`.
- Tags added: `GP.Command.SetRallyPoint`, `GP.Command.QueueProduction`, `GP.Command.CancelProduction`, `GP.Command.Repair`, `GP.Ability.Repair`.
- Code implementation deferred to follow-up task **GP-0301A Main Base Implementation** (Code Allowed: Yes).

## Pivot Note (2026-05-16 — Orbital Delivery Model)

**Major scope change** per [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §"Building Lifecycle — Orbital Drop" і memory rule `project_orbital_delivery_model`:

- **Production / Rally Point sections REMOVED.** MainBase no longer produces units / hosts production queue / has rally point. All units arrive via orbital drop (per [`../../TDD/14_Orbital_Delivery`](../../TDD/14_Orbital_Delivery.md)).
- **Retained:** drop-off zone, Health/Damage flow, repair receiver, sight source contribution.
- **Added:** Container System (per [`../../TDD/07_Resource_Architecture`](../../TDD/07_Resource_Architecture.md) §Container System Update) — `UGP_StorageComponent` extended з `TArray<FGP_Container>` and auto-ship-to-orbit pipeline.
- **MainBase destruction = annihilation** (changed from "consequence only"). Without MainBase no containers can ship → no path to victory → opponent wins. Per [`../../GDD/08_Win_Lose_Conditions`](../../GDD/08_Win_Lose_Conditions.md) revision pending.
- Implementation slice updated accordingly у TDD/13 (production-related GP-S## slices removed; Container-related slices added).
