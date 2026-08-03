# Cursor Work Report

## Task
GP-S23 movement result propagation analysis

## Status
ANALYSIS_READY_IMPLEMENTATION_PENDING

## Branch
feature/gp-s23-movement-result-analysis

## Base
main @ 5a41a2352f50d598ab8ee3e557791659403d6552

## Current Code Findings
- GP-S22 broadcasts only `Reached` via `FGP_OnMovementCompleted`; `StopMove` never broadcasts.
- `RequestMove` returns `bool`; reject logs `MoveRejected` / `MoveExecutionRejected` but Held Move remains (phantom Held).
- Move→Move silently replaces active serial (`MoveReplaced`); no terminal for previous serial.
- Move→non-Move calls `StopMove(CommandReplaced)` without result callback; Held already replaced.
- Manual `gp.Movement.Stop` leaves Held Move.
- EndPlay: silent `StopMove(EndPlay)` + command-layer `HeldCleared`.
- No runtime `Failed` producer (straight-line XY only).
- Call sites confined to `GPMovementComponent` / `GPUnitCommandComponent` (+ non-shipping console).

## Selected Architecture
Single native terminal delegate for accepted/active moves; synchronous structured `RequestMove` outcome for rejects (no sync reject multicast). Rename GP-S22 completion types to result types with Reason. Omit `Failed` until Nav stage.

## Result Contract
```cpp
enum class EGP_MovementResult : uint8 { Reached, Cancelled };
enum class EGP_MovementResultReason : uint8 { None, Superseded, CommandReplaced, Manual };
FGP_OnMovementResult(Serial, Result, Reason);
```
Sync-only: `FGP_MovementRequestOutcome { Status, RejectReason }`.

## RequestMove Decision
Return `FGP_MovementRequestOutcome`. No RequestRejected broadcast (reentrancy with Held mutation). On Rejected after Held-set Move: clear exact matching Held in `SynchronizeMovementWithHeldCommand`. Allocator never rewinds.

## Cancellation Decision
- Move→Move: emit Cancelled/Superseded after committing new active state.
- Move→non-Move: StopMove(CommandReplaced) emits Cancelled/CommandReplaced.
- Manual: emit Cancelled/Manual; clear matching Held Move.
- EndPlay: silent; no Cancelled broadcast.

## Held Policy
Clear only authority + exact Move tag + exact serial on Reached or Cancelled (and sync Reject clear in sync path). Newer / non-Move / no Held → ignore + log. Stale serial protection retained.

## Reentrancy Rules
Capture → mutate movement-local → log → broadcast → return. No post-broadcast mutation of old serial. No broadcast on Reject or EndPlay. Callback may start a new Move safely.

## Expected Implementation Files
- `GPMovementComponent.h/.cpp`
- `GPUnitCommandComponent.h/.cpp`
- `GP-S23_Movement_Result_Propagation.md`
- `AI_Project_Log.md`
- `Cursor_Work_Report.md`

NO: GPMobileUnit, GPStoredUnitCommand, gameplay tags, Build.cs, assets/maps/config.

## Validation Plan
2P Listen Server: Reached clear; real Rejected (no phantom Held); Move→Attack Cancelled ignores new Held; Move→Move Superseded; Manual clears Held; stale inject; reentrancy new Move; authority-only remote/multi-unit; EndPlay no crash/unsafe callback.

## Scope Verification
- C++ changed: **NO**
- Build.cs changed: **NO**
- assets/maps/config changed: **NO**
- Attack/Mine added: **NO**
- Nav/pathfinding added: **NO**
- queue implementation added: **NO**

## Git State
- Branch: `feature/gp-s23-movement-result-analysis`
- Base: `main` @ `5a41a2352f50d598ab8ee3e557791659403d6552`
- Docs-only commit; working tree clean after commit/push
- HEAD = origin
- no merge to main

## Implementation Pending
Explicit implementation task required. Target stage close status: `DONE_WITH_FAILED_RESULT_DEFERRED`.
