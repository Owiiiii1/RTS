# GP-S35B — Multi-Building Data Architecture

## Status
**GP-S35B_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.**

## Slice Group
Post-GP-S34W (Match Win/Lose MVP is on verified `main` @ `3b5cdb8afff9f10b28ee6338d6aa5d2344e68a1e`)

## Branch
`feature/gp-s35b-multi-building-data`  
Base: `main` @ `3b5cdb8afff9f10b28ee6338d6aa5d2344e68a1e`  
Prior remote feature head: `3afc126468b873eaca2b61b506aef811564cb446`  
Implementation: `1444f1d358bcb9e2eda0fcd17098691ddcb5bc8d`  
Shutdown fix: `a0bdeff0c04016190ac0c3e510b9cad577277438`  
Finalization: `60a986281d1f5dfa42a16cad177a84cb670308e6`

## Goal
Replace the single-building temporary architecture (`BuildingOrbitalPurchaseCost` + `BuildingPayloadClass` + `EGP_OrbitalBuildingType { LogisticsHub }` + `ReadyLogisticsHubCount`) with a data-driven multi-type acquisition model. Architecture only. Logistics Hub gameplay must survive unchanged. No turret combat, Wall gameplay, wall mounting, BuildGrid, or drag-building.

## Canonical ownership

| Asset | Owns |
| --- | --- |
| `UGP_BuildingDefinition` | Intrinsic identity / gameplay: `DisplayName`, soft `Icon`, `BuildingTags`, soft `SpawnedClass`, `MaxHealth`, `FootprintCells` |
| `UGP_OrbitalDropDefinition` | Acquisition / delivery: `Cost` (OrbitalFerronite), `DropTags`, soft `BuildingDefinition` |

Acquisition Cost SoT is **only** `UGP_OrbitalDropDefinition.Cost`. Display-name SoT is `BuildingDefinition.DisplayName` (DropDef resolves via `GetAcquisitionDisplayName()`). READY is keyed by stable `FPrimaryAssetId` of the DropDef (`GPOrbitalDropDefinition:<AssetName>`), not by enum and not by pointer address.

## In scope (delivered)
1. Native `UPrimaryDataAsset` types `UGP_BuildingDefinition` and `UGP_OrbitalDropDefinition` in `GPRuntime`
2. Native bootstrap catalog (`UGP_BuildingDropCatalog`) with four building + four drop identities (Logistics Hub, Defensive Turret, Wall, Wall Turret)
3. Owner-only replicated `TArray<FGP_ReadyBuildingEntry>` (`DropDefinitionId` + `ReadyCount`)
4. Purchase(DropDef) / Deploy(DropDef, Transform) authority path
5. DropPod building payload class from BuildingDefinition (Hub settings fallback)
6. TEMP HUD building panel: Hub row preserved + extra catalog rows (name / cost / READY / Purchase / Deploy)
7. Logistics Hub Purchase → READY → ghost → Deploy → DropPod → live Hub → +5 MaxUnits / destroy removes +5
8. Catalog lifetime: `TStrongObjectPtr` only; `OnEnginePreExit` release; no `AddToRoot`/`RemoveFromRoot`
9. Contract `gp.Building.RunMultiBuildingDataContractTest`

## Out of scope (deferred)
- Turret combat, Wall gameplay, wall mounting, drag-building
- BuildGrid actor/component, snapping, occupancy, rotation footprints, FoW placement
- Generic `EffectsOnPlacement` migration of Hub +5
- Faction/session AssetManager preload rewrite
- Authored `.uasset` Data Assets in git (native/test catalog is sufficient)
- Production Order Menu / CommonUI redesign
- Moving descent timing off `UGP_OrbitalDeliverySettings`

## Compatibility
- `EGP_OrbitalBuildingType` retained as **deprecated glue** (`LogisticsHub` maps to native Hub DropDef). New logic is definition-based.
- `UGP_OrbitalDeliverySettings.BuildingOrbitalPurchaseCost` / `BuildingPayloadClass` are `DeprecatedProperty` but remain the operator `DefaultGame.ini` bridge for Hub cost + authored `BP_GP_LogisticsHUB`. **Do not modify/commit DefaultGame.ini.**
- Native Hub BuildingDef `SpawnedClass` stays empty so S32R tests can still mutate settings payload class.

## Catalog lifecycle
Original Editor-close blocker: `Assertion failed: Index >= 0` in `FUObjectArray::IndexToObject` from `ShutdownCatalog()` `RemoveFromRoot()` after UObject-array teardown. Dual ownership (`TStrongObjectPtr` + `AddToRoot`) was the cause. Fix: strong pointer is sole owner; release on `OnEnginePreExit`; `ShutdownCatalog()` is idempotent.

## Operator validation — FINAL PASS (2026-08-17)

### Multi-building catalog
PASS. TEMP HUD BUILDINGS panel shows distinct rows: Logistics Hub, Defensive Turret, Wall, Wall Turret.

### Definition-keyed READY
PASS. Purchasing one Defensive Turret spent Orbital, set Turret `READY: 1`, left other buckets unchanged. Turret `Deploy READY` stayed disabled (no payload class in this slice — expected).

### Logistics Hub compatibility
PASS. Purchase → Hub `READY: 1` → Deploy → DropPod → authored/current Hub appeared → READY 0 → MaxUnits +5.

### Editor shutdown
PASS after fix. Launch Editor → PIE → catalog visible → stop PIE → close Editor completely: **no Crash Reporter / no assertion**.

## Finalization note
Docs-only. No C++ changes during finalization. GPEditor+UHT / GP Win64 Development / GP Win64 Shipping **PASS**. Listed regressions **Failures=0**.

## Stop condition
Operator PIE FINAL PASS complete. **NOT MERGED.** Human merge only. Agent must **not** merge. Do not start BuildGrid / turret combat / Wall.
