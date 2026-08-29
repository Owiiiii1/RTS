# Cursor Work Report

## Status

**BOTTOM_HUD_RUNTIME_CURSOR_PATROL_COMBAT_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head (implementation): `c90dceb062ec320da8975a29dfe9a57709489ce7`
- Previous operator-fail checkpoint: `71d88dcf592184c97fb79d17c5482aef39e276de`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping`: **not run** (intermediate gate)

## Exact old cursor failure

Two prior attempts wrote `CurrentMouseCursor` / `DefaultMouseCursor` and/or `UGP_HUDRootWidget::NativeOnCursorQuery`. Contracts passed because they asserted the enum. Operator PIE still showed the default arrow.

Root cause: under `FInputModeGameAndUI`, `FSlateUser::QueryCursor` walks the widget under the mouse. Production `WBP_GP_HUD` descendants typically return Unhandled, so Slate **forces `EMouseCursor::Default`**. Root `NativeOnCursorQuery` is not a guaranteed query owner. `FSceneViewport::OnCursorQuery` → `GetMouseCursor()` only runs when the hovered widget is the scene viewport.

This slice does **not** try a third hardware-cursor query fix.

## Exact native overlay implementation

New native-only Slate leaf `SGPCommandCursorOverlay` (no `.uasset`, no Blueprint).

- `AGP_PlayerController::ShowCommandCursor(Mode)` / `HideCommandCursor(Reason)`
- Host: full-viewport `SOverlay`, `EVisibility::HitTestInvisible`
- Added via local player `UGameViewportClient::AddViewportWidgetContent` at **ZOrder 10000** (HUD 100, marquee 1000)
- Paint is volatile and follows `FSlateApplication::Get().GetCursorPos()`
- No tick, no gameplay logic, no click blocking
- While targeting: `bShowMouseCursor = false`, hardware cursor `None`
- `GetCommandTargetingCursor` remains a harmless leftover mapping (returns `None` while targeting)
- `UGP_HUDRootWidget::NativeOnCursorQuery` **removed**

## Viewport ownership and cleanup

Centralized:

| Event | Hide reason |
| --- | --- |
| Enter targeting | Show (mode replace updates visual, same overlay) |
| Confirm | `Confirm` |
| RMB | `RMB` |
| Esc | `Esc` |
| Selection invalidation | `SelectionInvalid` |
| Building placement enter | `BuildingPlacement` |
| EndPlay / PC teardown | `EndPlay` (remove viewport content, reset shared ptrs) |
| Viewport rebuild | `Rebuild` |

Building placement cannot leave a command cursor. Overlay pointers are reset after `RemoveViewportWidgetContent`. No dangling Slate widget.

## Visual shape per mode

Slate primitives only. Color is not the only differentiator.

| Mode | Shape |
| --- | --- |
| Move | four-tick crosshair + small center square |
| AttackMove | crosshair + extra ring |
| Patrol | crosshair + two opposing arrows |

Operator PIE is the visual pixel gate. Automated tests do **not** assert rendered pixels or that the hardware cursor enum must change.

## Exact Patrol combat gating bug

`IsCombatCapableForAutoAcquire()` returned true only for:

- `GP.Unit.Type.SalvageWalker`
- `GP.Building.Type.DefensiveTurret`

Patrol acquire used that method. Operator units that can attack but lack the SalvageWalker **capability tag** never scanned/engaged. Contracts passed because they spawned `AGP_SalvageWalker`.

## New canonical combat capability rule

`UGP_UnitCommandComponent::IsCombatCapable()` (no class-name checks, SalvageWalker tag is **not** canonical combat):

- live / valid / definition ready
- not `GP.Unit.Type.Worker`
- `IsAttackConfigValid()` (`TryResolveEffectiveAttackRange()` range > 0)
- `UGP_UnitAttributeSet::GetDamage()` finite and > 0

Patrol auto-acquire uses that rule. Idle auto-acquire / timer start now share the same factual surface (turrets keep working via attack config). **Attack-Move eligibility is unchanged:** SalvageWalker capability tag only.

## Prove test uses combat unit WITHOUT SalvageWalker tag

`gp.Combat.RunPatrolCombatContractTest` stages K/L spawn native `AGP_Unit` (`GP_Unit_0`):

- `K_CombatUnitHasNoSalvageWalkerTag`
- `K0_CombatCapableWithoutSW`
- `E_AttackMoveEligibilityStaysSalvageWalker` (`IsEligibleForAttackMoveAcquire()` false)
- `K_PatrolAcquireWithoutSalvageWalker`
- `K_TemporaryAttackKeepsPatrolParent`
- `L_ResumeSamePatrolLeg`

Runtime log proof:

- `GP PatrolAcquire TargetFound Unit=GP_Unit_0 Target=GP_Worker_7`
- `GP PatrolEngage Started Unit=GP_Unit_0 Target=GP_Worker_7`
- `GP PatrolResume Unit=GP_Unit_0 ... HeadingToB=true`

Worker: `GP PatrolAcquire Disabled Unit=GP_Worker_5 Reason=NotCombatCapable` and no acquire. SalvageWalker stages remain the regression.

## Patrol temporary engagement / resume

Unchanged architecture, now reachable for any combat-capable unit:

- scan: existing auto-acquire timer + `FindNearestAutoAcquireTarget(..., AttackMove)`
- hostility / validity / FoW / range / LOS / death: existing filters
- `StartAttackMoveEngagement` under Held Patrol (parent not replaced)
- `FinishAttack` → `ResumePatrolTravelAfterEngagement()` same A/B leg, same serial, anchors unchanged
- non-combat units continue Patrol with no acquire

## Exact tests

All `-game` `L_PrototypeArena` `-unattended -nop4 -NullRHI`, one at a time, no `quit`, editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.Commands.RunMovePatrolTargetingContractTest` | Complete Failures=0 Cancelled=false |
| `gp.Combat.RunPatrolCombatContractTest` | Complete Failures=0 Cancelled=false |
| `gp.Combat.RunAttackMoveContractTest` | Complete Failures=0 Cancelled=false |
| `gp.Combat.RunAutoAcquireContractTest` | Complete Failures=0 Cancelled=false |
| `gp.Combat.RunRetaliationPursuitContractTest` | Complete Failures=0 Cancelled=false |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | Complete Failures=0 Cancelled=false |
| `gp.Worker.RunCommandIntentContractTest` | Complete Failures=0 Cancelled=None |
| `gp.UI.RunContextActionPresentationContractTest` | Complete Failures=0 Cancelled=false |
| `gp.Selection.RunMarqueeUnitsOnlyContractTest` | Complete Failures=0 Cancelled=false |

