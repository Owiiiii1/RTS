# Cursor Work Report

## Status

**BOTTOM_HUD_GROUP_ROW_CLICK_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head: `8bb55d5a97db7c844a324c5d87cda68db1b7380e`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping` / full suite: **not run** (intermediate gate)

## Exact Blueprint API

```
UFUNCTION(BlueprintCallable, Category="GP|HUD|Selection")
void UGP_HUDRootWidget::RequestSelectGroupRow(int32 RowIndex);
```

Local HUD only. Resolves `GetOwningPlayer()` → `AGP_PlayerController` → `UGP_SelectionComponent`. No RPC. Selection stays local-only.

Does **not** write SelectionVM. Does **not** force WidgetSwitcher / presentation mode. Refresh is `OnSelectionChanged` → `UGP_SelectionViewModelAdapter` → `BP_OnSelectionPresentationChanged`.

## How RowIndex maps to the selected actor

`FGP_SelectionGroupRow.Index` is the compact live index `0..N-1` in `GetSelectedUnits()` order after skipping invalid, `IsActorBeingDestroyed()`, and dead entries. That is the same filter `UGP_SelectionViewModelAdapter::CollectLiveSelectedUnits` / `FillGroupRow` uses when building rows.

`RequestSelectGroupRow(RowIndex)` walks that same compact live list and resolves the actor shown on that row. No persistent ids. Click is a no-op when:

- live count `< 2` (empty / single / not a group)
- `RowIndex` out of bounds
- the resolved unit is invalid, being destroyed, or not `IsGameplaySelectable()`

## Canonical mutation

Uses existing `UGP_SelectionComponent::ReplaceSelectionWithUnit(AGP_UnitBase*)`. No second selection-mutation path.

## No Actor* on GroupRow

`FGP_SelectionGroupRow` is unchanged: `Index`, `DisplayName`, `Icon`, health fields, `bIsUnit`, `bIsBuilding`. No `AActor*` / `AGP_UnitBase*`. HUD does not own gameplay actor refs; `Index` is presentation identity for the click request.

## Exact focused tests

`L_PrototypeArena` `-game -unattended -nop4 -NullRHI`. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunSelectionViewModelContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunProductionHUDFoundationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDBootstrapContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunContextActionPresentationContractTest` | **Complete Failures=0 Cancelled=false** |

SelectionVM click cases (all PASS):

| Case | Result |
| --- | --- |
| A. Group A,B,C → `RequestSelectGroupRow(1)` → count 1, selected B, Mode Single, name/icon/health from B | PASS |
| B. OOB `-1` / `99` → selection unchanged | PASS |
| C. Destroyed/stale row → safe no-op, no invalid dereference | PASS |
| D. Single / empty → `RequestSelectGroupRow(0)` does not invent selection | PASS |

## GPEditor / UHT

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** (UHT processed GPEditor, 4 generated files written for `RequestSelectGroupRow`) |

## Protected-file audit

**Not staged / not committed:** Config, maps, Blueprint (including `WBP_GP_HUD` / `WBP_GP_SelectionGroupRow`), DataAssets, Materials, VFX packs, Tools, `GP.uproject`.

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.

## Exact WBP operator wiring (local WBP only — do not commit)

`WBP_GP_SelectionGroupRow` click → parent HUD `RequestSelectGroupRow(RowData.Index)`

Do not Tick. Do not store actor refs on the row widget. Do not set SelectionVM or WidgetSwitcher from the row. After operator PASS, do not finalize; next slice remains PURCHASE categories.
