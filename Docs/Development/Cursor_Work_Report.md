# Cursor Work Report

## Status

**PRODUCTION_HUD_SELECTION_VM_READY_FOR_OPERATOR_VALIDATION**

Native Selection ViewModel + push adapter are on `ui/gp-production-hud-selection-vm`. Authored `WBP_GP_HUD` Selection/Info bindings remain operator work. This slice does not complete the visual Selection HUD.

## Branch / base / head

- Branch: `ui/gp-production-hud-selection-vm`
- Authoritative base: `origin/main` @ `e1af0c6e5fcf39f70c58169f32c457aa2850da8d`
- Implementation commit: `832b17ca8405f4b863e92076ef854df1c83ab63e`
- Report commit: this file's commit on `ui/gp-production-hud-selection-vm`
- Gameplay selection cap: **24** (unchanged)

## Factual selection data-flow

```
UGP_SelectionComponent (local PC)
  OnSelectionChanged()
        │
        ▼
UGP_SelectionViewModelAdapter   (GPUIRuntime, owned by UGP_HUDViewModelSubsystem)
  live selected = valid && !IsDead()
  else inspect fallback = Cast<AGP_UnitBase>(GetInspectedTarget())
                         && valid && !IsDead() && IsGameplayInspectable()
  bind GAS Health/MaxHealth + OnUnitDied + OnDestroyed
  bind Worker UGP_CargoComponent::OnCargoAmountChanged (single only)
        │
        ▼
UGP_SelectionViewModel          (canonical presentation SoT)
        │
        ├─ Manual MVVM slot `GP_SelectionViewModel` (fail-safe if missing)
        └─ HUD-root group seam: GetSelectionGroupRows()
           + BP_OnSelectionPresentationChanged()
        │
        ▼
authored WBP_GP_HUD             (not modified in this slice)
```

Gameplay state stays in `GPRuntime`. Presentation lives in `GPUIRuntime`. `GPRuntime` has no `GPUIRuntime` dependency.

No Tick. No polling. No world scan on the presentation path. The contract test uses `TActorIterator` only to neutralize authored combat actors, matching other diagnostic contracts.

## VM ownership

`UGP_HUDViewModelSubsystem` (local-player subsystem) creates:

- exactly one `UGP_SelectionViewModel` (outer = subsystem)
- exactly one `UGP_SelectionViewModelAdapter`

Widgets do not create VMs. `FGP_HUDViewModelBridge::AssignOwnedViewModels` injects the existing instance into Manual slot `GP_SelectionViewModel`. Missing slot / missing `UMVVMView` fails safe and does not spawn a replacement VM.

Bind path:

- `PlayerControllerChanged` / `Rebind` → `BindPlayerController` → `BindSelectionAdapter`
- Selection bind does **not** wait for GameState/team ready
- PC change / travel: previous adapter `Shutdown` (unbind) then `Initialize` on the new local `UGP_SelectionComponent`
- `Deinitialize`: adapter `Shutdown`

## Single / Group / Inspect semantics

Mode enum: `EGP_SelectionPresentationMode { None, Single, Group }`.

Priority (matches current component, not a new gameplay rule):

1. **Selected live units win.** `GetSelectedUnits()` filtered to `IsValid && !IsDead()`.
   - 0 live selected → inspect fallback
   - 1 live selected → **Single** (`bIsInspectPresentation = false`)
   - >1 live selected → **Group** (factual count, cap 24)
2. **Inspect fallback.** If selection is empty and `GetInspectedTarget()` is a live inspectable `AGP_UnitBase` (`IsGameplayInspectable()`), present **Single** with `bIsInspectPresentation = true`.
3. **None.** Otherwise count 0, single fields cleared, group rows empty.

`ClearSelection()` does not clear inspect. `SetInspectedTarget()` notifies `OnSelectionChanged()`. `ReplaceSelectionWithUnit()` does not clear inspect; selection still wins presentation.

Dead selected actors are omitted from presentation only. The adapter does not mutate `UGP_SelectionComponent`. `PruneInvalidEntries()` is unused by gameplay; death/destroy rebuilds presentation via `OnUnitDied` / `OnDestroyed`.

## Health binding / unbinding

On every selection/inspect rebuild:

1. Unbind Health, MaxHealth, `OnUnitDied`, `OnDestroyed`, cargo from previously presented actors.
2. Rebuild VM from current live selected / inspect.
3. Bind Health + MaxHealth `GetGameplayAttributeValueChangeDelegate` on **currently presented** actors only.
4. Health/MaxHealth push updates vitals only (`SetSingleVitals` / `SetGroupRowVitals`) — no selection rebuild, no Tick.

Current health is GAS `UGP_UnitAttributeSet::GetHealth()`, not `UGP_UnitDefinition` initial health.

On death/destroy of a presented actor: rebuild presentation (unbind + omit dead). No gameplay selection mutation.

After clear / replacement, stale actor health no longer mutates the VM.

