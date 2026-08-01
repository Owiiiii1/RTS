# GP-0304 Barracks

> # SUPERSEDED — DO NOT IMPLEMENT
> This file describes a pre-orbital-delivery (local production/construction) design and must not be used as an implementation source. Current canonical direction: ADR_0009 (Orbital Delivery) + current TDD/GDD (e.g. TDD/13, TDD/14, GDD/08, GDD/10). Filename retained for history/cursor stability.

Superseded by the Logistics Hub orbital-drop building (see TDD/06_Building_Architecture, GDD/05_Buildings, ADR_0009).

## Goal

Описати barracks як першу economy escalation building.

## Inputs

- [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md)
- [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md)
- [`../../TDD/10_Data_Assets.md`](../../TDD/10_Data_Assets.md)

## Code Allowed

No.

## Scope

Barracks cost, build validity, unit cap increase, damage, Data Asset fields.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Deliverables

- `DA_GP_Building_Barracks` fields.
- Cost: 1 primary resource.
- Max unit count contribution.
- Build valid/invalid feedback.
- Damage/destruction notes.

## Validation

- Cost/cap are not hardcoded.
- Server validates build request and resource spend.
- UI shows cap increase.

## Stop Condition

Зупинитися після barracks spec.

## Output

- Design spec: section **"Detailed Assembly Yard Rules (GP-0304)"** у [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md).
- Building name: **Assembly Yard** (renamed from "Barracks" per Pillar 2; task file rename pending GP-0802).
- Decisions:
  - Multi-worker construction speedup: linear N× to `MaxBuilders` cap (DA-driven).
  - Cancel: 100% refund будь-який час; abuse vector (fake-place scout) flagged для playtest mitigation.
  - Cap clamp on destroy: existing units survive, new production blocked until `CurrentUnits <= MaxUnits` (natural attrition).
- DataAsset schema enumerated; all numerics — DA placeholders + balance-pass TBD.
- Production reuses GP-0301 FIFO queue model. Build validity = 7-step server validation chain.
- `GE_GP_UnitCap_Plus5` removal via `RemoveActiveGameplayEffectBySourceEffect` on destroy.
- Code implementation deferred to follow-up task **GP-0304A Assembly Yard Implementation** (Code Allowed: Yes).

## Pivot Note (2026-05-16 — Orbital Delivery Model)

**Major scope change** per [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §"Post-Pivot Override" і memory rules `project_orbital_delivery_model` + `project_container_system`:

- **Building renamed: AssemblyYard → Logistics Hub.** Task file rename GP-0802 expanded.
- **Local build path REMOVED.** No more Worker channel construction, no `UGP_ConstructionComponent`, no `AGP_ConstructionSite`, no `AGP_GhostBuilding`.
- **Source: orbital drop only** via [`../../TDD/14_Orbital_Delivery`](../../TDD/14_Orbital_Delivery.md).
- **Function repurposed:**
  - +5 MaxUnits (`GE_GP_UnitCap_Plus5` — retained).
  - **NEW:** +N MaxContainerCount contribution to owning player's MainBase Storage (expands shipping pipeline).
- **Removed sections from previous spec:**
  - Multi-worker construction speedup.
  - 100% refund cancel policy (no cancel — orbital drops execute immediately on landing).
  - `MaxBuilders`, `CancelRefundRate` DataAsset fields.
  - Production queue capability (Logistics Hub doesn't produce — it's passive cap + storage).
- **Retained:** unit cap clamp logic (existing units survive if Logistics Hub destroyed; production blocked until below cap).
- Implementation slice migrated to "Orbital Delivery" group у TDD/13.
