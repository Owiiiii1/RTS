# Cursor Work Report

Status: **GP-S38D_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch
`feature/gp-s38d-unit-building-combat-data`

## Base main SHA
`c79b017a45b1560e025cedfe262b0afde3c9cb6a`

## Feature head SHA
Pending correction commit on this branch.

## Previous already-loaded-only defect
`ResolveLoadedUnitDefinition()` used `Get()` / `ResolveObject()` only. A valid authored soft `UnitDefinitionAsset` that was not already resident silently fell through to Default* / component CDO values. No async request. Contracts only covered already-resident native catalog objects.

## New async soft-loading lifecycle
`TSoftObjectPtr<UGP_UnitDefinition>` unchanged. No hard ref. No `LoadSynchronous`.

`BeginPlay`: ASC ActorInfo → `BeginUnitDefinitionInitialization()`:

1. Empty soft ref → immediate fallback complete.
2. Already-loaded definition → apply immediately.
3. Valid unloaded soft ref → `UAssetManager::GetStreamableManager().RequestAsyncLoad` → callback resolves and completes.

GAS / command / movement tuning and unit-cap register run only inside `CompleteUnitDefinitionInitialization`. One complete path. No second init.

## Empty-ref fallback
Immediate. Uses existing `Default*` + command/movement CDO / derived ctor values. `GetRetaliationPursuitSeconds()` = **5.0** (documented baseline).

## Load-failure fallback
Logs `GP UnitDefinitionLoadFailed` (null handle or resolve failed). Then the same Default* fallback. No hang.

## When GAS / command / movement become active
Only after `IsUnitDefinitionReady()`:

- GAS base values written from definition or Default*
- Command sight / scan / facing written from definition when present
- MoveSpeed written on mobile units when `MoveSpeedCmPerSecond > 0`
- Then `RefreshCombatAutoAcquireTimer()`

## AutoAcquire readiness
`StartCombatAutoAcquireTimer`, `IsEligibleForCombatAutoAcquire`, and `IsEligibleForAttackMoveAcquire` require `IsUnitDefinitionReady()`. Component `BeginPlay` cannot start scanning on stale/zero GAS. After complete, timer starts with the applied interval.

## EndPlay safety
`CancelPendingUnitDefinitionLoad()` cancels an in-flight streamable handle, marks abandoned, and resets pending. Callback no-ops if abandoned / destroyed / already ready.

## RetaliationPursuitSeconds (data only)
Canonical rule: pending and empty/failure fallback = **5.0**. Successful definition apply uses the definition value. No GP-S39R behavior.

## Unloaded-soft-ref test coverage
`gp.Units.RunUnitDefinitionContractTest` now proves:

- A already-loaded definition
- B empty soft ref → legacy fallback
- C valid unresolved soft path issues real `RequestAsyncLoad`
- D GAS stays at AttributeSet zeros (not locked to Default* 200) until completion
- E/F command + MoveSpeed apply after completion
- G missing path → deterministic fallback, no crash
- H EndPlay while pending → safe destroy

Non-shipping hold/inject seam exists so automation can observe the pending window. Production path still calls `RequestAsyncLoad`.

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
Correction vs prior S38D implementation:

- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Claude_Tasks/GP-S38D_Unit_Building_Combat_Data.md`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/TDD/05_Unit_Architecture.md`
- `GP/Source/GPRuntime/Private/Debug/GPUnitDefinitionContractTest.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
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
