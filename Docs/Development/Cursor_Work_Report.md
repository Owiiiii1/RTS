# Cursor Work Report

## Status

**BOTTOM_HUD_PURCHASE_ICON_ASYNC_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `0304a45d0f87a9f91cd0c0d04a674d9771c7072a`
- Previous Purchase execution: `0adab58ed826f2904c37385b230719f021da5d01`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Exact root cause

Purchase rows bind `FGP_PurchaseCatalogRow.Icon` from soft presentation textures:

- Units: `UGP_OrbitalUnitDropDefinition::Icon`
- Buildings / Defense buildings: `UGP_BuildingDefinition::Icon`
- Wall Package: `UGP_WallPackageDefinition::Icon`

The presenter only did `TSoftObjectPtr::Get()` or `ResolveObject()`. Authored icons are valid soft paths that are not resident when the catalog first rebuilds, so `RowData.Icon` stayed null. Selection icons work because `UGP_UnitDefinition::PresentationIcon` is an already-loaded `UTexture2D*`. WBP `SetBrushFromTexture` was wired; the native row data never became valid. No sync load is allowed.

## Async ownership

Presenter-owned lightweight StreamableManager requests (`UAssetManager::GetStreamableManager().RequestAsyncLoad`). Catalogs still own drop/definition readiness; they do not load HUD icons. DataAssets are not mutated.

## Request dedup

Requests are keyed by `FSoftObjectPath`. In-flight map + attempted set: one path is requested once even when Worker and Salvage Walker share it. Empty / null soft refs request nothing.

Handles are retained in `PendingPurchaseIconLoads` until completion, cancel, or Shutdown. Shutdown / BeginDestroy sets `bPurchaseIconLoadsAbandoned`, cancels handles, and clears caches so a late callback is a no-op.

## Rebuild callback

`HandlePurchaseIconLoaded` caches the loaded `UTexture2D`, then `RefreshPurchaseCatalogIfCategoryActive()` (category lists **and** `PurchaseBuildingSelected` / `PurchaseDefenseSelected`). That rebuilds rows / `GetSelectedPurchaseItem()` and broadcasts existing `OnContextActionsChanged`. No Tick. No `LoadSynchronous`. Inline Streamable completion during a rebuild is deferred via `bPurchaseIconRefreshDeferred` to avoid reentrancy.

Non-shipping hold/release seam (same idea as catalog definition hold tests): `DebugHoldPurchaseIconCompletion`, `DebugInjectHeldPurchaseIcon`, `DebugCompleteHeldPurchaseIconLoad`, `DebugCancelPurchaseIconLoads`.

## Unit / building / wall coverage

`ResolvePurchaseIcon` is used for unit drop rows, `MakeBuildingRow` (Logistics Hub / turret), Wall Package list rows, and selected-item rebuild for Building / DefensiveBuilding / WallPackage.

## Selected-item coverage

While `PurchaseBuildingSelected` or `PurchaseDefenseSelected`, completion rebuilds `SelectedPurchaseRow`. `GetSelectedPurchaseItem().Icon` becomes the loaded texture and the same UI change broadcast fires.

## Tests / build

`GPEditor Win64 Development` **Passed** (UHT included). No GP Dev/Shipping.

Headless `-game -nullrhi -unattended -nop4` `L_PrototypeArena`. No quit. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunPurchaseCatalogPresentationContractTest` | Complete Failures=0 (includes A unit / B building / C wall, dedup, empty-icon no request, selected-item, shutdown-safe cancel) |
| `gp.UI.RunPurchaseExecutionContractTest` | Complete Failures=0 |
| `gp.UI.RunSelectionViewModelContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Resource.RunOrbitalUnitDropContractTest` | Complete Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | Complete Failures=0 |
| `gp.Orbital.RunWallPackageInventoryContractTest` | Complete Failures=0 |

## Changed files (implementation commit)

- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPPurchaseCatalogPresentationContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified: purchase authority/RPCs, manifest rules, READY, placement, costs, category classification, authored DataAssets, `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `Content/`, `Config/`, maps, `GP.uproject`, `Tools/`. No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

## Operator note

Re-enter PurchaseUnits / Buildings / Defense after this head. Authored row icons should appear after async load without WBP changes. INTERMEDIATE / NOT MERGE READY.
