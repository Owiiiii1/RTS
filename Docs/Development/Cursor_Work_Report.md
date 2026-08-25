# Cursor Work Report

## Status

**BOTTOM_HUD_GROUP_AND_CONTEXT_ACTIONS_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization. The branch continues with PURCHASE categories after operator PASS.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head: `5cab8e244f4e90415776956f67424976214fbd9f`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping` / full suite: **not run** (intermediate gate)

## Group Selection event seam

Canonical SoT remains `UGP_SelectionViewModel::GroupRows` (factual selection order, cap **24** unchanged).

HUD seam (already present; not rewritten):

- `UGP_HUDRootWidget::GetSelectionGroupRows()`
- `BP_OnSelectionPresentationChanged()`

Row fields already complete: `Index`, `DisplayName`, `CurrentHealth`, `MaxHealth`, `HealthNormalized`, `bIsUnit`, `bIsBuilding`.

`SetGroupRowVitals` already broadcasts `OnSelectionPresentationChanged`. HUD root already forwards that multicast to `BP_OnSelectionPresentationChanged`. Contracts confirmed the event fires on group-row health push (single → group, group health, clear). No Tick.

Group **visual** grid is still not authored / not operator-validated.

## Context Action modes

`UGP_ContextActionPresenter` owned by `UGP_HUDViewModelSubsystem`. Source is live local `UGP_SelectionComponent` (not inspect-only SelectionVM Single). Inspect-only → **None**.

| Mode | Rule |
| --- | --- |
| **None** | 0 live selected; mixed unit+building; multi-building; non-friendly MainBase |
| **Unit** | exactly one live selected `IsSelectionTypeUnit()` |
| **UnitGroup** | >1 live selected, **all** `IsSelectionTypeUnit()` and none `IsSelectionTypeBuilding()` |
| **Building** | exactly one live selected building that is **not** `AGP_MainBase` |
| **MainBase** | exactly one live selected **friendly** `AGP_MainBase` (class seam, not DisplayName) |

Factual selection policy: Shift/Ctrl click and marquee can mix types (no mixed-prevention in the container). Mixed is **not** given invented UnitGroup/Building actions → **None**.

## Exact action availability

### Unit / UnitGroup

| Action | Visible | Enabled | Notes |
| --- | --- | --- | --- |
| Move | yes | **no** | no local Move targeting mode exists (RMB smart-command only) |
| Stop | yes | **yes** | live selected units |
| Attack-Move | yes | `SelectionHasAttackMoveEligibleUnit()` | existing Salvage Walker capability query, not actor-class hardcode |
| Patrol | yes | **no** | backend not implemented; no new tag/RPC |
| Purchase | **absent** | — | |

### Building (LogisticsHub / Turret)

Empty action list. Purchase absent.

### MainBase

Purchase visible/enabled. Unit actions absent.

## Move decision

No `EnterMoveMode` / Move targeting modal exists. HUD **Move** is visible and disabled (`DisabledReason`: Move targeting mode is not implemented). Does not emulate clicks or invent world coordinates.

## Stop implementation / routing

`GP.Command.Stop` already validates and executes on authority. Missing piece was a player/UI emit path.

Added `AGP_PlayerController::RequestStopSelectedUnits()`:

- local-only
- reads live selection
- builds `FGP_CommandRequest` with `GP.Command.Stop`
- submits through existing `Server_RequestCommand`
- does **not** call unit executors directly
- no alternate RPC

## AttackMove routing

`RequestContextAction(AttackMove)` calls existing `EnterAttackMoveMode()`. Eligibility uses the same PC query, now public: `SelectionHasAttackMoveEligibleUnit()`. No second modal owner.

## MainBase Purchase entry seam

`RequestContextAction(Purchase)` / `RequestOpenMainBasePurchase()` set presenter panel state to `PurchaseRoot` and broadcast `OnContextActionsChanged`. No spend, no Units/Buildings/Defense, no gameplay Purchase RPC.

Leaving MainBase selection resets panel to `Actions`.

## Exact changed files

New:

- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPContextActionPresentationContractTest.cpp`

Modified:

- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPHUDViewModelSubsystem.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPSelectionViewModelContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md` (this file)

`WBP_GP_HUD` is **not** in the diff and was **not** modified.

## Exact focused tests

`L_PrototypeArena` `-game -unattended -nop4 -NullRHI`.

| Command | Result |
| --- | --- |
| `gp.UI.RunContextActionPresentationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunSelectionViewModelContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunProductionHUDFoundationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDBootstrapContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunLaunchMenuPresentationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Combat.RunAttackMoveContractTest` | **Complete Failures=0 Cancelled=false** |

No existing dedicated Stop/command-dispatch contract in repo. Full suite / GP Development / GP Shipping: **not run**.

## GPEditor / UHT

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** (UHT processed GPEditor, 11 generated files written) |

## Protected-file audit

**Not staged / not committed:**

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/Content/Basic_VFX/`
- `GP/Content/GrimProtocol/Blueprint/` (includes `WBP_GP_HUD`)
- `GP/Content/GrimProtocol/DataAssets/`
- `GP/Content/GrimProtocol/Materials/`
- `GP/Content/Mixed_Magic_VFX_Pack/`
- `GP/Content/RocketThrusterExhaustFX/`
- `Tools/`
- `GP/GP.uproject`

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.

## Operator Blueprint wiring (`WBP_GP_HUD`, local only)

Do **not** commit the WBP.

1. **Group icon row widget** — consume `FGP_SelectionGroupRow` (`Index`, `DisplayName`, health fields, `bIsUnit` / `bIsBuilding`). Placeholder images only; no gameplay icon field.
2. **Rebuild group grid** on `BP_OnSelectionPresentationChanged`: call `GetSelectionGroupRows()` (stable `Index` 0..N-1, N ≤ 24). Extra reserved cells stay empty. Event also fires when a group-row health value changes.
3. **Context Action Grid buttons** — rebuild on `BP_OnContextActionsChanged` from `GetContextActionPresentations()`. Drive visibility/enablement from `bVisible` / `bEnabled`. Show `DisabledReason` if useful. Use `GetContextActionMode()` for Unit vs Building vs MainBase layout.
4. **OnClicked** → `RequestContextAction(ActionId)`. Expected live behavior:
   - Stop: selected units stop via existing command path
   - Attack-Move: existing A-mode (LMB ground); only if enabled
   - Move / Patrol: remain disabled
   - Purchase: `GetContextActionPanelState()` becomes `PurchaseRoot` (placeholder; no catalog yet)
5. Do not Tick, do not read Actors/ASC from the widget, do not create presenters/VMs in the widget.

After operator PASS: **do not finalize**. Next Cursor task on this same branch is PURCHASE categories.
