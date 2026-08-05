# Cursor Work Report — GP-S28 ResourceNode EndPlay Reentrant Occupancy Cleanup

## Task
GP-S28 — ResourceNode EndPlay Reentrant Occupancy Cleanup Fix

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28-storage-threat`

## Base
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e`

## Prior correction
Registry uniqueness: `c59b12031d88ea9b3c9dd584e4aa1028c2a846dc`

## Operator PIE teardown ensure
Full haul loop worked (Mining → CargoFull → ReturnToBase → DropOff → ReturnToDeposit → Mining; Storage 150/500). On PIE Stop:

```
Ensure condition failed: Array has changed during ranged-for iteration!
AGP_ResourceNode::EndPlay() GPResourceNode.cpp:137
```

## Exact stack / source
- `AGP_ResourceNode::EndPlay` ranged-for over `ActiveMiners` then `WaitingMiners`
- Line ~137: `BroadcastMinerSlotStateChanged(... Active → None)`

## Root cause
Live-array callback mutation: occupancy Broadcast → `UGP_MiningComponent::HandleMinerSlotStateChanged` → `StopMining(TargetEndPlay)` → `ReleaseMiningSlot` removes from `ActiveMiners`/`WaitingMiners` (and could PromoteWaiting) during the ranged-for.

## Snapshot / clear / guard solution
1. Copy Active + Waiting snapshots  
2. Set `bIsClearingOccupancy`  
3. Reset production arrays; counts = 0  
4. Broadcast terminal None from snapshots only  
5. `OnMinerSlotStateChanged.Clear()`  
6. `Super::EndPlay`

## Release / Promote during teardown
- `RequestMiningSlot` → `RejectedDepositInvalid`
- `ReleaseMiningSlot` → idempotent no-op (no Broadcast, no Promote)
- `PromoteWaitingMiners` → forbidden
- `CleanupInvalidMiners` / `RefreshOccupancyCounts` → no-op / zeros
- No new delegate broadcasts except controlled snapshot notifications

## Listener review
- **MiningComponent:** Unbind before Release; skip Release when `IsClearingOccupancy`; Waiting→Active ignored while clearing → `TargetEndPlay`
- **UnitCommand:** TargetEndPlay + cargo>0 may start haul without return-to-deposit; zero cargo does not haul; no false threat without drop-off
- Contract runners use production MiningComponent path for occupancy teardown stage

## EndPlay contract test
`gp.Resource.RunEndPlayContractTest`
- `ResourceNodeEndPlayWithActiveAndWaitingMiners` — 5 miners (4 Active + 1 Waiting), Destroy node; assert no promote, 5× None, timers off, TargetEndPlay
- `ResourceNodeEndPlayDuringHaulLoop` — navigable Worker scenario, partial cargo, Destroy node; no false threat; no stale node ref

## Haul-loop teardown test
Covered in contract stage above; operator still validates infinite haul + PIE Stop.

## Full regression results
GPEditor compile: **PASSED**.  
Console suite (`RunEndPlayContractTest`, DiagnosticScenario, Storage, Hauling, Worker, Mining, Cargo) + PIE Stop after haul: **operator validation pending**.

## Non-blocking note
`LogCrowdFollowing: Unable to find RecastNavMesh instance while trying to create UCrowdManager` after `BeginTearingDown` — engine teardown warning; not the ensure root cause; NavigationSystem scope not expanded.

## GPEditor / UHT
**PASSED**

## GP Dev / Shipping
**Not run**

## Files changed
- `GPResourceNode.h/.cpp` — EndPlay snapshot/guard; API guards; `IsClearingOccupancy`
- `GPMiningComponent.h/.cpp` — Release skip while clearing; EndPlay contract runner + `gp.Resource.RunEndPlayContractTest`
- `GPUnitCommandComponent.cpp` — TargetEndPlay partial cargo haul without return-to-deposit
- Docs: task, AI log, this report

## Map / content / LFS
Unchanged

## Correction commit
(see git after push)

## Git state
Branch `feature/gp-s28-storage-threat` pushed; main untouched; no PR
