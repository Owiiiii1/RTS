# Cursor Work Report

Status: **GP-S38D_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch
`feature/gp-s38d-unit-building-combat-data`

## Base main SHA
`c79b017a45b1560e025cedfe262b0afde3c9cb6a`

## Feature head SHA
`cc94ac85c3069431ab4ec4f49b7ae0fc335a71a6`

## Factual old stat ownership (pre-S38D)

| Stat | Owner |
| --- | --- |
| MaxHealth / Health / Damage / Armor / DamageResistance / AttackCooldown / AttackRange | `AGP_UnitBase` `Default*` → `InitializeCombatAttributesIfNeeded()` → `UGP_UnitAttributeSet` |
| SightRange / AutoAcquireScanInterval / AttackFacingRotationSpeed | `UGP_UnitCommandComponent` CDO / derived ctor |
| MoveSpeed | `UGP_MovementComponent` CDO (`600`) or `AGP_SalvageWalker` ctor (`250`) |
| Building catalog MaxHealth | `UGP_BuildingDefinition.MaxHealth` metadata only — not applied to spawned actors |
| CapabilityTags | `AGP_UnitBase` class defaults (left in place) |
| Acquisition cost / transport slots | orbital drop / unit delivery layer |

Old TDD UnitDefinition schemas (Icon / Mesh / Cost / AllowedCommands / GrantedAbilities) did **not** match disk.

## UnitDefinition schema

`UGP_UnitDefinition : UPrimaryDataAsset`, `PrimaryAssetType = GPUnitDefinition`.

- Identity: `DisplayName` only
- Vitals: `MaxHealth`, `InitialHealth`, `Armor`, `DamageResistance`
- Combat: `Damage`, `AttackRangeCm`, `AttackCooldownSeconds`, `SightRangeCm`, `AutoAcquireScanIntervalSeconds`, `AttackFacingRotationSpeedDegreesPerSecond`
- Movement: `MoveSpeedCmPerSecond` only
- Behavior: `RetaliationPursuitSeconds` (default 5.0, ClampMin 0, `GP|Behavior|Retaliation`)

No CombatProfile / MovementProfile / VisionProfile hierarchy.

## Canonical ownership after migration

| Concern | Canonical |
| --- | --- |
| Initial/base vitals + combat + sight + facing + MoveSpeed + RetaliationPursuitSeconds | `UGP_UnitDefinition` |
| Runtime Health / Damage / Armor / Resistance / Cooldown / Range | `UGP_UnitAttributeSet` (GAS). Definition initializes base values only. |
| Sight / scan / facing at runtime | `UGP_UnitCommandComponent` (initialized from definition when loaded) |
| MoveSpeed at runtime | `UGP_MovementComponent` for mobile units when `MoveSpeedCmPerSecond > 0` |
| RetaliationPursuitSeconds | readable, unused until GP-S39R |
| Building identity / icon / tags / SpawnedClass / footprint | `UGP_BuildingDefinition` |
| Acquisition cost / DropTags / READY | `UGP_OrbitalDropDefinition` |
| Unit delivery cost / slots / manifest | orbital unit delivery layer |

`AGP_UnitBase` soft ref: `TSoftObjectPtr<UGP_UnitDefinition> UnitDefinitionAsset`. Resolver is already-loaded only (`Get()` / `ResolveObject()`). No `LoadSynchronous`. No replicated CurrentDamage / CurrentRange / DataAsset.

## BuildingDefinition relationship

`UGP_BuildingDefinition` now has soft `UnitDefinition`. `MaxHealth` is **compatibility fallback** (`GP|Vitals|Fallback`). Canonical MaxHealth = `UnitDefinition.MaxHealth` via `ResolveCanonicalMaxHealth()`. Native turret building links `DA_GP_Unit_DefensiveTurret`. BuildingDef.MaxHealth never outranks a loaded UnitDefinition.

## Fallback precedence

1. Resolved loaded `UGP_UnitDefinition`
2. Existing actor / component defaults (`Default*`, command CDO, movement CDO / derived ctor)

Empty `UnitDefinitionAsset` does not break authored BPs. Native catalog is **not** auto-applied by class.

## Worker values (preserved)

MaxHealth/Health 100, Damage 25, Armor 0, Resistance 0, AttackRange 250, AttackCooldown 1.0, Sight 900, Scan 0.35, Facing 360, MoveSpeed **600** (live movement CDO; TDD 350 was stale), RetaliationPursuitSeconds 5.0.

## Salvage Walker values (preserved)

MaxHealth/Health 200, Damage 20, Armor 0, Resistance 0, AttackRange 600, AttackCooldown 1.0, Sight 900, Scan 0.35, Facing 360, MoveSpeed 250, RetaliationPursuitSeconds 5.0.

## Defensive Turret values (preserved)

MaxHealth/Health 400, Damage 20, Armor 0, Resistance 0, AttackRange 600, AttackCooldown 1.0, Sight 600, Scan 0.35, Facing 360, MoveSpeed 0 (no movement write), RetaliationPursuitSeconds 5.0.

## Movement ownership decision

Unit-type **MoveSpeed** is on UnitDefinition. NavProjectionExtent, RepathInterval, BlockedFail, Separation, AcceptanceRadius stay on `UGP_MovementComponent` (algorithm / system tuning).

## RetaliationPursuitSeconds

DATA ONLY. Default 5.0. `0` = disabled. `>0` = max pursuit duration after reacting to attacker. No damage reaction, no attacker callback, no pursuit timer, no new command state. GP-S39R owns behavior.

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
| GP Win64 Development | not run (candidate) |
| GP Win64 Shipping | not run (candidate) |

## Exact changed files
Diff vs `c79b017a45b1560e025cedfe262b0afde3c9cb6a`:

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
- `GP/Source/GPRuntime/Private/Units/GPUnitDefinition.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitDefinitionCatalog.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinition.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinitionCatalog.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinitionContractTest.h`

## Protected assets untouched
Confirmed **not** committed:

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `BP_GP_DefensiveTurret` and other operator Blueprint/material/VFX assets
- untracked `GP/Content/Basic_VFX/`, `GrimProtocol/Blueprint/`, `Materials/`, `Mixed_Magic_VFX_Pack/`, `RocketThrusterExhaustFX/`, `Tools/`

**NOT MERGED.**
**NOT FINALIZED.**
