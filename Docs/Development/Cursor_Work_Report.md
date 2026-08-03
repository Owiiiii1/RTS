# Cursor Work Report

## Task
GP-S21 Held Move integration analysis

## Status
ANALYSIS_READY_IMPLEMENTATION_PENDING

## Branch
feature/gp-s21-held-move-integration-analysis

## Base
main @ b676a4a41ae29349d3e1ca0c0d4051e23a97d76e

## Current Pipeline
```text
Input → BuildSmartCommand → Server validation → Dispatch
→ AGP_UnitBase::ReceiveCommand
→ UGP_UnitCommandComponent::HandleCommand
→ HeldAccepted / HeldReplaced / QueueDeferred
```

Separate (unlinked) movement:
```text
AGP_MobileUnit → UGP_MovementComponent → RequestMove / StopMove
```

RMB currently Holds only; physical move only via `gp.Movement.Test`.

## Selected Architecture
- Owner: `UGP_UnitCommandComponent` after non-queued Held store
- Private sync: `SynchronizeMovementWithHeldCommand(const TOptional<FGP_StoredUnitCommand>& PreviousCommand)`
- Resolve movement: `Cast<AGP_MobileUnit>(Owner)->GetUnitMovementComponent()`
- Move tag: exact `CommandTag == FGPGameplayTags::Get().Command_Move`
- Move held → `RequestMove(TargetLocation, CommandSerial)`
- Non-Move while `IsMoving` → `StopMove(CommandReplaced)`
- QueueDeferred → no sync call
- RequestMove false → keep Held; log reject (no rollback)
- Non-mobile / missing component → Held kept; `MovementUnavailable`

Rejected: UnitBase reaction, MobileUnit ReceiveCommand override, extra router, delegates.

## Transition Matrix
| Previous | Incoming | Queue | Held | Movement |
| --- | --- | ---: | --- | --- |
| None | Move | false | Accepted | RequestMove |
| Move | Move | false | Replaced | RequestMove (MoveReplaced) |
| Move | Attack/Mine | false | Replaced | StopMove(CommandReplaced) if moving |
| Attack/Mine | Move | false | Replaced | RequestMove |
| Attack/Mine | Attack/Mine | false | Replaced | none |
| Any | Any | true | QueueDeferred | unchanged |
| None | Attack/Mine | false | Accepted | none |

Tags verified present: Move, Attack, Mine.

## Stop Reason Decision
**Option B selected:** plain C++ `EGP_MovementStopReason { Manual, CommandReplaced, EndPlay }` and `StopMove(Reason = Manual)`.

- Not UENUM / Blueprint
- Avoids incorrect `Reason=Manual` for Attack cancel
- Console Stop → Manual; EndPlay → EndPlay; command cancel → CommandReplaced
- Option A rejected (wrong Manual semantics); Option C rejected (duplicate API)

## Exact Proposed API
```cpp
// UnitCommandComponent private
void SynchronizeMovementWithHeldCommand(
    const TOptional<FGP_StoredUnitCommand>& PreviousCommand);

// MovementComponent
enum class EGP_MovementStopReason : uint8 { Manual, CommandReplaced, EndPlay };
void StopMove(EGP_MovementStopReason Reason = EGP_MovementStopReason::Manual);
```

Logs (`LogGPUnitCommandExecution`): MoveExecutionRequested, MoveExecutionRejected, MovementCancelledByCommand, MovementUnavailable.

Ordering: capture previous → store Held → sync movement → HeldAccepted/Replaced logs.

## Exact Files
### Modified (future implementation)
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Units/GPMovementComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPMovementComponent.cpp`

### Unchanged
UnitBase, Unit, MobileUnit API, PC, CommandComponent, payloads, tags, Build.cs, assets/maps/config

### Build.cs impact
NO

## Validation Plan
- Host RMB Move → HeldAccepted + MoveStarted; serial match
- Second RMB before reach → HeldReplaced + MoveReplaced
- Moving + RMB enemy → Held Attack + MoveStopped CommandReplaced; no attack
- Held Attack + RMB ground → MoveStarted
- Shift+RMB while moving → QueueDeferred; movement continues
- Remote Team 2 → server Held+Move; both see transform; no client execution
- Multi-unit independent serials
- Regression: console still works; no Held clear on reach; no AI/Nav/GAS

## Deferred
- GP-S22 completion callback / Held clear / stale protection
- NavMesh / AIController / Attack/Mine execution / queue
- Stale Held Move after Reach (document-only until S22)

## Completion Status (after future impl + validation)
DONE_WITH_COMPLETION_DEFERRED

## Docs Changed (this analysis)
- `Docs/Development/Claude_Tasks/GP-S21_Held_Move_Integration.md` (new)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Git State
- git diff --check: clean
- working tree clean after commit/push
- branch pushed: `feature/gp-s21-held-move-integration-analysis`
- HEAD = origin
- no merge to main
- confirmation: docs-only; no C++/assets/maps/config