Cursor contract now checks overlay lifecycle (visible + mode, hardware hidden via `bShowMouseCursor`, Move→Patrol same overlay mode update, confirm/RMB/Esc/placement overlay gone). It does **not** require hardware `EMouseCursor` to change.

## GPEditor / UHT

`GPEditor Win64 Development` **Passed** (UHT ran; makefile invalidated for added overlay source).

## Changed files (implementation commit)

- `GP/Source/GPRuntime/Public/UI/SGPCommandCursorOverlay.h`
- `GP/Source/GPRuntime/Private/UI/SGPCommandCursorOverlay.cpp`
- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Combat/GPPatrolCombatContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPPatrolCombatContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPCommandTargetingContractTest.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`

## Protected audit

Not modified in this commit:

- `WBP_GP_HUD`
- `WBP_GP_SelectionGroupRow`
- `Content/`
- `Config/`
- maps
- DataAssets
- Materials
- VFX
- `Tools/`
- `GP.uproject`

No destructive git. Local dirty Content/Config/maps/Tools/uproject left unstaged.

## Operator test

**Operator PIE is the visual gate.** Automated tests cannot prove pixels.

1. Select a mobile unit. Press MOVE. Hardware arrow hides. Overlay crosshair + center square follows the mouse above the HUD. Clicks still work.
2. Without confirming, press PATROL. Overlay stays; visual switches to crosshair + opposing arrows.
3. LMB ground: overlay gone, normal cursor back, Patrol starts.
4. RMB or Esc while targeting: overlay gone, no command.
5. Enter building placement while targeting: overlay gone.
6. Combat unit that is **not** SalvageWalker (native `AGP_Unit` / any unit with valid range+damage, not Worker): Patrol past a hostile → temporary attack → resume same A/B leg. Held stays Patrol.
7. Worker Patrol: walks A↔B, never auto-acquires.
8. SalvageWalker Attack-Move still available; generic combat unit still has **no** Attack-Move cell.

INTERMEDIATE / NOT MERGE READY.
