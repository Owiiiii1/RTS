# GP-S28P2 — Resource Depletion, Registry, Reassignment and FIFO Recovery

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
- Branch: `feature/gp-s28p2-depletion-resource-reassignment`
- Base main: `86bcc9740fde0f19ac40c70f2f49298680f5f7d6`
- Approach-path / settings: `53e5ff944730180764731c75fb495a38adeb91ab`
- FIFO crash correction: see latest commit on this branch

## Goal
Safe one-shot ResourceNode depletion, registry, path-aware reassignment / WaitingForResource, and stable FIFO WaitingForSlot — without changing mining cadence, Cargo, Storage, Threat, or combat.

## Operator failure #3 — FIFO crash (07.08 GP.log)
5 Workers → one node (Max=4): Active=4 Waiting=1, then Editor crash.

**Stale CrashContext (05.08 WorkerHaulingContractTestRunner) is NOT the root cause.**

**Actual cause (07.08 log):** synchronous same-frame loop:
`MineBegin(WaitingForSlot)` → `SlotFullAlternative` → `MineRetarget(same node)` → `MineBegin` …

## FIFO stable-state contract
After `BeginMining` → `WaitingForSlot`:
- Worker stays in node WaitingMiners FIFO
- No BeginMining / SlotFullAlternative / same-target MineRetarget
- Promotion only via `OnMinerSlotStateChanged` Waiting→Active → Mining + timer (no second BeginMining)

Alternative free-slot search runs **once before** FIFO entry. If none → FIFO wait.

## Guards
- Same-target retarget no-op + `MineRetargetSkippedSameTarget` (once per transition)
- WaitingForSlot on current target → executor idle for search/retarget
- `bBeginMiningAtHeldTargetInProgress` blocks recursive Begin↔Retarget stacks

## Preserved PASSED operator cases
- A: deplete Node A → haul → retarget Node B
- B: 5 Workers, Node A full + Node B free → 5th goes to Node B

## Settings
`UGP_ResourceGameplaySettings` / `DefaultGame.ini` unchanged by this correction.

## Tests
`gp.Resource.RunDepletionReassignmentContractTest` includes FIFO subcase (5+1 Workers, promote order, watchdog ≤1 BeginMining, no same-target retarget). PIE result **operator pending** (not claimed non-interactively).

## Builds
- GPEditor Win64 Development + UHT — **PASSED**
- GP Dev/Shipping — deferred

## Operator-local assets
Untouched / uncommitted: Blueprint/**, Materials/**, map, DefaultEngine.ini, Niagara.
