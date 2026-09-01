# Cursor Work Report

## Status

**BOTTOM_HUD_PURCHASE_READINESS_BUILDING_ICON_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `0aa852246d172f63ba995147dd721def624fe6cf`
- Previous unit purchase icon fallback: `c06743d613c6fea71f5d492f7acdfd16fbae22e5`
- Previous report tip: `cf5de49024071f9b9079198a85d09d6bb4ad64da`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Exact first-open root cause

`UGP_OrbitalUnitDropCatalog` getters return null while an authored Worker/Walker slot is Pending (nested UnitDefinition / PayloadClass included). `RebuildPurchaseCatalogRows()` omits null products. Opening PurchaseUnits can start/observe async readiness, but the presenter had no event when Pending → Ready/Failed. A second category entry rebuilt after the load completed, so rows appeared only then. Native bootstrap is still not shown while authored is Pending.

## Catalog readiness delegate

`UGP_OrbitalUnitDropCatalog::OnCatalogChanged` broadcasts when canonical Worker/Walker availability materially changes (authored top-level/nested complete, Failed → native fallback, Empty). Duplicate broadcasts are suppressed when canonical pointers and pending flags are unchanged. Failed slots no longer retry the same authored path on every `Get()` refresh (that would re-enter Pending and hide native fallback). Catalog shutdown clears the multicast. No Tick. No polling. No sync load.

## Presenter bind / unbind

`UGP_ContextActionPresenter` binds once in `Initialize` via `AddUObject`. Unbind on `Shutdown` / `BeginDestroy` using `TryGetExisting()` so a destroyed singleton is safe. Rebuilds do not rebind. On catalog change while `PanelState == PurchaseUnits`: `RebuildPurchaseCatalogRows()` + existing `OnContextActionsChanged`. First open: empty while genuinely Pending → async completion → rows appear without leaving the category.

## First-open regression

`gp.UI.RunPurchaseCatalogPresentationContractTest` now covers:

- first PurchaseUnits entry while Worker/Walker Pending omits rows
- complete readiness while remaining in PurchaseUnits → rows appear (broadcast observed)
- Pending → Failed → native Worker row appears without category re-entry
- no Tick; no duplicate catalog bind on repeated Initialize / category churn
- isolated presenter Shutdown unbind is idempotent and ignores later catalog events

## Building icon precedence

`ResolveBuildingPurchaseIcon(BuildingDefinition)`:

1. Loaded/cached `UGP_BuildingDefinition::Icon` (optional purchase override)
2. If override exists but unresolved: request existing async purchase-icon load, then fall through
3. Already-loaded `BuildingDefinition->ResolveLoadedUnitDefinition()->PresentationIcon`
4. Else nullptr

Used for Buildings list, Defense building list (Logistics Hub / Defensive Turret / Wall Turret if operator-visible), and `SelectedPurchaseRow`. Wall Package still uses only package soft `Icon` (no UnitDefinition fallback). Building catalog canonical-ready does not require UnitDefinition; fallback uses factual `ResolveLoadedUnitDefinition()` only (no UI-owned gameplay-definition load, no DataAsset mutation).

## Selected row coverage

Selected building/defense items use the same `ResolveBuildingPurchaseIcon` helper. Pending override shows `PresentationIcon` immediately; completion rebuilds the selected row to the override.

## Tests / build

`GPEditor Win64 Development` **Passed** (UHT included). No GP Dev/Shipping.

Headless `-game -nullrhi -unattended -nop4` `L_PrototypeArena`. No quit. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunPurchaseCatalogPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunPurchaseExecutionContractTest` | Complete Failures=0 |
| `gp.UI.RunContextActionPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Resource.RunOrbitalUnitDropContractTest` | Complete Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | Complete Failures=0 |
| `gp.UI.RunSelectionViewModelContractTest` | Complete Failures=0 |

## Changed files (implementation commit)

- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropCatalog.h`
- `GP/Source/GPRuntime/Private/Orbital/GPOrbitalUnitDropCatalog.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h` (comment only)
- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPPurchaseCatalogPresentationContractTest.cpp`
- `Docs/TDD/10_Data_Assets.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified: purchase authority/RPCs, manifest rules, READY, placement, costs, category classification, authored DataAssets, `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `WBP_GP_SelectionGroupRow`, `Content/`, `Config/`, maps, Materials, VFX, `GP.uproject`, `Tools/`. No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

INTERMEDIATE / NOT MERGE READY.
