# GP-S38D — Unit/Building Combat Data

## Status
**GP-S38D_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Slice Group
Post-GP-S37T (Defensive Turret MVP is on verified `main` @ `c79b017a45b1560e025cedfe262b0afde3c9cb6a`)

## Branch
`feature/gp-s38d-unit-building-combat-data`  
Base: `origin/main` @ `c79b017a45b1560e025cedfe262b0afde3c9cb6a`  
Implementation: `cc94ac85c3069431ab4ec4f49b7ae0fc335a71a6`

## Goal
One designer-facing per-type `UGP_UnitDefinition` (`UPrimaryDataAsset`, `PrimaryAssetType = GPUnitDefinition`) as the canonical **initial/base** source for unit/building gameplay stats. No CombatProfile / MovementProfile / VisionProfile hierarchy. No balance pass. No retaliation behavior.

## Pillar 8

| Question | Answer |
| --- | --- |
| Strengthens core loop? | YES — orbital units/buildings become tunable and production-usable. |
| Meaningful player decision? | Indirectly YES — enables real balance differentiation between unit/building choices. |
| Testable now? | YES |
| Bounded? | YES — data ownership + initialization only. |
| Avoid speculative framework? | YES — one UnitDefinition, no profile hierarchy. |

Verdict **PASS**.

## Factual old ownership (pre-S38D)

| Stat | Owner before S38D |
| --- | --- |
| MaxHealth / Health / Damage / Armor / DamageResistance / AttackCooldown / AttackRange | `AGP_UnitBase` `Default*` → `InitializeCombatAttributesIfNeeded()` → `UGP_UnitAttributeSet` |
| SightRange / AutoAcquireScanInterval / AttackFacingRotationSpeed | `UGP_UnitCommandComponent` CDO / derived ctor |
| MoveSpeed | `UGP_MovementComponent` CDO / `AGP_SalvageWalker` ctor |
| Building catalog MaxHealth | `UGP_BuildingDefinition.MaxHealth` metadata only — **not** applied to spawned actors |
| CapabilityTags / unit identity tags | `AGP_UnitBase` class defaults (not migrated) |
| Acquisition cost / transport slots | Orbital drop / unit delivery layer (unchanged) |

TDD schemas that listed Icon / Mesh / Cost / AllowedCommands / GrantedAbilities on `UGP_UnitDefinition` did **not** match disk. Code wins.

## Canonical ownership after S38D

| Concern | Canonical |
| --- | --- |
| Initial/base vitals + combat + sight + facing + MoveSpeed + RetaliationPursuitSeconds | `UGP_UnitDefinition` |
| Runtime Health / Damage / Armor / Resistance / Cooldown / Range | still `UGP_UnitAttributeSet` (GAS). Definition initializes base values only. |
| Sight / scan / facing at runtime | `UGP_UnitCommandComponent` (initialized from definition when loaded) |
| MoveSpeed at runtime | `UGP_MovementComponent` for mobile units when `MoveSpeedCmPerSecond > 0` |
| RetaliationPursuitSeconds | readable, **unused** until GP-S39R |
| Building identity / icon / tags / SpawnedClass / footprint | `UGP_BuildingDefinition` |
| Acquisition cost / DropTags / READY key | `UGP_OrbitalDropDefinition` |
| Unit delivery cost / slots / manifest | orbital unit delivery layer (not UnitDefinition) |

`UGP_UnitDefinition` does **not** replace GAS.

## Schema (`UGP_UnitDefinition`)

Identity: `DisplayName` only. CapabilityTags / UnitTags stay on `AGP_UnitBase`.

Vitals: `MaxHealth`, `InitialHealth`, `Armor`, `DamageResistance`

Combat: `Damage`, `AttackRangeCm`, `AttackCooldownSeconds`, `SightRangeCm`, `AutoAcquireScanIntervalSeconds`, `AttackFacingRotationSpeedDegreesPerSecond`

Movement: `MoveSpeedCmPerSecond` only (unit-type balance)

Behavior: `RetaliationPursuitSeconds` (default 5.0, ClampMin 0, category `GP|Behavior|Retaliation`)

**Not migrated** (system / nav algorithm tuning stays on `UGP_MovementComponent`): AcceptanceRadius, NavProjectionExtent, RepathInterval, BlockedFail, Separation.

