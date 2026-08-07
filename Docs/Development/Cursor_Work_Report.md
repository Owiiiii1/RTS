# Cursor Work Report — GP-S28P3 Operator Validation Note

## Status
GP-S28P3_CODE_READY_OPERATOR_VALIDATION_PENDING

## Branch
feature/gp-s28p3-dropoff-resilience

## Tip referenced
`422bc70454bf51a9cdd31dc2ab4f490f20f018a0` (operator helper)

## Scope
Docs-only. No C++, Config, Blueprint/map/content, no new debug helpers.

## Operator Validation
| Scenario | Result |
| --- | --- |
| A — Missing MainBase + registration wake | **PASS** |
| B — MainBase destroyed during active haul | **PASS** |
| C — existing MainBase unreachable → path restored → retry | **DEFERRED** |
| D — explicit Move replaces WaitingForDropOff | **PASS** |

### A PASS (observed)
WaitingForDropOff; Cargo preserved; runtime MainBase registration wakes; auto-deliver; Accepted/Threat OK; P2 PostDropOff continues.

### B PASS (observed)
Movement cancelled; `Reason=MainBaseDestroyed`; Cargo preserved; WaitingForDropOff; replacement register wakes; deliver; `ReturnToDeposit=true`; return to deposit + continue mining.

### D PASS (observed)
Mine/Haul cancelled `CommandReplaced`; Held → Move; Cargo preserved; later MainBase register does not stale DropOffWait Wake; Move completes; no haul resurrect.

### C DEFERRED (not failed)
Current MovementComponent is not full NavMesh/path-following; wall/BlockingVolume/NavModifier after destination accept is not a valid MoveFailed operator test. See [`DEFERRED_VALIDATION_GP-S28P3_Scenario_C.md`](DEFERRED_VALIDATION_GP-S28P3_Scenario_C.md). Re-run after **future canonical navigation/path-following movement stage**.

Remaining manual C is an **accepted deferred validation dependency** on future navigation implementation — **not** a blocker for current P3 finalization, because deterministic contract unreachable coverage already exists and remains PASS.

## Helpers
- Kept: `gp.Resource.SpawnTestMainBase`, `gp.Resource.DestroyTestMainBase`
- Abandoned / not implemented: `MakeTestMainBaseUnreachable`, `MakeTestMainBaseReachable`

## Commit
`51f88c7d15b3fe2404cd7e07922cddf5513eab08`
