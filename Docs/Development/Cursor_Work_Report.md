# Cursor Work Report — GP-S33M RTS Movement Reconciliation

## Status
**GP-S33M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Branch
`feature/gp-s33m-rts-movement-reconciliation`  
Prior remote head: `1e47b728042fea96266ee1e270c2b1bf1c9b4372`  
This revision: *(recorded after commit)*

## Operator second-pass facts
| Check | Result |
| --- | --- |
| Building NavigationObstacle | **PASS** (unchanged) |
| NavMesh / unit nav / unit↔unit avoidance | PASS (prior) |
| Reassigned Worker CargoFull → automatic haul | **FAIL** (second operator pass) |

### Log fact
Manual Mine after standing full cargo → `MineRejected: Reason=CargoFull`.  
Proves cargo did fill from mining, but automatic HaulReturnToBase had not started. Manual CargoFull reject remains intentional.

## Why previous contract was a false positive
`GPMineReassignmentHaulContractTest` masked the bug:
- teleported tested Worker near NodeB
- issued a second `IssueMine(Worker, NodeB)` if assignment looked wrong

That repaired broken Held/binding/serial state before forcing mining cycles, so CargoFull→haul could pass without proving the natural reassignment chain.

## Corrected contract methodology
`gp.Resource.RunMineReassignmentHaulContractTest` rewritten:
1. Fill A slots with fillers
2. Spawn two tested workers
3. Issue **exactly one** `Mine(A)` per tested worker
4. **No** teleport / second IssueMine / direct BeginMining / retarget helpers on tested workers after that
5. Wait for natural reassignment → move → mining B
6. Prove ownership (Held=B, MineTarget=B, ActiveMineSerial, Mining node=B, Mining state, binding alive) **before** any `DebugForceExecuteMiningCycle`
7. Force cycles only accelerate already-valid mining
8. Assert automatic haul, unload, return to B
9. Concurrent worker2 keeps independent component chain
10. End-stage manual Mine while full cargo remains rejected (does not start haul)

## Proven root cause
Not only the prior Idle-during-`BeginMiningAtHeldTarget` gap.

Factual failures in the natural chain:
1. **Mine SelfSupersede teardown** — `RequestMineApproachMove` replace while moving broadcast `Cancelled/Superseded` with the same serial; `TryConsumeMineMovementResult` cleared Held + `ResetMineExecutor`, leaving approach without coherent Mine ownership (Attack already ignored SelfSupersede; Mine did not). Haul had the analogous Superseded→`WaitingForDropOff` path.
2. **Post-BeginMining ownership not reaffirmed** after remine/retarget — binding/serial/MineTarget could diverge from the active deposit after SlotFullAlternative → B.
3. **Stale local Node after retarget** — fallthrough could `BeginMining` the pre-retarget Node pointer after Held was updated to B.
4. **Orphan CargoFull** — if UnitCommand was unbound when MiningComponent reached CargoFull, multicast never started haul; cargo stayed full → later manual Mine correctly rejected `CargoFull`.

## Exact code fix
- Mine/Haul: ignore movement `Cancelled+Superseded` self-replace (keep chain)
- After successful `BeginMining` (Started/Waiting/Already): reaffirm `MineTarget`, `LastMineDepositForHaul`, `ActiveMineSerial`, `BindMiningStateEvents`
- Retarget: log Old/New/Held/MineTarget; refresh Node from Held after SlotFullAlternative
- `LastMineDepositForHaul` survives `MineTarget.Reset()` for haul deposit resolution
- `UGP_MiningComponent::SetMiningState` direct `NotifyMiningComponentTerminal` after multicast (orphan CargoFull safety net)
- CargoFull: recover `ActiveMineSerial` from Held if cleared; idempotent when haul already active
- Idle remine ignore remains **only** under `bBeginMiningAtHeldTargetInProgress` (external `StopMining` still clears)

## State ownership after reassignment
- `HeldCommand.TargetActor` = active mine intent deposit (updated to B on retarget)
- `MineTarget` / `LastMineDepositForHaul` = runtime active deposit for approach/haul
- `ActiveMineSerial` = chain identity shared with haul
- Manual Mine + full cargo still rejected; does not start haul

## Building NavigationObstacle
Operator PASS. Implementation not changed this revision.

## Test results (Failures=0)
| Test | Result |
| --- | --- |
| gp.Resource.RunMineReassignmentHaulContractTest | PASS |
| gp.Resource.RunS28RegressionSuite | PASS |
| gp.Resource.RunDropOffResilienceContractTest | PASS |
| gp.Resource.RunContainerLaunchContractTest | PASS |
| gp.Resource.RunContainerLaunchHUDContractTest | PASS |
| gp.Movement.RunRTSMovementReconciliationContractTest | PASS |
| gp.Combat.RunAttackMoveContractTest | PASS |
| gp.Combat.RunAutoAcquireContractTest | PASS |
| gp.Combat.RunSalvageWalkerContractTest | PASS |
| gp.Combat.RunLOSFireGateContractTest | PASS |
| gp.Resource.RunOrbitalUnitDropContractTest | PASS |
| gp.Building.RunOrbitalBuildingDropContractTest | PASS |

## Build
GPEditor Win64 Development + UHT: **PASS**  
GP Development / Shipping: not run.

## Operator retest (Worker only)
Two workers, deposit A full/unavailable, one player Mine(A):
- reassign → mine B → CargoFull → automatic MainBase haul → unload → continue  
- no manual Mine after cargo full

## NOT MERGED