## Runtime init

`AGP_UnitBase` has:

```
TSoftObjectPtr<UGP_UnitDefinition> UnitDefinitionAsset;
```

No `LoadSynchronous`. Valid non-empty soft ref is `UAssetManager::GetStreamableManager().RequestAsyncLoad`. Empty ref falls back immediately. Load failure logs `GP UnitDefinitionLoadFailed` and uses Default* fallback.

`BeginPlay` order: ASC ActorInfo → `BeginUnitDefinitionInitialization()` (apply now, or async then apply). GAS / command / movement tuning and unit-cap register run only after definition init completes. AutoAcquire timer starts only when `IsUnitDefinitionReady()`.

`GetRetaliationPursuitSeconds()`: documented baseline **5.0** while pending and for empty/failure fallback. Definition value after successful apply. Data only until GP-S39R.

EndPlay cancels the pending streamable handle. Callback no-ops if abandoned/destroyed.

## Fallback precedence

1. Resolved loaded `UGP_UnitDefinition`
2. Existing actor / component defaults (`Default*`, command CDO, movement CDO / derived ctor)

Empty `UnitDefinitionAsset` must not break authored BPs. Native catalog is **not** auto-applied by class (that would break empty-ref fallback).

`UGP_BuildingDefinition.MaxHealth` is compatibility-only. Canonical MaxHealth = `UnitDefinition.MaxHealth` via `ResolveCanonicalMaxHealth()`. BuildingDef never outranks a loaded UnitDefinition.

## Native bootstrap values (no rebalance)

Copied from current C++ / CDO. Contracts do not depend on binary `.uasset` DataAssets.

| Field | Worker | Salvage Walker | Defensive Turret |
| --- | --- | --- | --- |
| MaxHealth / InitialHealth | 100 | 200 | 400 |
| Armor / DamageResistance | 0 / 0 | 0 / 0 | 0 / 0 |
| Damage | 25 | 20 | 20 |
| AttackRangeCm | 250 | 600 | 600 |
| AttackCooldownSeconds | 1.0 | 1.0 | 1.0 |
| SightRangeCm | 900 | 900 | 600 |
| AutoAcquireScanIntervalSeconds | 0.35 | 0.35 | 0.35 |
| AttackFacingRotationSpeedDegreesPerSecond | 360 | 360 | 360 |
| MoveSpeedCmPerSecond | 600 | 250 | 0 (no movement write) |
| RetaliationPursuitSeconds | 5.0 | 5.0 | 5.0 |

Worker MoveSpeed **600** is the live `UGP_MovementComponent` CDO. TDD 350 is stale.

## Retaliation field

DATA ONLY. `0` = pursuit disabled. `>0` = max pursuit duration after reacting to attacker. No damage reaction, no attacker callback, no pursuit timer, no new command state. **GP-S39R** owns behavior.

## Designer workflow (operator-side)

Expected later authored assets:

- `DA_GP_Unit_Worker`
- `DA_GP_Unit_SalvageWalker`
- `DA_GP_Unit_DefensiveTurret`

BP class defaults (`BP_GP_Worker` / `BP_GP_SalvageWalker` / `BP_GP_DefensiveTurret`) point at the matching UnitDefinition. BuildingDefinition may soft-ref the same UnitDefinition. This slice does **not** commit binary DataAssets.

Operator PIE check: assign a temporary UnitDefinition, change AttackRange or MoveSpeed, confirm gameplay changes.

## Tests
All Failures=0:

- `gp.Units.RunUnitDefinitionContractTest` (A–L + unloaded soft-ref C–H)
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
- `gp.Movement.RunRTSMovementReconciliationContractTest`

## Builds
GPEditor Win64 Development + UHT **PASS**.  
No GP Development / Shipping in this candidate.

## Out of scope
Retaliation behavior, damage reaction, pursuit timer, AI controller / BT, FoW, Wall / Wall Turret, upgrades, research modifiers, weapon inventory / ammo, generic weapon framework, runtime stat UI, balance pass, save/load, nav-algorithm dump into UnitDefinition, binary authored DA assets, `DefaultGame.ini` AssetManager scan.

## Stop Condition
**NOT MERGED. NOT FINALIZED.** Await operator PIE. Do not start GP-S39R / Wall / FoW.