## Definition fallback

Source: `AGP_UnitBase::ResolveLoadedUnitDefinition()` only. No `LoadSynchronous` from HUD.

There is **no** definition-ready multicast. The adapter snapshots at bind/rebuild. No polling if the soft ref is still pending.

If definition is resident: `DisplayName`, `Damage`, `Armor`, `MoveSpeedCmPerSecond`, and MaxHealth fallback from `UGP_UnitDefinition`.

If not resident: empty `DisplayName`; `Damage` / `Armor` / `MoveSpeed` from GAS if present; MaxHealth from GAS, else 0. No crash.

Contract spawn of native `AGP_Worker` hit the not-resident path (`A_DefinitionNotResident_SafeFallback` PASS). Runtime health still came from GAS.

## Cargo decision

**Included in this slice (event-driven).**

`AGP_Worker::GetCargoComponent()` + `UGP_CargoComponent::OnCargoAmountChanged` (dynamic, four floats). `AddCargo` on authority broadcasts the delegate. No cargo polling.

Single Worker presentation exposes `bHasCargo`, `CargoAmount`, `CargoCapacity`, `CargoNormalized`. `bHasCargo` is true when capacity > 0 (cargo UI present), not “currently carrying > 0”.

Group mode does not project cargo. A separate `UGP_CargoVM` type was not added.

## Group-array Blueprint exposure

Canonical SoT: `UGP_SelectionViewModel::GroupRows` (`FieldNotify`) plus `OnSelectionPresentationChanged`.

Blueprint list seam (Launch Menu pattern, one SoT):

- `UGP_HUDRootWidget::GetSelectionGroupRows()`
- `BP_OnSelectionPresentationChanged`

Scalar single fields are MVVM FieldNotify. Dynamic group rows are intended to refresh from the HUD-root accessor after the BP event. No second array owner.

Row fields: `Index`, `DisplayName`, `CurrentHealth`, `MaxHealth`, `HealthNormalized`, `bIsUnit`, `bIsBuilding`. Widgets do not receive Actors.

No icon field. `UGP_UnitDefinition` has no icon. DataAssets were not modified.

## Exact changed files

New:

- `GP/Source/GPUIRuntime/Public/ViewModels/GPSelectionViewModel.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPSelectionViewModel.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPSelectionViewModelAdapter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPSelectionViewModelAdapter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPSelectionViewModelContractTest.cpp`

Modified:

- `GP/Source/GPUIRuntime/Public/ViewModels/GPHUDViewModelSubsystem.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDViewModelBridge.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDViewModelBridge.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDViewModelBridgeContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md` (this file)

## Exact contracts / results

`L_PrototypeArena` `-game -unattended -NullRHI`.

| Command | Result |
| --- | --- |
| `gp.UI.RunSelectionViewModelContractTest` | **Complete Failures=0 Cancelled=false** (A–H including health/cargo/group/inspect) |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunProductionHUDFoundationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDBootstrapContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunLaunchMenuPresentationContractTest` | **Complete Failures=0 Cancelled=false** |
| Existing Selection **component** contract | **None in repo** — no `gp.Selection.*` / SelectionComponent contract to re-run |

`GP Win64 Development` / `GP Win64 Shipping` / full suite: **not run** (pre-operator gate).

## GPEditor / UHT

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** (first compile ran UHT after new UObject files; link succeeded after adding `GPSelectionComponent.h` include for `IsValid`) |

## Protected-file audit

**Not staged / not committed:**

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/Content/Basic_VFX/`
- `GP/Content/GrimProtocol/Blueprint/`
- `GP/Content/GrimProtocol/Materials/`
- `GP/Content/Mixed_Magic_VFX_Pack/`
- `GP/Content/RocketThrusterExhaustFX/`
- `Tools/`
- `WBP_GP_HUD` / `WBP_GP_LaunchContainerRow`
- authored unit/building Blueprints/DataAssets
- `GP/GP.uproject`

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.

## Operator work in `WBP_GP_HUD`

1. Add Manual ViewModel slot named exactly **`GP_SelectionViewModel`** (`UGP_SelectionViewModel`). Resource/Match slot names stay unchanged.
2. Bind Single scalars: Mode, SelectionCount, DisplayName, CurrentHealth, MaxHealth, HealthNormalized, Damage, Armor, MoveSpeed, bIsUnit, bIsBuilding, cargo fields, bIsInspectPresentation as needed.
3. On `BP_OnSelectionPresentationChanged`, rebuild the bottom-center icon grid from `GetSelectionGroupRows()` (stable `Index` 0..N-1, N ≤ 24). Layout may reserve 30 cells; extra cells stay empty.
4. Place placeholder images in WBP. Do **not** add icon fields to gameplay DataAssets in this slice.
5. Do not Tick, do not read Actors/ASC from the widget, do not create VMs in the widget.

Native foundation only. Authored visual HUD is still operator-local and uncommitted.
