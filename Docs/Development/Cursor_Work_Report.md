# Cursor Work Report

## Status

**BOTTOM_HUD_SELECTION_ICON_RANGE_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head: this implementation commit on `ui/gp-bottom-hud`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping` / full suite: **not run** (intermediate gate)

## Exact icon property

| Location | Name | Type |
| --- | --- | --- |
| `UGP_UnitDefinition` | `PresentationIcon` | `TObjectPtr<UTexture2D>` (`EditAnywhere`, `BlueprintReadOnly`, Category `GP\|Identity\|Presentation`) |
| `UGP_SelectionViewModel` (Single) | `Icon` | `TObjectPtr<UTexture2D>` FieldNotify |
| `FGP_SelectionGroupRow` | `Icon` | `TObjectPtr<UTexture2D>` |

Presentation metadata only. Gameplay does not read `PresentationIcon`. No class→texture map. No Worker/SalvageWalker/MainBase hardcode.

**Authored DataAssets were not modified.** Existing unit/building definitions therefore have `PresentationIcon = nullptr` until the operator assigns textures.

## AttackRange source

Single `AttackRange` is `UGP_UnitDefinition::AttackRangeCm` (cm, no conversion). No second attack-range field was added to gameplay definitions. If the definition is not resident, numeric fallback matches existing Damage/Armor/MoveSpeed (GAS attributes); icon stays null.

## Single fields

`DisplayName`, `CurrentHealth`, `MaxHealth`, `HealthNormalized`, `Damage`, `Armor`, `MoveSpeed`, **`AttackRange`**, **`Icon`**, `bIsUnit`, `bIsBuilding`, Worker cargo (`bHasCargo` / `CargoAmount` / `CargoCapacity` / `CargoNormalized`), `bIsInspectPresentation`.

Icon and AttackRange clear on None / Group / `ResetPresentation`.

## GroupRow fields

`Index`, **`Icon`**, `DisplayName`, `CurrentHealth`, `MaxHealth`, `HealthNormalized`, `bIsUnit`, `bIsBuilding`. No Damage/Armor/Speed/Range on rows. Equality includes `Icon`.

Each row icon is that actor's `ResolveLoadedUnitDefinition()->PresentationIcon`.

## Missing-icon fallback

Null icon is valid. WBP should show a placeholder. `ResolveLoadedUnitDefinition()` is already-resident only — **no `LoadSynchronous`** on the Selection VM / adapter / HUD path.

## Exact changed files

- `GP/Source/GPRuntime/Public/Units/GPUnitDefinition.h`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPSelectionViewModel.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPSelectionViewModel.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPSelectionViewModelAdapter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPSelectionViewModelAdapter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPSelectionViewModelContractTest.cpp`
- `Docs/TDD/10_Data_Assets.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`
- `Docs/Development/Cursor_Work_Report.md` (this file)

`WBP_GP_HUD` / `WBP_GP_SelectionGroupRow` / authored UnitDefinition DataAssets: **not modified**.

## Exact focused tests

`L_PrototypeArena` `-game -unattended -nop4 -NullRHI`. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunSelectionViewModelContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunProductionHUDFoundationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDBootstrapContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunContextActionPresentationContractTest` | **Complete Failures=0 Cancelled=false** |

## GPEditor / UHT

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** (UHT processed GPEditor, 9 generated files written) |

## Protected-file audit

**Not staged / not committed:** Config, maps, Blueprint (including `WBP_GP_HUD` / `WBP_GP_SelectionGroupRow`), DataAssets, Materials, VFX packs, Tools, `GP.uproject`.

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.

## Operator wiring (local WBP only — do not commit)

1. **`WBP_GP_SelectionGroupRow` Image** — bind/set from `FGP_SelectionGroupRow.Icon`. If null, keep the existing placeholder brush. Rebuild rows from `GetSelectionGroupRows()` on `BP_OnSelectionPresentationChanged`.
2. **`WBP_GP_HUD` Single icon** — Manual MVVM slot `GP_SelectionViewModel` field `Icon` (`UTexture2D`). Null → placeholder. Visible in Single mode only.
3. **Single AttackRange** — bind `GP_SelectionViewModel.AttackRange` (cm, same unit convention as MoveSpeed). No extra conversion. Hide or show `0` factually; do not invent a building exception.

Do not Tick. Do not load textures from the widget. Do not finalize after operator PASS; next slice remains PURCHASE categories.
