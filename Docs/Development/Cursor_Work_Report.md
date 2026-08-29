# Cursor Work Report

## Status

**BOTTOM_HUD_MOVE_PATROL_CURSOR_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head: `b708d09e6f55e4e03a88ba2a28d7f56291620e17`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)
- Project-wide full automation suite: **does not exist** (only resource-scoped `gp.Resource.RunS28RegressionSuite`). Broader command / combat / movement / HUD / marquee contracts **were run** (below).

## Targeting state: unified (option B)

Attack-Move already owned a local modal (bools, Tick input-edge ownership, LMB confirm, RMB/Esc cancel, selection/marquee/smart-RMB suppression). Move and Patrol needed the same ownership.

Unified to `EGP_CommandTargetingMode { None, Move, AttackMove, Patrol }` on `AGP_PlayerController`. Public AttackMove APIs remain wrappers (`EnterAttackMoveMode`, `CancelAttackMoveMode`, `IsAttackMoveModeActive`, `UpdateAttackMoveInputEdgesForContract`, `ConsumeAttackMoveCommandInput`) so existing AttackMove contracts still compile and keep the same confirm / cancel / eligibility semantics.

Why B was safe: one cursor helper, one selection-invalidation path, one confirm/cancel path. No second RPC. Combat AttackMove behavior is unchanged.

Canonical modal rules (all modes):

- local-only
- UI enter → LMB valid ground confirm → RMB cancel → Esc cancel (existing Tick input-edge ownership)
- while active: selection/marquee blocked; smart RMB blocked; building placement cannot co-own input (placement cancels targeting first)
- confirm/cancel restore `EMouseCursor::Default`
- no Tick gameplay decisions beyond the existing input-edge ownership

## Exact Move request path

1. Unit/group Context Action `RequestContextAction(Move)` → `UGP_ContextActionPresenter` → `AGP_PlayerController::EnterMoveMode()` → `EnterCommandTargetingMode(Move)` when `SelectionHasMoveEligibleUnit()`.
2. LMB ground (or contract `ConfirmCommandTargetingDestinationForContract`) → `ConfirmCommandTargetingDestinationAt`.
3. Builds `FGP_CommandRequest`: `CommandTag = GP.Command.Move`, `TargetLocation` = clicked ground, `IssuingUnits` = selected `IsMobileCommandEligible()` units, `bQueue = IsShiftModifierDown()`.
4. `CancelCommandTargetingMode()` then `Server_RequestCommand(Request)`.
5. Existing `UGP_CommandComponent::ValidateAndNormalizeCommand` / `DispatchValidatedCommand` → `UGP_UnitCommandComponent::HandleCommand` → existing Move executor (`SynchronizeMovementWithHeldCommand` / `RequestMove`).

No direct `MovementComponent` call from UI or PlayerController confirm.

## Exact Patrol request path

1. `RequestContextAction(Patrol)` → `EnterPatrolMode()` when `SelectionHasPatrolEligibleUnit()` (same mobility seam as Move).
2. LMB ground → `FGP_CommandRequest` with `GP.Command.Patrol` (new canonical tag; none existed), `TargetLocation` = clicked ground, `IssuingUnits` = selected mobile-eligible units, **`bQueue = false` always** (MVP does not invent queued patrol).
3. `CancelCommandTargetingMode()` then `Server_RequestCommand(Request)`.
4. CommandComponent accepts Patrol like Move (location sanity + mobile-eligible filter + group destination spread).
5. Unit command `HandleCommand` stores Held, `ResetPatrolExecutor()`, `BeginPatrolExecutor()`, then movement sync.

No `Server_RequestPatrol`. No client-authoritative patrol.

## Patrol per-unit state owner

`UGP_UnitCommandComponent` (not UI):

- `bPatrolActive`
- `PatrolAnchorA` = owner location at accept
- `PatrolAnchorB` = held destination
- `bPatrolHeadingToB`

MVP: one destination click. Two-click A→B patrol is out of scope.

## Arrival / loop mechanism

`UGP_MovementComponent` already broadcasts `EGP_MovementResult::Reached` after `ClearActiveMovementState`. `TryConsumePatrolMovementResult` listens to that seam (no world-distance Tick polling).

On Reached: flip heading, write next destination into Held `TargetLocation`, `RequestMove` with the **same command serial**. Failed movement clears patrol + held. `Superseded` / `CommandReplaced` are ignored so a replacement command can own the next move.

## Stop / replacement cleanup

- **STOP:** `ResetPatrolExecutor()` then `StopMove(Manual)` then clear Held (same Stop path as other held commands).
- **Any replacing explicit command** (`HandleCommand` accept path): `ResetPatrolExecutor()` before the new Held executes. Covered for Move and AttackMove in the targeting contract; smart RMB uses the same `HandleCommand` replacement.
- **Death / EndPlay / NotifyOwnerDied:** `ResetPatrolExecutor()` plus existing movement stop.

## Cursor enum mapping

Built-in `EMouseCursor` only. No `.uasset`. Central helper `RefreshCommandTargetingCursor()`.

