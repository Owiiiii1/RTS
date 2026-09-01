# Cursor Work Report

## Status

**BOTTOM_HUD_UNIT_PURCHASE_ICON_FALLBACK_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `c06743d613c6fea71f5d492f7acdfd16fbae22e5`
- Previous Purchase icon async: `0304a45d0f87a9f91cd0c0d04a674d9771c7072a`
- Previous Purchase execution: `0adab58ed826f2904c37385b230719f021da5d01`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Icon precedence

Unit purchase rows (`PurchaseUnits`) resolve via `UGP_ContextActionPresenter::ResolveUnitPurchaseIcon(Drop)`:

1. Loaded / cached `UGP_OrbitalUnitDropDefinition::Icon` — optional purchase-specific override.
2. Else `Drop->ResolveLoadedUnitDefinition()->PresentationIcon` — canonical generic HUD icon.
3. Else `nullptr`.

`UGP_UnitDefinition::PresentationIcon` is the canonical generic HUD icon (selection and unit purchase fallback). `UGP_OrbitalUnitDropDefinition::Icon` is an optional acquisition override only. No duplicate required authoring. `PresentationIcon` is not copied onto the drop. Authored DataAssets were not modified.

Building / Wall Package rows still use generic `ResolvePurchaseIcon` on definition soft `Icon`. No unit semantics were pushed into that helper.

## Immediate fallback while async override pending

Existing presenter StreamableManager async icon loading remains.

If `Drop->Icon` is non-null but unresolved:

- request the async override (deduped by `FSoftObjectPath`)
- show `PresentationIcon` immediately so the row is not blank
- on completion, rebuild the catalog row and replace the fallback with the override

No Tick. No `LoadSynchronous`. Empty drop `Icon` requests nothing.

## Native / authored coverage

Fallback uses factual loaded `UnitDefinition` only (no sync load).

- Native Worker / Salvage Walker drops (empty `Icon`, catalog `UnitDefinition`) use `PresentationIcon`.
- Authored Worker / Salvage Walker drops with loaded `UnitDefinition` use the same fallback.
- Loaded drop override still wins over `PresentationIcon`.
- Both empty → null icon.

## Tests / build

`GPEditor Win64 Development` **Passed** (UHT included). No GP Dev/Shipping.

Headless `-game -nullrhi -unattended -nop4` `L_PrototypeArena`. No quit. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunPurchaseCatalogPresentationContractTest` | Complete Failures=0 (A empty-drop→PresentationIcon; B loaded override wins; C pending override keeps PresentationIcon then replaces; D both empty→null; E native Worker/Walker fallback; F building/wall async unchanged) |
| `gp.UI.RunPurchaseExecutionContractTest` | Complete Failures=0 |
| `gp.UI.RunSelectionViewModelContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Resource.RunOrbitalUnitDropContractTest` | Complete Failures=0 |

## Changed files (implementation commit)

- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPPurchaseCatalogPresentationContractTest.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinition.h` (comment only)
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropDefinition.h` (comment only)
- `Docs/TDD/10_Data_Assets.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified: purchase authority/RPCs, manifest rules, READY, placement, costs, category classification, authored DataAssets, `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `WBP_GP_SelectionGroupRow`, `Content/`, `Config/`, maps, Materials, VFX, `GP.uproject`, `Tools/`. No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

INTERMEDIATE / NOT MERGE READY.
