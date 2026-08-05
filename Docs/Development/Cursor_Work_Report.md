# Cursor Work Report — GP-S28 Contract Runner Isolation / Ownership / Async Null-Safety

## Task
GP-S28 — Contract Runner Isolation, Ownership and Async Null-Safety

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28-storage-threat`

## Base
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e`

## Prior correction
EndPlay occupancy: `7f81d19d236d0cf197c1c650174ef28532245244`

## Crash
- GUID: `UECC-Windows-989B9A8648D69236AEE3A7ACE8E502A7`
- `EXCEPTION_ACCESS_VIOLATION` reading `0x0000000000000548`
- Callstack: `UGP_WorkerHaulingContractTestRunner::AdvanceStage` `GPWorker.cpp:1977`
- Null `Worker` dereferenced after `WaitHaulOrMove` returned false on invalid weak

## Command ordering (GP.log)
1. Hauling (or Worker suite overlap) still had scheduled `AdvanceStage`
2. `gp.Resource.RunDiagnosticScenarioContractTest` started with `OperatorTeam1Present=true`
3. Team1 diagnostic actors EndPlay-destroyed
4. Diagnostic spawned new Team1 (`T1_1`) and failed remap assertions
5. Stale Hauling case 6 crashed on destroyed `PrimaryWorkerWeak`

## Premature Complete
`Worker.RunContractTest: Complete Failures=0` could print while another async runner (Hauling) still owned world actors / timers — each runner had only local `GActive*` locks, not global mutual exclusion. Nested `GEngine->Exec(gp.Cargo.RunContractTest)` removed from Worker contract.

## Root cause (Team1 destruction)
Diagnostic cleanup used TeamId + generic `OwnedByContract` and cleaned Team1/Team2 contract leftovers while Hauling still held contract-owned Team1/Team2 actors. That destroyed the live Hauling Worker; Hauling timer continued → AV. Remap then saw Team1 free and respawned `T1_1`.

## Execution coordinator
`GPContractTestCoordinator` (narrow PIE lock):
- single active token
- `TryAcquire` / `Release` / `IsTokenActive` / world tear-down check
- reject: `ContractTestRejected … Reason=AnotherContractTestRunning`
- wired into Diagnostic / EndPlay / Hauling / Worker / Mining / Storage / Cargo

## Runner ownership IDs
Exact OwnerTags: `GP_DiagOwner_Operator`, `GP_DiagOwner_<Kind>_<ExecutionId>`.  
Cleanup only via `CleanupScenarioByOwnerTag`. Contract spawn remaps **before** cleanup; never Team-wide contract wipe.

## Null-safe stage audit
Hauling `WaitHaulOrMove` + case 6 (and related stages) validate Worker/Base/Cmd/Cargo/Storage before use; controlled `PartialStorageObjectsLost` + Finish. AdvanceStage ignores stale token after Finish.

## Sequential S28 suite
`gp.Resource.RunS28RegressionSuite` — Cargo → Mining → Worker → Hauling → Storage → Diagnostic → EndPlay, waiting on coordinator finish callback.

## Isolation contract
`gp.Resource.RunContractIsolationContractTest`:
- ContractRunnerMutualExclusion
- ContractOwnedCleanupIsolation
- AsyncActorLossNullSafety

## Tests / build
- GPEditor Win64 Development + UHT: **PASSED**
- GP Dev/Shipping: **Not run**
- PIE suite / isolation: **operator validation pending**

## Files changed
- `Debug/GPContractTestCoordinator.*` — global token
- `Debug/GPContractIsolationAndSuite.cpp` — suite + isolation commands
- `GPResourceLoopDiagnostics.*` — OwnerTag spawn/cleanup/remap-before-cleanup
- `GPWorker.*` / `GPMiningComponent.*` / `GPStorageComponent.*` / `GPCargoComponent.cpp` — coordinator + null-safety
- Docs: task, AI log, this report

## Map / content / LFS
Unchanged

## Correction commit
4b5331cb3333b46bb952453540dab6d268bff9cd

## Git state
Branch `feature/gp-s28-storage-threat` pushed; main untouched; no PR
