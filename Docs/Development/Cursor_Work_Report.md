# Cursor Work Report

## Status

**BOTTOM_HUD_PURCHASE_NAVIGATION_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `dffeba00b5b5c8ecdf2f22b5754e1f03da8b08bd`
- Previous runtime cursor / Patrol combat implementation: `c90dceb062ec320da8975a29dfe9a57709489ce7`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Enum states

`EGP_ContextActionPanelState` (Blueprint display names):

| Value | Display |
| --- | --- |
| `Actions` | Actions |
| `PurchaseRoot` | Purchase Root |
| `PurchaseUnits` | Purchase Units |
| `PurchaseBuildings` | Purchase Buildings |
| `PurchaseDefense` | Purchase Defense |

`EGP_PurchaseCategory`: `Units` / `Buildings` / `Defense`.

Same presenter: `UGP_ContextActionPresenter`. No second presenter. No Tick. No catalog/spend.

## Exact transitions

Friendly MainBase selected (`Mode == MainBase`):

```
Actions
  PURCHASE / RequestOpenMainBasePurchase
    ↓
PurchaseRoot
  Units     → PurchaseUnits      Back → PurchaseRoot
  Buildings → PurchaseBuildings  Back → PurchaseRoot
  Defense   → PurchaseDefense    Back → PurchaseRoot
  Back      → Actions
```

Category open is only valid from `PurchaseRoot`. No-op transitions do not broadcast.

## Blueprint API

`UGP_HUDRootWidget` (no Actor refs):

- `GetContextActionPanelState()` — unchanged
- `RequestOpenMainBasePurchase()` — unchanged
- `RequestOpenPurchaseCategory(EGP_PurchaseCategory Category)` — new
- `RequestPurchaseBack()` — new (context-sensitive)
- existing `BP_OnContextActionsChanged` on every real panel-state / presentation change

## Safety reset semantics

Purchase states exist only while `Mode == MainBase` (friendly selected MainBase).

Forced `Actions` when:

- selection changes away from that MainBase
- selection cleared
- MainBase died / destroyed (`IsActorBeingDestroyed` excluded from live selection)
- another unit/building selected
- enemy / inspect MainBase (`ResolveMode` is not MainBase; Purchase requests no-op)

## Tests

`gp.UI.RunContextActionPresentationContractTest` **Complete Failures=0**

Covers: friendly MainBase Actions → Root → Units/Buildings/Defense + Back; Root Back → Actions; category request without MainBase is no-op; leaving MainBase resets substate; enemy MainBase cannot enter Purchase; destroy while substate → Actions.

Regressions (Failures=0):

| Command | Result |
| --- | --- |
| `gp.UI.RunSelectionViewModelContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.Commands.RunMovePatrolTargetingContractTest` | Complete Failures=0 |
| `gp.Selection.RunMarqueeUnitsOnlyContractTest` | Complete Failures=0 |

Targeting actions and MainBase Purchase appearance are unchanged.

## GPEditor / UHT

`GPEditor Win64 Development` **Passed**.

## Changed files (implementation commit)

- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPContextActionPresentationContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified: `WBP_GP_HUD`, `WBP_GP_SelectionGroupRow`, `Content/`, `Config/`, maps, DataAssets, Materials, VFX, `Tools/`, `GP.uproject`. No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

## Operator wiring (WBP_GP_HUD, local, not committed)

Authored `WS_ActionPanel` already has index 0 `OV_Actions` and index 1 `OV_PurchaseRoot`.

Bind:

- `BTN_PurchaseUnits` → HUD `RequestOpenPurchaseCategory(Units)`
- `BTN_PurchaseBuildings` → `RequestOpenPurchaseCategory(Buildings)`
- `BTN_PurchaseDefense` → `RequestOpenPurchaseCategory(Defense)`
- `BTN_PurchaseBack` → `RequestPurchaseBack()`

Drive widget switcher from `GetContextActionPanelState()` on `BP_OnContextActionsChanged`. Later category overlays use the same Back API. No catalog rows yet.

INTERMEDIATE / NOT MERGE READY.
