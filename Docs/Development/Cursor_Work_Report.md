# Cursor Work Report

## Status

**BOTTOM_HUD_PURCHASE_CATALOG_PRESENTATION_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `43dd8275cb27a97d09d5cd272521b3770037391f`
- Previous Purchase navigation implementation: `dffeba00b5b5c8ecdf2f22b5754e1f03da8b08bd`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Exact SoT per category

No new gameplay catalog. Presenter reads existing products only.

### UNITS — `UGP_OrbitalUnitDropCatalog`

Canonical ready products:

- `GetWorkerDrop()` → `UGP_OrbitalUnitDropDefinition` (`DisplayName`, already-loaded `Icon`, `Cost`, `TransportSlotCost`)
- `GetSalvageWalkerDrop()` → same

Pending authored slot: `Get*Drop()` is null (no native identity kept visible). Row is **omitted**. No `LoadSynchronous`.

### BUILDINGS — `UGP_BuildingDropCatalog::GetOperatorVisibleDrops()` then tag filter

- Ordinary orbital buildings with `GP.Drop.Type.Building` that are not defense/wall/MainBase.
- Logistics Hub **is shown** (`GP.Building.Type.LogisticsHub`).
- MainBase **not shown** (`GP.Building.Type.MainBase`).
- Wall Package **not shown** (separate catalog).
- Defensive Turret **not shown** here (`GP.Building.Type.DefensiveTurret` → Defense).
- Wall segment drop (`GP.Drop.Type.Wall` / `GP.Building.Type.Wall`) **not shown**.
- Cost: `UGP_OrbitalDropDefinition.Cost` (same SoT as `GetPurchaseCost` when ready).
- DisplayName/Icon: already-loaded linked `UGP_BuildingDefinition`.

Pending authored building slot: `GetOperatorVisibleDrops` keeps **native identity** visible; purchase still rejects `DefinitionNotReady`. Presenter shows that native row **disabled** with `DefinitionNotReady`.

### DEFENSE

- Defensive Turret from building drop catalog (`GP.Building.Type.DefensiveTurret`).
- Wall Package from `UGP_WallPackageCatalog::GetWallPackage()` / `UGP_WallPackageDefinition` (`PrimaryAssetId`, `Cost`, `SegmentCount`, already-loaded `Icon`).
- Wall-mounted Turret (`GP.Building.Type.WallTurret`): shown **only** if `ResolveLoadedSpawnedClass()` is already resolved. Native bootstrap has no spawned class → **omitted**. Not a fictitious row.
- Foundation Slab: not in any current catalog → not added.
- Logistics Hub: not in Defense.

## Exact rows actually exposed (native isolation / native bootstrap)

With authored slots empty/failed (native products):

| Panel | Rows |
| --- | --- |
| Actions / PurchaseRoot / non-MainBase | empty |
| PurchaseUnits | Worker (`DA_GP_OrbitalUnitDrop_Worker`), Salvage Walker (`DA_GP_OrbitalUnitDrop_SalvageWalker`) |
| PurchaseBuildings | Logistics Hub (`DA_GP_OrbitalDrop_LogisticsHub`) |
| PurchaseDefense | Defensive Turret (`DA_GP_OrbitalDrop_DefensiveTurret`), Wall Package (`DA_GP_WallPackage`) |

Authored-ready products replace native identity via existing catalog precedence; `ItemId` is that product’s `FPrimaryAssetId`.

## Classification logic

Never class name / DisplayName.

Gameplay tags (`FGPGameplayTags`):

1. `Building_Type_MainBase` → skip
2. `Drop_Type_WallPackage` on an orbital drop → skip (Wall Package comes from wall catalog)
3. `Drop_Type_Wall` or `Building_Type_Wall` → skip (segment, not package)
4. `Building_Type_DefensiveTurret` → Defense / `DefensiveBuilding`
5. `Building_Type_WallTurret` → Defense / `DefensiveBuilding` **only if spawned class already loaded**
6. `Drop_Type_Building` → Buildings / `Building` (includes Logistics Hub via `Building_Type_LogisticsHub`)

Units are not classified from the building catalog; they come only from `GetWorkerDrop` / `GetSalvageWalkerDrop`.

## Icon resolution

No sync load. `TSoftObjectPtr<UTexture2D>::Get()` then `ToSoftObjectPath().ResolveObject()`. Null is valid.

- Units: `UGP_OrbitalUnitDropDefinition.Icon`
- Buildings / turret: linked loaded `UGP_BuildingDefinition.Icon`
- Wall Package: `UGP_WallPackageDefinition.Icon`

## Price resolution

All OrbitalFerronite. Presenter does **not** duplicate catalog constants.

