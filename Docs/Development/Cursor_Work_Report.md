# Cursor Work Report

## Status

**PRODUCTION_HUD_SELECTION_VM_FINALIZED_READY_TO_MERGE**

Operator PASS received. Tech-lead pre-finalization SHAs verified. Gameplay/UI semantics were not changed during finalization. Authored `WBP_GP_HUD` was not modified and is not in the commit.

## Operator PASS

Operator confirmed in PIE (2026-08-25):

- `GP_SelectionViewModel` is successfully connected in authored `WBP_GP_HUD` via Manual MVVM slot `GP_SelectionViewModel`
- selecting Worker / Salvage Walker / MainBase updates Selection data
- CurrentHealth / Damage display
- Worker CargoAmount updates live during mining
- authored `WBP_GP_HUD` remains operator-local and is **not** committed

Group visual (icon grid from `GetSelectionGroupRows` / `BP_OnSelectionPresentationChanged`) is **not** authored and is **not** operator-validated.

## Branch / base / head

- Branch: `ui/gp-production-hud-selection-vm`
- Base: `origin/main` @ `e1af0c6e5fcf39f70c58169f32c457aa2850da8d`
- Implementation commit: `832b17ca8405f4b863e92076ef854df1c83ab63e`
- Pre-finalization head (tech-lead check): `26b4da12856595b4702eca6cdb574808e138485f`
- Finalization head: this finalization commit
- Ahead of `origin/main`: implementation + report SHA record + this finalization
- Behind `origin/main`: **0**

## Final SelectionVM data flow

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
authored WBP_GP_HUD             (operator-local; Single bindings PIE-validated; not committed)
```

Gameplay state stays in `GPRuntime`. Presentation lives in `GPUIRuntime`. `GPRuntime` has no `GPUIRuntime` dependency.

No Tick. No polling. No world scan on the presentation path. The contract test uses `TActorIterator` only to neutralize authored combat actors, matching other diagnostic contracts.

`UGP_HUDViewModelSubsystem` (local-player subsystem) owns exactly one `UGP_SelectionViewModel` and exactly one `UGP_SelectionViewModelAdapter`. Widgets do not create VMs. `FGP_HUDViewModelBridge::AssignOwnedViewModels` injects the existing instance into Manual slot `GP_SelectionViewModel`. Missing slot / missing `UMVVMView` fails safe and does not spawn a replacement VM.

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
   - >1 live selected → **Group** (factual count, cap **24** unchanged)
2. **Inspect fallback.** If selection is empty and `GetInspectedTarget()` is a live inspectable `AGP_UnitBase` (`IsGameplayInspectable()`), present **Single** with `bIsInspectPresentation = true`.
3. **None.** Otherwise count 0, single fields cleared, group rows empty.

`ClearSelection()` does not clear inspect. `SetInspectedTarget()` notifies `OnSelectionChanged()`. `ReplaceSelectionWithUnit()` does not clear inspect; selection still wins presentation.

Dead selected actors are omitted from presentation only. The adapter does not mutate `UGP_SelectionComponent`. Death/destroy rebuilds presentation via `OnUnitDied` / `OnDestroyed`.

## Health / cargo push confirmation

On every selection/inspect rebuild:

1. Unbind Health, MaxHealth, `OnUnitDied`, `OnDestroyed`, cargo from previously presented actors.
2. Rebuild VM from current live selected / inspect.
3. Bind Health + MaxHealth `GetGameplayAttributeValueChangeDelegate` on **currently presented** actors only.
4. Health/MaxHealth push updates vitals only (`SetSingleVitals` / `SetGroupRowVitals`) — no selection rebuild, no Tick.

Current health is GAS `UGP_UnitAttributeSet::GetHealth()`, not `UGP_UnitDefinition` initial health.

Cargo is event-driven: `AGP_Worker::GetCargoComponent()` + `UGP_CargoComponent::OnCargoAmountChanged`. No cargo polling. Single Worker presentation exposes `bHasCargo`, `CargoAmount`, `CargoCapacity`, `CargoNormalized`. Group mode does not project cargo.

Operator PIE confirmed CurrentHealth display and live Worker CargoAmount while mining.

Definition source: `AGP_UnitBase::ResolveLoadedUnitDefinition()` only. No `LoadSynchronous` from HUD. No icon field (`UGP_UnitDefinition` has none).

## Manual MVVM slot `GP_SelectionViewModel`

Authored `WBP_GP_HUD` uses Manual slot name **`GP_SelectionViewModel`**. Native injects the subsystem-owned VM into that slot. Operator PIE **PASSED** for Single Selection bindings. Group visual remains unauthored.

## Exact changed files (branch vs `origin/main`)

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

`WBP_GP_HUD` is **not** in the branch diff and is **not** in this commit.

## Exact contract results

Focused re-run after operator PASS. `L_PrototypeArena` `-game -unattended -nop4 -NullRHI`. Log: `Saved/ContractLogs/gp.UI.SelectionVMFinalizationSuite.log`.

| Command | Result |
| --- | --- |
| `gp.UI.RunSelectionViewModelContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunProductionHUDFoundationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDBootstrapContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunLaunchMenuPresentationContractTest` | **Complete Failures=0 Cancelled=false** |
| Existing Selection **component** contract | **None in repo** — no additional Selection/UI contract to re-run |

Full suite: **not run** (no cross-cutting defect found).

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** (editor incremental GPUIRuntime compile/link; `GP Win64 Development` ran UHT: 10 generated files written) |
| `GP Win64 Development` | **Succeeded** |
| `GP Win64 Shipping` | **Succeeded** |

## Final diff audit vs `origin/main`

Behind: **0**. Ahead: implementation + docs/report finalization.

Only expected `GPUIRuntime` C++ / tests / docs. **None** of:

- `Content`
- `Config`
- maps
- Blueprint / DataAsset
- Tools
- generated / binary files
- `WBP_GP_HUD`

## Protected-file audit

**Not staged / not committed:**

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/Content/Basic_VFX/`
- `GP/Content/GrimProtocol/Blueprint/` (includes authored `WBP_GP_HUD`)
- `GP/Content/GrimProtocol/DataAssets/`
- `GP/Content/GrimProtocol/Materials/`
- `GP/Content/Mixed_Magic_VFX_Pack/`
- `GP/Content/RocketThrusterExhaustFX/`
- `Tools/`
- `GP/GP.uproject`

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.