| Mode | Cursor |
| --- | --- |
| None (normal) | `Default` |
| Move | `Crosshairs` |
| AttackMove | `Crosshairs` |
| Patrol | `CardinalCross` |
| Building placement active | `Default` (placement precedence; placement had no custom cursor) |

## Every cursor reset path

`RefreshCommandTargetingCursor()` / `CancelCommandTargetingMode()` restore `Default` on:

- successful confirm (`ConfirmCommandTargetingDestinationAt`)
- RMB cancel (`CancelAttackMoveModeFromRMB` → `CancelCommandTargetingMode`)
- Esc cancel (existing Tick edge ownership)
- selection invalidation (`HandleSelectionChangedForCommandTargeting`)
- entering building placement (`EnterBuildingPlacementMode` cancels targeting first)
- `CancelBuildingPlacement` (refresh)
- EndPlay
- any forced `CancelCommandTargetingMode`

Constructor / `BeginPlayingState` set `DefaultMouseCursor` / `CurrentMouseCursor` = `Default`.

## ContextAction availability

Eligibility is `AGP_UnitBase::IsMobileCommandEligible()` (valid, not dead, not destroying, `IsSelectionTypeUnit()`, `AGP_MobileUnit`, has movement component). **No Worker / SalvageWalker class-name checks.** AttackMove eligibility remains SalvageWalker capability tag.

| Selection | MOVE | STOP | ATTACK MOVE | PATROL | PURCHASE |
| --- | --- | --- | --- | --- | --- |
| Unit / UnitGroup | visible; enabled if ≥1 mobile-eligible | visible; enabled | existing SalvageWalker capability | visible; enabled if ≥1 mobile-eligible | absent |
| MainBase | absent | absent | absent | absent | Purchase only |
| Other buildings | unchanged (empty) | | | | |

UI routing (WBP OnClicked already wired; **WBP_GP_HUD not changed**):

- `RequestContextAction(Move)` → `EnterMoveMode`
- `RequestContextAction(Patrol)` → `EnterPatrolMode`

Message Strip (secondary): `UGP_HUDRootWidget::GetCommandTargetingPrompt()` BlueprintPure.

- MOVE: `Select destination`
- ATTACK MOVE: `Select destination`
- PATROL: `Select patrol point`
- idle: empty

Presenter rebuilds on `OnCommandTargetingModeChanged` (no Tick). Cursor feedback is the required visual; prompt is optional WBP bind.

## Exact changed files (implementation head)

`b708d09e6f55e4e03a88ba2a28d7f56291620e17`

- `GP/Source/GPGASRuntime/Public/Tags/GPGameplayTags.h`
- `GP/Source/GPGASRuntime/Private/Tags/GPGameplayTags.cpp`
- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Private/Command/GPCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Command/GPCommandTargetingContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPCommandTargetingContractTest.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPContextActionPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPContextActionPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPContextActionPresentationContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Exact tests / results

`L_PrototypeArena` `-game -unattended -nop4 -NullRHI`. Editor killed after Complete. No `quit` in ExecCmds.

| Command | Result |
| --- | --- |
| `gp.Commands.RunMovePatrolTargetingContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Combat.RunAttackMoveContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Worker.RunCommandIntentContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunContextActionPresentationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunSelectionViewModelContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunProductionHUDFoundationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunHUDBootstrapContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Selection.RunMarqueeUnitsOnlyContractTest` | **Complete Failures=0 Cancelled=false** |

Focused targeting cases (all PASS): Move eligibility + enter + `GP.Command.Move` held + cursor Default after confirm; RMB/Esc cancel with no command + cursor reset; AttackMove cursor Crosshairs; Patrol CardinalCross + per-unit anchors; arrival B→A and A→B; Stop clears Patrol; replacement Move/AttackMove clears Patrol; destroy cleanup; building-placement transition forced-cleanup restores Default (placement did not become active under NullRHI; fallback `U_ForcedCleanupRestoresDefaultWithoutPlacement` PASS).

## GPEditor / UHT

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** |

## Protected-file audit

**Not staged / not committed:** Config, maps, Blueprint (including `WBP_GP_HUD` / `WBP_GP_SelectionGroupRow`), DataAssets, Materials, VFX packs, Tools, `GP.uproject`, cursor assets.

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.

## Concise operator test

1. Select a Worker or SalvageWalker. MOVE and PATROL are enabled. STOP enabled. PURCHASE absent.
2. MOVE → cursor becomes Crosshairs → LMB ground → unit uses existing Move → cursor Default.
3. RMB or Esc during targeting cancels with no command; cursor Default.
4. PATROL → cursor CardinalCross (not Crosshairs) → one LMB → unit loops current position ↔ click. STOP ends it. A new Move / Attack-Move / smart RMB replaces it.
5. SalvageWalker ATTACK MOVE still uses Crosshairs and existing combat travel.
6. MainBase: Purchase only. Other buildings unchanged.
7. Optional: bind Message Strip to `GetCommandTargetingPrompt()` on `BP_OnContextActionsChanged`. Cursor is PlayerController-driven; no widget cursor assets.

INTERMEDIATE / NOT MERGE READY.
