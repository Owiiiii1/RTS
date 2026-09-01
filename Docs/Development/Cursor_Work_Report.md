# Cursor Work Report

## Status

**BOTTOM_HUD_AUTHORED_BUILDING_CATALOG_FIX_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `335b201c1a14e97590f1f177beeba8decd9b6cd3`
- Previous catalog presentation implementation: `43dd8275cb27a97d09d5cd272521b3770037391f`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Exact root cause

`GetOperatorVisibleDrops()` was already returning the canonical authored Logistics Hub when that slot was ready. PurchaseBuildings was empty because `ClassifyBuildingDrop()` required `Drop->DropTags` to contain `GP.Drop.Type.Building` before assigning `OrdinaryBuilding`.

Native bootstrap `CreateNativeDrop` always stamps that drop tag. Authored `UGP_OrbitalDropDefinition` products do not: identity lives on the linked `UGP_BuildingDefinition.BuildingTags` (`GP.Building.Type.LogisticsHub`). The authored drop reached the catalog and was then classified `Skip`.

Catalog precedence, purchase authority, cost, READY, and deploy were not wrong. This was presentation classification only.

## Real authored-vs-native difference

| | Native bootstrap Logistics Hub | Authored Logistics Hub |
| --- | --- | --- |
| `BuildingTags` | `GP.Building.Type.LogisticsHub` | `GP.Building.Type.LogisticsHub` |
| `DropTags` | includes `GP.Drop.Type.Building` | typically **empty** / no `Drop.Type.Building` |
| Catalog | `GetOperatorVisibleDrops` native fallback | canonical authored drop when ready |
| Old presenter | shown (drop-tag test passed) | **skipped** (no drop tag) |
| New presenter | shown (`Building_Type_LogisticsHub`) | **shown** (same building tag) |

## New classification precedence

`BuildingDefinition.BuildingTags` is canonical for category assignment. `DropTags` remain acquisition/fallback metadata and must not hide a known Logistics Hub.

1. `Building_Type_MainBase` → Skip
2. `Drop_Type_WallPackage` → Skip
3. `Drop_Type_Wall` or `Building_Type_Wall` → Skip
4. `Building_Type_DefensiveTurret` → Defense (`DefensiveBuilding`)
5. `Building_Type_WallTurret` → Defense only if spawned class is already resolved; else Skip
6. `Building_Type_LogisticsHub` → Buildings (`OrdinaryBuilding`)
7. `Drop_Type_Building` → fallback OrdinaryBuilding when no more specific building identity applies

Specific BuildingDefinition identity wins over generic `Drop_Type_Building` (turret with that drop tag stays Defense).

Non-shipping skip diagnostic (rebuild only, no Tick): `GP PurchaseCatalog SkipBuilding Drop=... DropTags=... BuildingTags=...`

## Authored LogisticsHub test WITHOUT Drop_Type_Building

Inside `gp.UI.RunPurchaseCatalogPresentationContractTest`:

- Inject authored drop + `UGP_BuildingDefinition` tagged `Building_Type_LogisticsHub`
- `DropTags` intentionally empty (no `Drop_Type_Building`)
- Valid `AGP_LogisticsHub` spawned class
- Drive via `DebugAssignLoadedAuthoredLogisticsHub`

Asserted:

- A. `GetOperatorVisibleDrops` includes the authored drop
- B. PurchaseBuildings shows that authored row
- C. `ItemId` is authored, not native
- D. Cost from authored drop (`77`, not native `100`)
- E. DisplayName from linked BuildingDefinition
- F. Row does not require `Drop_Type_Building`

Lane regressions in the same block: authored DefensiveTurret **with** `Drop_Type_Building` stays Defense; Wall / MainBase / WallTurret stay excluded from Buildings; WallTurret still omitted without a resolved spawned class.

## Tests / build

`GPEditor Win64 Development` **Passed** (UHT included). No GP Dev/Shipping.

| Command | Result |
| --- | --- |
| `gp.UI.RunPurchaseCatalogPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunContextActionPresentationContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | Complete Failures=0 |
| `gp.Building.RunMultiBuildingDataContractTest` | Complete Failures=0 |

## Changed files (implementation commit)

- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPPurchaseCatalogPresentationContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified: `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `Content/`, authored DataAssets, `Config/`, maps, `GP.uproject`, `Tools/`. No building purchase authority / READY / cost / deploy / catalog precedence change. No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

## Operator note

Re-enter PurchaseBuildings after this head. Authored Logistics Hub should appear with authored `ItemId`, BuildingDefinition display name/icon, and authored Cost. Do not add `GP.Drop.Type.Building` to the DataAsset as a workaround.

INTERMEDIATE / NOT MERGE READY.
