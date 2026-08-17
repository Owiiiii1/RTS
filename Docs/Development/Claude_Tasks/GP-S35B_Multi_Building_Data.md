# GP-S35B — Multi-Building Data Architecture

## Status
**GP-S35B_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.** Do not self-approve. Await operator PIE validation.

## Slice Group
Post-GP-S34W (Match Win/Lose MVP is on verified `main` @ `3b5cdb8afff9f10b28ee6338d6aa5d2344e68a1e`)

## Branch
`feature/gp-s35b-multi-building-data`  
Base: `main` @ `3b5cdb8afff9f10b28ee6338d6aa5d2344e68a1e`  
Implementation: `1444f1d358bcb9e2eda0fcd17098691ddcb5bc8d`

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
8. Contract `gp.Building.RunMultiBuildingDataContractTest`

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
- Operator does **not** need to author eight content DAs for this slice.

## Operator validation target (do not self-approve)
1. Existing Logistics Hub flow still works (Purchase / READY / ghost / Deploy / DropPod / live Hub / +5).
2. Building panel can show multiple catalog rows (native catalog: Hub + Turret + Wall + Wall Turret). Turret/Wall have no production payload — Deploy stays disabled until a class exists.
3. Purchasing one type changes only that type's READY count.
4. Deploying Logistics Hub still uses the authored/current Hub BP via the settings bridge.

## Stop Condition
Implementation candidate complete. **NOT MERGED.** Await operator validation. Do not start BuildGrid / turret combat / Wall.
