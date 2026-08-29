# Cursor Work Report

## Status

**BOTTOM_HUD_PATROL_COMBAT_CURSOR_FIX_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head: `71d88dcf592184c97fb79d17c5482aef39e276de`
- Previous targeting implementation: `b708d09e6f55e4e03a88ba2a28d7f56291620e17`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Root cause of invisible cursor

Writing `CurrentMouseCursor` was a contract-only change. In `FInputModeGameAndUI`, Slate `FSlateUser::QueryCursor` walks widgets under the mouse. HUD overlay widgets typically return Unhandled; Slate then **forces `EMouseCursor::Default`** and never consults `APlayerController::CurrentMouseCursor`.

The production viewport seam is `FSceneViewport::OnCursorQuery` → `UGameViewportClient::GetCursor` → `GetMouseCursor()` (reads `CurrentMouseCursor`, and `InitInputSystem` copies `DefaultMouseCursor` into it). That path only runs when the hovered widget is the scene viewport, not a full-screen UMG overlay.

## Exact cursor fix

1. `GetCommandTargetingCursor()` is the single mapping.
2. `RefreshCommandTargetingCursor()` writes **both** `DefaultMouseCursor` and `CurrentMouseCursor`, then `FSlateApplication::QueryCursor()`.
3. `UGP_HUDRootWidget::NativeOnCursorQuery` returns `FCursorReply::Cursor(GetCommandTargetingCursor())` so the GameAndUI HUD overlay uses the same mapping. **WBP_GP_HUD was not modified.**

Why it should be visible in real PIE: Slate now receives a handled cursor reply from the HUD root (overlay) and the PlayerController fields that `FSceneViewport` queries when the mouse is on the viewport.

Automated `-game` cannot prove OS-rendered pixels. Contracts check the APIs Slate queries: `GetCommandTargetingCursor()`, `DefaultMouseCursor`, `CurrentMouseCursor`, and `GetMouseCursor()` when it is not `None`.

## Effective built-in cursor mapping

| Mode | Cursor |
| --- | --- |
| None / placement | `Default` |
| Move | `Crosshairs` |
| AttackMove | `Crosshairs` |
| Patrol | `CardinalCross` |

No cursor `.uasset`. Reset on confirm, RMB, Esc, invalid selection, building placement enter, `CancelBuildingPlacement`, EndPlay, forced mode replacement.

## Root cause of Patrol not attacking

`IsEligibleForCombatAutoAcquire()` treated Held `GP.Command.Patrol` like Move and returned false. Idle `TryIssueAutoAcquireAttack()` would have called `HandleCommand(Attack)` and **replaced** Patrol. There was no locomotion-parent acquire path for Patrol (AttackMove had one; Patrol did not).

## Parent Patrol / temporary combat architecture

Shared AttackMove engagement seam, no second executor, no new RPC:

- `IsEligibleForPatrolAcquire()` — combat-capable (`IsCombatCapableForAutoAcquire`, SalvageWalker capability tag, not class name) + Held Patrol + not already attacking.
- Scan: AttackMove **or** Patrol → `FindNearestAutoAcquireTarget(AttackMove)` → `StartAttackMoveEngagement`.
- Held Patrol stays the parent. Attack FSM uses the Patrol serial.
- `HasExactActiveHeldAttack()` now includes `Command_Patrol` so attack movement results are not treated as patrol arrival.
- `TryConsumePatrolMovementResult` ignores results while `IsAttackActive()`.

Worker has no combat capability → patrols, never acquires.

## Resume semantics

On attack terminal (`FinishAttack`), if Held is still Patrol with the same serial → `ResumePatrolTravelAfterEngagement()`:

- does **not** change AnchorA / AnchorB
- resumes `HeadingToB ? B : A` (the leg in progress before engagement)
- `RequestMove` same serial

Reached on that leg then flips A↔B as before.

## Movement-result ownership

`HandleMovementResult`: Retaliation → Attack → Patrol → Haul → Mine.

Attack approach/self-supersede consumes combat movement. Patrol only handles Reached / failure when attack is idle.

## Stop / replacement cleanup

Unchanged `HandleCommand` path: `ResetAttackExecutorForReplacement` + `ResetPatrolExecutor` before the new Held. STOP clears both, no resume. Move / AttackMove / smart command replace Patrol. Death/`NotifyOwnerDied` / EndPlay clear both.

## Exact tests / results

`L_PrototypeArena` `-game -unattended -nop4 -NullRHI`. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.Commands.RunMovePatrolTargetingContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Combat.RunPatrolCombatContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Combat.RunAttackMoveContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Combat.RunAutoAcquireContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Combat.RunRetaliationPursuitContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Worker.RunCommandIntentContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunContextActionPresentationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.Selection.RunMarqueeUnitsOnlyContractTest` | **Complete Failures=0 Cancelled=false** |

## GPEditor / UHT

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** |

## Exact changed files (implementation head)

`71d88dcf592184c97fb79d17c5482aef39e276de`

- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPCommandTargetingContractTest.cpp`
- `GP/Source/GPRuntime/Public/Combat/GPPatrolCombatContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPPatrolCombatContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPContextActionPresentationContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected-file audit

**Not staged / not committed:** Config, maps, Blueprint (`WBP_GP_HUD` / `WBP_GP_SelectionGroupRow`), DataAssets, Materials, VFX, Tools, `GP.uproject`, cursor assets.

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.

## Concise operator test

1. PIE: MOVE → Crosshairs; ATTACK MOVE → Crosshairs; PATROL → CardinalCross; confirm/RMB/Esc → arrow.
2. SalvageWalker PATROL past an enemy: unit stops the A↔B travel, attacks, enemy dies → resumes the same leg, then flips A↔B.
3. STOP during that fight: no resume. MOVE / Attack-Move during the fight: Patrol gone.
4. Worker PATROL near an enemy: keeps walking, never attacks.

INTERMEDIATE / NOT MERGE READY.
