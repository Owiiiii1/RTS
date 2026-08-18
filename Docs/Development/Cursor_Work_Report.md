# Cursor Work Report

Status: **GP-S38D_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.**

## Branch
`feature/gp-s38d-unit-building-combat-data`

## Base main SHA
`c79b017a45b1560e025cedfe262b0afde3c9cb6a`

## Feature head SHA
`e4fe2d255a1c54007707a21c379dab67cc40e57e`

## Operator PASS
Operator created a `UGP_UnitDefinition` Data Asset, assigned it on `BP_SalvageWalker` via `Unit Definition Asset`, set `MoveSpeedCmPerSecond = 100`, and confirmed in PIE that Salvage Walker moved at ~100 cm/s instead of legacy 250. Authored UnitDefinition loads and applies as the gameplay source.

Operator DataAsset and BP assignment are **not** committed.

## Final UnitDefinition schema
`UGP_UnitDefinition : UPrimaryDataAsset`, `PrimaryAssetType = GPUnitDefinition`.

- Identity: `DisplayName`
- Vitals: `MaxHealth`, `InitialHealth`, `Armor`, `DamageResistance`
- Combat: `Damage`, `AttackRangeCm`, `AttackCooldownSeconds`, `SightRangeCm`, `AutoAcquireScanIntervalSeconds`, `AttackFacingRotationSpeedDegreesPerSecond`
- Movement: `MoveSpeedCmPerSecond`
- Behavior: `RetaliationPursuitSeconds` (default 5.0, ClampMin 0)

No CombatProfile / MovementProfile / VisionProfile hierarchy.

## Canonical stat ownership

| Concern | Canonical |
| --- | --- |
| Initial/base vitals + combat + sight + facing + MoveSpeed + RetaliationPursuitSeconds | `UGP_UnitDefinition` |
| Runtime Health / Damage / Armor / Resistance / Cooldown / Range | `UGP_UnitAttributeSet` (GAS). Definition initializes base values only. |
| Runtime sight / scan / facing | `UGP_UnitCommandComponent` (initialized from definition) |
| Runtime MoveSpeed | `UGP_MovementComponent` (initialized from definition when `MoveSpeedCmPerSecond > 0`) |
| Building identity / icon / tags / SpawnedClass / footprint | `UGP_BuildingDefinition` |
| BuildingDef.MaxHealth | compatibility fallback only |
| Acquisition cost / slots / READY | orbital drop / unit delivery layer |

## Async soft-loading lifecycle
`TSoftObjectPtr<UGP_UnitDefinition> UnitDefinitionAsset`. No hard ref. No `LoadSynchronous`.

- Empty ref → immediate Default* / CDO fallback
- Already loaded → immediate apply
- Valid unloaded soft ref → `UAssetManager::GetStreamableManager().RequestAsyncLoad` → apply on completion
- Load failure → `GP UnitDefinitionLoadFailed` + deterministic fallback

While pending: no stale GAS init, no AutoAcquire, no premature lock-in. EndPlay cancels the pending handle; callback no-ops if abandoned.

## Fallback precedence
1. Resolved loaded `UGP_UnitDefinition`
2. Existing actor / component defaults

Native catalog is not auto-applied by class.

## BuildingDefinition relationship
Soft `UnitDefinition`. Canonical MaxHealth = `UnitDefinition.MaxHealth` via `ResolveCanonicalMaxHealth()`. `BuildingDefinition.MaxHealth` never outranks a loaded UnitDefinition.

## Baseline values (no rebalance)

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
| MoveSpeedCmPerSecond | 600 | 250 | 0 |
| RetaliationPursuitSeconds | 5.0 | 5.0 | 5.0 |

## RetaliationPursuitSeconds
DATA ONLY. Baseline / empty / failure fallback = 5.0. No damage reaction, pursuit timer, or attacker callback. **GP-S39R** is the next separate slice.

## Exact tests

| Command | Result |
| --- | --- |
| `gp.Units.RunUnitDefinitionContractTest` | Complete Failures=0 |
| `gp.Building.RunDefensiveTurretContractTest` | Complete Failures=0 |
| `gp.Combat.RunAutoAcquireContractTest` | Complete Failures=0 |
| `gp.Combat.RunAttackMoveContractTest` | Complete Failures=0 |
| `gp.Combat.RunLOSFireGateContractTest` | Complete Failures=0 |
| `gp.Building.RunBuildGridContractTest` | Complete Failures=0 |
| `gp.Building.RunMultiBuildingDataContractTest` | Complete Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | Complete Failures=0 |
| `gp.Resource.RunUnitCapLogisticsHubContractTest` | Complete Failures=0 |
| `gp.Resource.RunOrbitalUnitDropContractTest` | Complete Failures=0 |
| `gp.Resource.RunContainerLaunchContractTest` | Complete Failures=0 |
| `gp.Match.RunWinLoseContractTest` | Complete Failures=0 |
| `gp.Resource.RunS28RegressionSuite` | GP-S28 RegressionSuite Complete Failures=0 |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | Complete Failures=0 |

All: **Failures=0**.

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASS** |
| GP Win64 Development | **PASS** |
| GP Win64 Shipping | **PASS** |

## Exact changed files
Diff vs `c79b017a45b1560e025cedfe262b0afde3c9cb6a` plus this finalization:

- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Claude_Tasks/GP-S38D_Unit_Building_Combat_Data.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/TDD/05_Unit_Architecture.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/10_Data_Assets.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingDefinition.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPUnitDefinitionContractTest.cpp`
- `GP/Source/GPRuntime/Private/GPRuntime.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropCatalog.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitDefinition.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitDefinitionCatalog.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinition.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinitionCatalog.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinitionContractTest.h`

## Operator DataAsset / BP not committed
Not added:

- `GP/Content/GrimProtocol/DataAssets/Units/DA_GP_Unit_SalvageWalker.uasset`
- `BP_SalvageWalker` / `BP_GP_SalvageWalker` class-default assignment
- `BP_GP_DefensiveTurret`

## Protected assets untouched
Confirmed **not** committed:

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- other operator Blueprint / material / VFX assets
- untracked `GP/Content/Basic_VFX/`, `GrimProtocol/Blueprint/`, `Materials/`, `Mixed_Magic_VFX_Pack/`, `RocketThrusterExhaustFX/`, `Tools/`

**NOT MERGED.**
