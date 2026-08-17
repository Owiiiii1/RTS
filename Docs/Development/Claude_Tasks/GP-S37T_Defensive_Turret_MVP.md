# GP-S37T — Defensive Turret MVP

## Status
**GP-S37T_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Goal
Add the first functional combat building after Logistics Hub: Purchase → READY → Deploy → DropPod → operational `AGP_DefensiveTurret` that auto-acquires and damages enemies through the existing combat pipeline.

## Inputs
- Verified `origin/main` @ `9ace159714b5eca0f79e4985fc2496d34cbb7cc3` (includes GP-S36G)
- Implementation: `7aa0565f9e10e9fe447fe9898835eba28ddc9b81`
- GP-S35B native catalog row `DA_GP_OrbitalDrop_DefensiveTurret`
- GP-S30R AutoAcquire / GP-S29R LOS / GP-S36G BuildGrid

## Code Allowed
Yes

## Pillar 8 MVP Gate
1. Strengthen core loop? **YES** — OrbitalFerronite becomes perimeter defense.
2. Meaningful decision? **YES** — static defense vs units/economy.
3. Testable now? **YES** — combat + orbital + BuildGrid exist.
4. Bounded? **YES** — one building, no walls/upgrades/FoW.
5. Avoid speculative framework? **YES** — reuses `UGP_UnitCommandComponent`.

Verdict: **PASS**.

## Architecture decisions
- Production combat owner is `UGP_UnitCommandComponent` on `AGP_UnitBase`. No second combat framework.
- Turret is thin `AGP_BuildingBase` glue: identity, 2×2 footprint, combat Default* values, `CombatOrigin` anchor.
- AutoAcquire eligibility: Salvage Walker **or** `GP.Building.Type.DefensiveTurret`. AttackMove stays Salvage Walker only.
- Range/Damage/Cooldown/MaxHealth: `AGP_UnitBase` Default* → GAS `UGP_UnitAttributeSet` (same as Salvage Walker). Not hardcoded in fire code. `UGP_BuildingDefinition.MaxHealth` remains catalog metadata (400), mirrored on turret CDO.
- AutoAcquire sight = 600 cm (equal fire range) so stationary Attack never needs approach.
- Authored payload seam: `UGP_BuildingDefinition.SpawnedClass` + optional settings `DefensiveTurretPayloadClass`.
- Target set: existing AutoAcquire filter (enemy units, not buildings).

## Scope
Native `AGP_DefensiveTurret`, catalog SpawnedClass, BuildGrid 2×2, auto-acquire/LOS/damage, orbital Purchase/READY/Deploy, contract test, TEMP HUD enablement via payload resolve.

## Out of Scope
Wall, wall-mounted turret, FoW/vision, rotation UI, manual targeting, upgrades, ammo, power, sell/demolish, generic weapon rewrite.

## Acceptance Criteria
Operator can Purchase / READY / Deploy a Defensive Turret; spawned actor occupies 2×2; auto-attacks enemy units in range with LOS; friendly/out-of-range ignored; death releases grid.

## Validation
`gp.Building.RunDefensiveTurretContractTest` plus listed regressions. GPEditor Win64 Development + UHT.

## Stop Condition
Await operator PIE validation. **NOT MERGED.** Do not start Wall / FoW / turret upgrades.
