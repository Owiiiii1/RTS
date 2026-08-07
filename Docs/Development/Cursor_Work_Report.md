# Cursor Work Report — GP-S28P2 FIFO Crash Correction

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28p2-depletion-resource-reassignment` (no merge; main untouched)

## Third operator failure
5 Workers on one ResourceNode (MaxConcurrentMiners=4): Active=4 Waiting=1 → Unreal Editor crash.

## Stale CrashContext
Saved/Crashes dated **05.08** (`WorkerHaulingContractTestRunner`) was **not** used as root cause for this failure.

## Actual 07.08 log evidence
Tight same-frame loop:
```
MineBegin ... Result=1 (WaitingForSlot)
ResourceCandidate Rejected ... Reason=ExcludedNode (Active=4 Waiting=1)
ResourceReassignmentNoCandidate ... Reason=SlotFullAlternative
MineRetarget ... NewTarget=<same ResourceNode>
MineBegin ... Result=1
```
repeated many times in frame [304].

## Exact synchronous state loop
`BeginMiningAtHeldTarget` treated `WaitingForSlot` as a trigger for `TryAutoReassignMine(SlotFullAlternative)`, which fell through to `TryRetargetMineToNode(same full node)` → `StopMining` FIFO churn → `BeginMining` again.

## Correction
- Free-slot alternative search **before** FIFO `BeginMining` only
- `WaitingForSlot` / `AlreadyMiningTarget` → stable wait (log `MineWaitingForSlot` once)
- `TryAutoReassignMine` never same-target retargets a full preferred node
- Same-target retarget guard + re-entry flag on `BeginMiningAtHeldTarget`
- Promotion remains MiningComponent occupancy delegate (no second BeginMining)

## FIFO stable-state contract
WaitingForSlot = terminal-stable until Waiting→Active promotion.

## Test results
| Command | Result |
| --- | --- |
| `gp.Resource.RunDepletionReassignmentContractTest` | Extended with FIFO regression; **PIE not run non-interactively — operator pending** |
| `gp.Resource.RunS28RegressionSuite` | **Not run non-interactively — operator pending** |

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Dev / Shipping | Deferred |

## Operator-local assets — untouched
DefaultEngine.ini, map, Blueprint/**, Materials/**, authored ResourceNode, Niagara.

## Commit SHA
Filled after commit on this branch tip.
