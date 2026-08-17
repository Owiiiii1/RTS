# Cursor Work Report — GP-S35B Multi-Building Data Architecture

## Status
**GP-S35B_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

## Branch
`feature/gp-s35b-multi-building-data`  
Base `main` SHA: `3b5cdb8afff9f10b28ee6338d6aa5d2344e68a1e`  
Prior remote feature head: `0bc66c83f6a3ee6e84f9d1c83912927daaeb979c`  
Implementation SHA (this fix): `a0bdeff0c04016190ac0c3e510b9cad577277438`  
Feature head SHA: `a0bdeff0c04016190ac0c3e510b9cad577277438`

## Operator blocker
Full Unreal Editor close crashed:

```text
Assertion failed: Index >= 0
UObjectArray.h:1083
UGP_BuildingDropCatalog::ShutdownCatalog()
GPBuildingDropCatalog.cpp:40
```

## Root cause
The catalog was held **twice**:

1. `TStrongObjectPtr<UGP_BuildingDropCatalog>`
2. manual `AddToRoot()` / `RemoveFromRoot()`

`FGPRuntimeModule::ShutdownModule()` called `RemoveFromRoot()` after UObject-array teardown had already progressed. `TStrongObjectPtr::operator->` / `RemoveFromRoot` then hit `FUObjectArray::IndexToObject()` with a dead index.

## Lifecycle fix
- `TStrongObjectPtr` is the **only** lifetime owner.
- **`AddToRoot()` removed.**
- **`RemoveFromRoot()` removed.**
- Native Hub/Turret/Wall/WallTurret definitions remain `UPROPERTY` children of the catalog; they stay alive while the strong pointer holds the catalog.
- `FCoreDelegates::OnEnginePreExit` releases the strong pointer while the UObject system is still valid (`BindEngineLifecycle` from module startup).
- `ShutdownModule` unbinds the delegate, then `ShutdownCatalog()` (idempotent no-op if PreExit already ran).
- No leaked process-lifetime rooted UObject. No `GIsRequestingExit` / `IsValidLowLevelFast` / InternalIndex guards.

## Final ownership model
Static `TStrongObjectPtr` → `UGP_BuildingDropCatalog` (TransientPackage) → UPROPERTY arrays of native/registered definitions. `Get()` after `ShutdownCatalog()` reclaims a still-living transient object or constructs a new one with the same native PrimaryAssetId names.

## ShutdownCatalog idempotence
- Never created: `Reset()` on empty pointer.
- Once: releases strong owner.
- Repeated (tests): no-op.
- Later `Get()` recreates/reclaims a valid catalog.

## Teardown regression coverage
`gp.Building.RunMultiBuildingDataContractTest` now also covers:

- catalog create
- native definitions alive while strongly owned
- `ShutdownCatalog()` + second call
- `Get()` after shutdown restores native Hub/Turret/Wall/WallTurret identities

**Automation cannot emulate process-level GUObjectArray teardown.** Operator **Editor-close** is a required regression gate.

## Tests (all Failures=0)
- `gp.Building.RunMultiBuildingDataContractTest: Complete Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest: Complete Failures=0`
- `gp.Resource.RunUnitCapLogisticsHubContractTest: Complete Failures=0`
- `GP Resource.RunOrbitalUnitDropContractTest: Complete Failures=0`
- `gp.Match.RunWinLoseContractTest: Complete Failures=0`
- `GP-S28 RegressionSuite Complete Failures=0`

## GPEditor / UHT
`GPEditor Win64 Development` + UHT **PASS**. GP Development / Shipping not run.

## Exact changed files
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropCatalog.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropCatalog.h`
- `GP/Source/GPRuntime/Private/GPRuntime.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPMultiBuildingDataContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

Purchase/READY/DropDef schema/Hub +5/HUD/BuildGrid unchanged.

## Operator Editor-close retest required (do not self-approve)
1. Launch Editor normally
2. Enter PIE
3. Confirm BUILDINGS catalog appears
4. Stop PIE
5. Close Unreal Editor completely

Acceptance: **no Crash Reporter / assertion during Editor shutdown.**

**NOT MERGED.**
