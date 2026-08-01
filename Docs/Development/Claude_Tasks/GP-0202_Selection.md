# GP-0202 Selection

## Goal

Описати selection rules для units/buildings у MVP.

## Inputs

- [`../../GDD/09_UI_UX.md`](../../GDD/09_UI_UX.md)
- [`../../TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md)
- [`../../TDD/09_Gameplay_Tags.md`](../../TDD/09_Gameplay_Tags.md)

## Code Allowed

No.

## Scope

Selection state, selected entity UI, valid/invalid command exposure. Не реалізовувати selection code.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Гравець вибирає entity і розуміє, які команди доступні.

## Deliverables

- Local selection ownership.
- Deselect and invalidation rules.
- Friendly/enemy selection behavior.
- UI requirements.
- Tag/Data Asset needs for selectable capability.

## Validation

- Selection is client-local.
- Selection does not mutate gameplay state.
- Destroyed selected actor is handled.

## Stop Condition

Зупинитися після selection spec.

## Output

- Design spec: section **"Detailed Selection Rules (GP-0202)"** у [`../../TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md).
- Decisions: cap=24 (hard), enemy/neutral = inspect-only (separate `InspectedTarget` state), full control groups Ctrl+1..9, highlight = MID emissive boost.
- New tags reserved (registered у `GPGASRuntime` при code task): `GP.Capability.Selectable`, `GP.Capability.Inspectable`, `GP.Selection.Type.Unit`, `GP.Selection.Type.Building`.
- DataAsset addition: `CapabilityTags : FGameplayTagContainer` on UnitDefinition / BuildingDefinition.
- Code implementation deferred to follow-up task **GP-0202A Selection Implementation** (Code Allowed: Yes).

## Pivot Note (2026-05-16 — Fog of War у MVP)

**Selection scope обмежено FoW** per [`../../TDD/15_Fog_of_War.md`](../../TDD/15_Fog_of_War.md):

- LMB / Marquee — тільки visible actors hit (engine relevance culls hidden enemies).
- Inspect — тільки visible enemies inspectable.
- Control groups: hidden member's last-known position used (replaced by "?" status у UI); on re-sight, resyncs.
- Selection rules vs cap / friendly / mixed — unchanged.
