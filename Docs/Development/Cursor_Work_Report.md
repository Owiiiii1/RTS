# Cursor Work Report — GP-S35B Multi-Building Data Architecture

## Status
**GP-S35B_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.**

## Branch
`feature/gp-s35b-multi-building-data`  
Base `main` SHA: `3b5cdb8afff9f10b28ee6338d6aa5d2344e68a1e`  
Prior remote feature head: `3afc126468b873eaca2b61b506aef811564cb446`  
Implementation SHA: `1444f1d358bcb9e2eda0fcd17098691ddcb5bc8d`  
Shutdown-fix SHA: `a0bdeff0c04016190ac0c3e510b9cad577277438`  
Final feature head SHA: *(filled in SHA-record commit)*

## Operator FINAL PASS

### Multi-building catalog
**PASS.** TEMP HUD BUILDINGS panel shows distinct rows for Logistics Hub, Defensive Turret, Wall, Wall Turret.

### Definition-keyed READY inventory
**PASS.** Operator purchased one Defensive Turret: purchase accepted, Orbital spent, Turret `READY: 1`, other READY buckets unchanged. Turret `Deploy READY` remained disabled (no payload class / gameplay actor in this slice — expected).

### Logistics Hub compatibility
**PASS.** Purchase → Hub `READY: 1` → Deploy mode → place → DropPod completed → authored/current Logistics Hub appeared → Hub READY 0 → MaxUnits +5.

### Editor shutdown lifecycle
**PASS** after fix.

Original blocker:

```text
Assertion failed: Index >= 0
UObjectArray.h:1083
UGP_BuildingDropCatalog::ShutdownCatalog()
GPBuildingDropCatalog.cpp:40
```

Root cause: dual ownership (`TStrongObjectPtr` + manual `AddToRoot`). `RemoveFromRoot()` ran during late module shutdown after UObject-array teardown.

Fix (already on branch, SHA `a0bdeff`):

- no `AddToRoot`
- no `RemoveFromRoot`
- `TStrongObjectPtr` is sole catalog owner
- native definitions stay alive via catalog `UPROPERTY` children
- `OnEnginePreExit` releases the strong pointer while UObject is valid
- `ShutdownCatalog()` is idempotent (`Reset()` only)

Operator retest: launch Editor → PIE → BUILDINGS catalog appeared → stop PIE → close Editor completely. **No Crash Reporter / no assertion / Editor closed normally.**

## BuildingDefinition schema
`UGP_BuildingDefinition : UPrimaryDataAsset` — `DisplayName`, soft `Icon`, `BuildingTags`, soft `SpawnedClass`, `MaxHealth`, `FootprintCells`. No acquisition cost. Primary type `GPBuildingDefinition`.

## OrbitalDropDefinition schema
`UGP_OrbitalDropDefinition : UPrimaryDataAsset` — `DropTags`, `Cost` (OrbitalFerronite), soft `BuildingDefinition`. Primary type `GPOrbitalDropDefinition`. Display name resolved from BuildingDefinition.

## Cost SoT
`UGP_OrbitalDropDefinition.Cost` only. Not on BuildingDefinition. Settings `BuildingOrbitalPurchaseCost` is a deprecated Hub compatibility sync only.

## Stable identity
READY / Purchase / Deploy keyed by `FPrimaryAssetId` of the OrbitalDropDefinition. Not pointer identity. Enum is not required for new buildings.

## READY replication
Owner-only `TArray<FGP_ReadyBuildingEntry>` (`DropDefinitionId`, `ReadyCount`), `COND_OwnerOnly`. Independent buckets, no duplicate ids, no negative counts, exact-once consume, zero entries removed + UI notify, no client mutation.

## Purchase flow
`Purchase(DropDef)` → validate definition + BuildingDefinition → Cost from DropDef → GAS spend once → READY[DropDef]++.

## Deploy flow
`Deploy(DropDef, Transform)` → READY[DropDef] > 0 → interim placement → spawn DropPod → consume READY once → payload from BuildingDefinition.SpawnedClass (Hub settings fallback). No second Orbital spend.

## Logistics Hub compatibility
Native Hub DropDef identity. +5 MaxUnits only when live/operational; removed on destroy. Native actor logic (not EffectsOnPlacement). Authored `BuildingPayloadClass` / `BP_GP_LogisticsHUB` still used when Hub SpawnedClass is empty.

## DefaultGame.ini / authored BP bridge
`DefaultGame.ini` **not modified / not committed**. Deprecated settings remain the Hub cost/payload bridge.

## Legacy enum / settings
`EGP_OrbitalBuildingType` is compatibility glue only (`LogisticsHub` → native Hub DropDef). Core path is definition-based.

## Catalog lifecycle ownership
Static `TStrongObjectPtr` → catalog → UPROPERTY native/registered definitions. No process-lifetime rooted UObject.

## TEMP HUD
Layout unchanged (top-right Orbital+UNITS, bottom-right procurement, bottom-left container, bottom-center Launch, top-center match). Building panel: N catalog rows, independent READY, Purchase, Deploy only when READY > 0 **and** payload resolvable.

## BuildGrid
**Deferred.** Only `FootprintCells` metadata. No snapping, occupancy, rotation, clearance, wall rules, FoW placement. Interim free placement unchanged.

## Deferred defensive building gameplay
No turret combat, Wall actor/gameplay, Wall Turret, wall mounting, drag-building.

## Tests (all Failures=0)
- `gp.Building.RunMultiBuildingDataContractTest: Complete Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest: Complete Failures=0`
- `gp.Resource.RunUnitCapLogisticsHubContractTest: Complete Failures=0`
- `GP Resource.RunOrbitalUnitDropContractTest: Complete Failures=0`
- `gp.Match.RunWinLoseContractTest: Complete Failures=0`
- `GP Resource.RunContainerLaunchContractTest: Complete Failures=0`
- `GP Resource.RunContainerLaunchHUDContractTest: Complete Failures=0`
- `GP-S28 RegressionSuite Complete Failures=0`
- `gp.Movement.RunRTSMovementReconciliationContractTest: Complete Failures=0`
- `gp.Combat.RunAttackMoveContractTest: Complete Failures=0`
- `gp.Combat.RunAutoAcquireContractTest: Complete Failures=0`
- `GP Combat.RunSalvageWalkerContractTest: Complete Failures=0`
- `GP Combat.RunLOSFireGateContractTest: Complete Failures=0`
- `GP Combat.RunHealthBarContractTest: Complete Failures=0`

## Builds
- GPEditor Win64 Development + UHT **PASS**
- GP Win64 Development **PASS**
- GP Win64 Shipping **PASS**

## Finalization C++
**None.** Docs only.

## Exact files changed during finalization
- `Docs/Development/Claude_Tasks/GP-S35B_Multi_Building_Data.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Cursor_Work_Report.md`

**NOT MERGED.**
