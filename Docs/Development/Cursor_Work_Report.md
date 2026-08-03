# Cursor Work Report

## Task
GP-S24 attack terminal cleanup fix

## Status
CODE_READY_OPERATOR_VALIDATION_PENDING

## Branch
feature/gp-s24-attack-execution-implementation

## Base
main @ 462f9bba0bc64c5a7da4fdd5eae9207f36524a7f

## Defects
1. DestroyTarget while Approaching: FinishAttack → StopMove(Manual) → sync Cancelled/Manual fell through to Held Move routing → false `MovementResultIgnored HeldTagNotMove` (because `bFinishingAttack` short-circuited Attack consume).
2. AttackFinished after target destroy logged Distance=FLT_MAX from sentinel `TNumericLimits<float>::Max()`.

## Root Cause
- Cleanup StopMove callback ran while `bFinishingAttack==true` and ActiveAttackSerial already cleared, so TryConsume returned false.
- Distance helper returned FLT_MAX for null/invalid target and logged it as a real value.

## Fix Ordering
```text
capture serial/state/target + TryCompute distance
→ bFinishingAttack=true
→ set bExpectAttackCleanupStopResult + PendingAttackCleanupMovementSerial
→ clear Attack runtime (Idle / serial 0)
→ StopMove(Manual)
→ TryConsume: TerminalCleanupStop (Cancelled/Manual, exact pending serial) → return true
→ clear cleanup expectation
→ clear exact Held Attack
→ AttackFinished (Distance=-1 DistanceAvailable=false if unavailable)
```

Range-entry still uses `bExpectRangeEntryStop` only. External Manual cancel still Failed/MovementCancelled.

## Build Results
- GPEditor Development: **PASSED**
- UHT: **PASSED**

## Operator Validation Matrix
| Case | Result |
| --- | --- |
| Approaching→Ready | **PASS** |
| Ready→Approaching | **PASS** |
| Attack retarget | **PASS** |
| Attack→Move | **PASS** |
| Invalid Self/Friendly/Null | **PASS** |
| DestroyTarget Ready | **PASS** |
| DestroyTarget Approaching | **PASS** functional; cleanup false-ignore + FLT_MAX **fixed** (retest cleanup log) |
| Moving-target SelfSupersede | **PASS** |
| EndPlay | **PASS** |
| QueueDeferred | **PENDING** |
| Remote Team 2 | **PENDING** |
| Multi-unit | **PENDING** |

## Files Changed
- `GPUnitCommandComponent.h/.cpp`
- GP-S24 doc / AI log / Cursor report

## Scope Verification
- MovementComponent changed: **NO**
- UnitBase / tags / Build.cs / assets: **NO**
- damage/GAS/Nav/queue: **NO**
- finalization builds: **NO**

## Git State
- Same branch pushed
- Working tree clean after commit/push
- HEAD = origin
- no merge to main

## Operator Validation Needed
1. DestroyTarget during Approaching → AttackTargetInvalidated → AttackApproachResultIgnored IgnoreReason=TerminalCleanupStop → AttackFinished Distance=-1 DistanceAvailable=false; **no** HeldTagNotMove
2. DestroyTarget Ready unchanged
3. `gp.Movement.Stop` during Approaching still MovementCancelled
4. Range-entry Manual still Ready
5. QueueDeferred still pending

## Deferred
Damage/health/GAS/Nav/Mine/queue/replication/UI; finalization after full operator accept
