# Cursor Work Report — GP-S35B Multi-Building Data Architecture

## Status
**GP-S35B_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

## Branch
`feature/gp-s35b-multi-building-data`  
Base `main` SHA: `3b5cdb8afff9f10b28ee6338d6aa5d2344e68a1e`  
Feature head SHA: *(filled in SHA-record commit)*

## Pre-slice temporary architecture found
- One `UGP_OrbitalDeliverySettings.BuildingOrbitalPurchaseCost`
- One `BuildingPayloadClass` (operator `DefaultGame.ini` → authored Logistics Hub BP)
- `EGP_OrbitalBuildingType { None, LogisticsHub }` as catalog key
- `UGP_OrbitalBuildingInventoryComponent::ReadyLogisticsHubCount` (single int)
- Purchase/Deploy RPCs and TEMP HUD assumed one Logistics Hub READY counter

## BuildingDefinition schema implemented (`UGP_BuildingDefinition : UPrimaryDataAsset`)
- Primary type `GPBuildingDefinition`; identity = `(Type, GetFName())`
- `DisplayName` (acquisition UI SoT)
- soft `Icon` (`TSoftObjectPtr<UTexture2D>`)
- `BuildingTags`
- soft `SpawnedClass` (`TSoftClassPtr<AGP_BuildingBase>`)
- `MaxHealth`
- `FootprintCells` (`FIntPoint`, stored for future BuildGrid; unused by interim placement)
- `ResolveLoadedSpawnedClass()` — already-loaded / `ResolveObject` only. No `LoadObject` / `ConstructorHelpers` / `/Game/` paths

## OrbitalDropDefinition schema implemented (`UGP_OrbitalDropDefinition : UPrimaryDataAsset`)
- Primary type `GPOrbitalDropDefinition`; identity = `(Type, GetFName())`
- `DropTags`
- `Cost` (OrbitalFerronite) — **only acquisition Cost SoT**
- soft `BuildingDefinition` (`TSoftObjectPtr<UGP_BuildingDefinition>`)
- `GetAcquisitionDisplayName()` resolves BuildingDefinition.DisplayName
- Descent timing **not** moved (stays on settings)

## Stable identity choice
READY / Purchase / Deploy / HUD rows use `FPrimaryAssetId` of `UGP_OrbitalDropDefinition` (replicable, not pointer identity). Native catalog names: `DA_GP_OrbitalDrop_LogisticsHub` / `DefensiveTurret` / `Wall` / `WallTurret`.

## READY inventory representation / replication
`TArray<FGP_ReadyBuildingEntry>` (`DropDefinitionId`, `ReadyCount`), `COND_OwnerOnly`, `ReplicatedUsing=OnRep_ReadyEntries`. No TMap replication. Zero-count entries removed. Same id does not create a second bucket. `OnReadyChanged(FPrimaryAssetId, int32)` includes which definition changed. Client mutation not exposed.

APIs: `GetReadyCount` / `AuthorityAddReady` / `AuthorityTryConsumeReady` for `FPrimaryAssetId`, `UGP_OrbitalDropDefinition*`, and deprecated enum glue.

## Purchase flow
`AuthorityPurchaseBuilding(World, PS, DropDef)`:
Finished gate → valid DropDef + loaded BuildingDefinition → MainBase → Cost from DropDef → Orbital check → `GE_GP_SpendOrbital` once → READY[DropDef]++. No READY if spend fails. Cost is never read from BuildingDefinition.

## Deploy flow
`AuthorityDeployBuilding(World, PS, DropDef, Transform)`:
Finished gate → READY[DropDef] > 0 → interim placement (finite transform, MainBase radius, overlap) → payload class from BuildingDefinition (Hub settings fallback) → spawn DropPod → consume READY exactly once → pod spawns `SpawnedClass` with team. No second Orbital spend. Ghost cancel does not consume. Duplicate deploy → `NoReadyInventory`.

## Logistics Hub compatibility
Native Hub DropDef + BuildingDef identities exist. +5 MaxUnits remains **native** on `AGP_LogisticsHub` (live/operational only; removed on destroy). Not migrated to `EffectsOnPlacement`. Enum Purchase/Deploy still maps `LogisticsHub` → native Hub DropDef.

## DefaultGame.ini / authored BP compatibility
`DefaultGame.ini` **not modified / not committed**.  
`BuildingOrbitalPurchaseCost` is synced onto the native Hub DropDef.Cost on catalog access.  
Hub `SpawnedClass` stays empty so payload resolves through `Settings->ResolveBuildingPayloadClass()` (authored `BP_GP_LogisticsHUB` if configured, else native `AGP_LogisticsHub`). This preserves operator-authored Hub visuals and S32R payload-class mutation tests.

**Later authoring (not required for this candidate):** content DAs `DA_GP_Building_*` + `DA_GP_OrbitalDrop_*` under `/Game/GrimProtocol/DataAssets/`. Do not fabricate `.uasset` files in git.

## Old enum / settings migration
- `EGP_OrbitalBuildingType` documented deprecated; retained as glue. New path does not depend on extending the enum.
- Settings cost/payload marked `DeprecatedProperty`; still the Hub config bridge.

## TEMP HUD
Layout of match HUD / resource bar / container / unit procurement unchanged. Building section: existing Logistics Hub row kept; extra native catalog rows (name, cost, READY, Purchase, Deploy when payload class resolves). Turret/Wall/Wall Turret Deploy stays disabled until a SpawnedClass exists.

## BuildGrid boundary
Not implemented: grid actor, snapping, occupancy, rotation footprints, clearance, wall drag/mount, FoW placement. Interim free placement preserved. `FootprintCells` is on BuildingDefinition for later.

## Deferred defensive building gameplay
No `AGP_DefensiveTurret` / `AGP_Wall` / `AGP_WallTurret` gameplay. Catalog identities + contract stubs only.

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

## GPEditor / UHT
`GPEditor Win64 Development` + UHT **PASS**. GP Development / Shipping not run.

## Exact changed files
See SHA-record / `git show --name-only` on the feature branch. Operator-local `DefaultEngine.ini`, `DefaultGame.ini`, maps, GrimProtocol Blueprint/Materials, VFX packs, `Tools/`, and AutoAcquire CRLF noise were **not** committed.

## Operator validation (do not self-approve)
1. Logistics Hub Purchase → READY → ghost → Deploy → DropPod → live Hub → +5 still works.
2. Building panel shows multiple catalog rows.
3. Purchasing one type changes only that type's READY.
4. Deployed Hub still uses authored/current payload via the settings bridge.

**NOT MERGED.**
