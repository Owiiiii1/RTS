# GP-S37T — Defensive Turret MVP

## Status
**GP-S37T_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.** Await human merge.

## Slice Group
Post-GP-S36G (BuildGrid is on verified `main` @ `9ace159714b5eca0f79e4985fc2496d34cbb7cc3`)

## Branch
`feature/gp-s37t-defensive-turret-mvp`  
Base: `origin/main` @ `9ace159714b5eca0f79e4985fc2496d34cbb7cc3`  
Implementation: `7aa0565f9e10e9fe447fe9898835eba28ddc9b81`  
Building-target correction: `b88926acc6415abc8efe07729ac48837ce8a5fae`  
Finalization: `7ce40f6de4f420616dddd911775ad853a094b670`

## Goal
Add the first functional combat building after Logistics Hub: Purchase → READY → Deploy → DropPod → operational `AGP_DefensiveTurret` that auto-acquires and damages enemies through the existing combat pipeline.

## Operator PASS (final)

Operator confirmed the main gameplay path:

- `BP_GP_DefensiveTurret` authored child created operator-side
- Defensive Turret purchases
- READY / Deploy work
- DropPod lands the turret
- authored BP payload works
- turret appears on BuildGrid
- turret auto-detects enemies
- turret fires
- damage applies

Contracts already proved: enemy building targets, friendly reject, LOS, cooldown, health/death, grid release, legacy Salvage Walker target surface unchanged.

## Final architecture

- Production combat owner remains `UGP_UnitCommandComponent` on `AGP_UnitBase`. No `UGP_CombatComponent`. No second framework.
- `AGP_DefensiveTurret` is thin `AGP_BuildingBase` glue: identity, native 2×2 footprint, Default* combat stats, `CombatOrigin`.
- Idle AutoAcquire timer + `GPCombatLOS` + `UGP_GE_Damage_Basic`.
- Target policy seam: `EGP_AutoAcquireMode` + `IsEligibleAutoAcquireTarget`. Defensive Turret idle may target valid enemy units **and** buildings. Salvage Walker idle + AttackMove stay unit-only.
- Candidates still pass `ValidateAttackTarget` (same team / dead / self / invalid rejected).
- AttackMove remains Salvage Walker only. Buildings reject Move / AttackMove.
- Orbital Purchase / READY / Deploy / DropPod unchanged (GP-S35B/S36G).
- BuildGrid occupancy uses existing live `PlacementFootprintBounds` (native 400×400 cm → 2×2). Death / EndPlay releases cells.

## Follow-up (record only — not implemented)

### GP-S38D Unit/Building Combat Data
Combat/stat tuning is currently fragmented:

- `AGP_UnitBase` EditDefaultsOnly: `DefaultMaxHealth`, `DefaultHealth`, `DefaultDamage`, `DefaultArmor`, `DefaultDamageResistance`, `DefaultAttackCooldown`, `DefaultAttackRange`
- `UGP_UnitCommandComponent`: `AutoAcquireScanIntervalSeconds`, `AutoAcquireSightRangeCm`, `AttackFacingRotationSpeedDegreesPerSecond`

`AGP_UnitBase` already documents future UnitDefinition as the canonical source. Next architecture slice should give designers per-type combat/stat configuration instead of scattered BP/C++ defaults. **Do not implement in S37T.**

### GP-S39R Timed Retaliation Pursuit
Damage reaction / retaliation is missing. Factual seam: `AGP_UnitBase::ApplyDamageFromUnit(SourceUnit, ...)` already knows the authoritative attacker.

Desired later behavior: a mobile combat unit hit by an unseen attacker may pursue/react for a configurable limited time (proposed baseline 5s). If the attacker becomes visible/valid, the normal Attack FSM continues. If the timeout expires without valid engagement, stop pursuit / return Idle. Manual player command overrides retaliation. No infinite pursuit. **Do not implement in S37T.**

## Tests
All Failures=0:

- `gp.Building.RunDefensiveTurretContractTest`
- `gp.Combat.RunAutoAcquireContractTest`
- `gp.Combat.RunAttackMoveContractTest`
- `gp.Combat.RunLOSFireGateContractTest`
- `gp.Building.RunBuildGridContractTest`
- `gp.Building.RunMultiBuildingDataContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest`
- `gp.Resource.RunUnitCapLogisticsHubContractTest`
- `gp.Resource.RunOrbitalUnitDropContractTest`
- `gp.Resource.RunContainerLaunchContractTest`
- `gp.Match.RunWinLoseContractTest`
- `gp.Resource.RunS28RegressionSuite`

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development **PASS**.  
GP Win64 Shipping **PASS**.

## Stop Condition
**NOT MERGED.** Human merge only. Do not start Wall / FoW / GP-S38D combat-data rewrite / GP-S39R retaliation without explicit assignment.