- Unit: `DropDefinition.Cost` (equals catalog `GetWorkerOrbitalDropCost` / `GetSalvageWalkerOrbitalDropCost` when that product is canonical)
- Building / turret: `OrbitalDropDefinition.Cost` (equals `GetPurchaseCost` when not pending)
- Wall Package: `WallPackageDefinition.Cost`

Pending building native identity still displays native `Cost`, but the row is disabled `DefinitionNotReady` (`GetPurchaseCost` is 0 while pending; UI does not use that 0 as the displayed price).

## Availability logic (presentation only; authority remains final)

All:

- friendly MainBase selected + matching purchase category panel
- product definition ready
- Cost finite and ≥ 0
- `OrbitalFerronite >= Cost` or disabled `Insufficient Orbital Ferronite`

Unit: ready + affordable. `TransportSlotCost` is metadata only. No shuttle-manifest validation.

Building: ready/valid (`!IsDropDefinitionPending` and loaded `BuildingDefinition`) + affordability.

Wall Package:

- product ready (`GetWallPackage()` non-null; pending → omit, same as unit catalog)
- `UGP_WallSegmentInventoryComponent` on selected MainBase:
  - pending delivery → disabled `Delivery already pending`
  - stock at capacity → disabled `Wall stock full`
- affordability

## Event refresh seams

No Tick. Rows rebuild with existing `OnContextActionsChanged` / `BP_OnContextActionsChanged`.

- panel/category change (`SetPanelState` rebuilds rows then broadcasts)
- selection / MainBase / death / destroy (`RebuildPresentation`)
- `ASC->GetGameplayAttributeValueChangeDelegate(OrbitalFerronite)`
- selected MainBase `OnWallInventoryChanged` / `OnWallPackagePendingChanged`

Catalogs have **no** public readiness delegate. No world polling. Pending→ready uses existing rebuild seams (re-enter category / selection). Tests re-open the category after debug pending injection.

## Blueprint API

`UGP_HUDRootWidget` (no Actor* / UObject* in the row except already-loaded `UTexture2D*`):

- `GetPurchaseCatalogRows()` — current `PanelState` only (`PurchaseUnits` / `PurchaseBuildings` / `PurchaseDefense`). Actions and PurchaseRoot → empty array.
- existing `BP_OnContextActionsChanged` — no second Blueprint event

`FGP_PurchaseCatalogRow`: `ItemId` (`FPrimaryAssetId`), `ItemKind`, `Category`, `DisplayName`, `Icon`, `Cost`, `bVisible`, `bEnabled`, `DisabledReason`, `TransportSlotCost`, `SegmentCount`.

`EGP_PurchaseCatalogItemKind`: Unit / Building / DefensiveBuilding / WallPackage.

No `RequestPurchaseCatalogItem`. No spend / RPC / manifest / LAUNCH / READY / placement / wall buy.

## Tests

`gp.UI.RunPurchaseCatalogPresentationContractTest` **Complete Failures=0**

A–J as specified: Worker + Salvage Walker names/cost/slots from catalog; Buildings LogisticsHub present, turret/MainBase/WallPackage absent; Defense turret + Wall Package present, LogisticsHub absent, Wall Turret omitted; prices from definitions; null icon accepted; insufficient/sufficient Orbital; empty on non-MainBase / Actions / PurchaseRoot; category navigation changes the row set; pending Worker omitted without sync load; pending Hub native identity disabled `DefinitionNotReady`.

Regressions (Failures=0):

| Command | Result |
| --- | --- |
| `gp.UI.RunContextActionPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunSelectionViewModelContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Commands.RunMovePatrolTargetingContractTest` | Complete Failures=0 |
| `gp.Resource.RunOrbitalUnitDropContractTest` | Complete Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | Complete Failures=0 |
| `gp.Orbital.RunWallPackageInventoryContractTest` | Complete Failures=0 |

## GPEditor / UHT

`GPEditor Win64 Development` **Passed** (UHT included). No GP Dev/Shipping.

## Changed files (implementation commit)

- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPPurchaseCatalogPresentationContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPContextActionPresentationContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified: `WBP_GP_HUD`, `WBP_GP_SelectionGroupRow`, `Content/`, `Config/`, maps, DataAssets, Materials, VFX, `Tools/`, `GP.uproject`. No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

## Operator wiring (WBP_GP_HUD, local, not committed)

Cursor does not create a row uasset. On `BP_OnContextActionsChanged`:

1. Read `GetContextActionPanelState()`.
2. If `PurchaseUnits` / `PurchaseBuildings` / `PurchaseDefense`, call `GetPurchaseCatalogRows()`.
3. For each row bind DisplayName, Icon (null-safe), Cost, Enabled, DisabledReason, TransportSlotCost / SegmentCount.
4. Actions / PurchaseRoot: hide catalog list (getter is empty).

Do not call purchase/spend APIs. Click-to-buy is the next checkpoint.

INTERMEDIATE / NOT MERGE READY.
