# Cursor Work Report — GP-S28 MainBase Registry Uniqueness + Contract Isolation

## Task
GP-S28 — MainBase Registry Uniqueness + Diagnostic Contract Isolation

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28-storage-threat`

## Base
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e`

## Prior commits
- Candidate: `cd83858390db086c6913669f348a7402ae0a5ad3`
- TeamId diagnostic correction: `61f69dff98bb2b79f795a74d93e0b2c8a2b12b76`
- Nav correction: `caf5bf0c947176ce5c72affadae41cbbd60be590`

## Successful nav-ready operator scenario
Operator validated Team1 navigable scenario:
- MainBase=`GP_DiagMainBase_T1_0`, Worker=`GP_DiagWorker_T1_0`, Node=`GP_DiagResourceNode_T0_0`
- NavSystemPresent / NavWorkerToNode / NavNodeToBase / NavBaseToNode = true
- ReadyForHaulingTest=true, Errors=0, Warnings=0

## Duplicate registry operator log
With Team1 operator scenario live, `gp.Resource.RunDiagnosticScenarioContractTest` logged:
- `AGP_GameState::RegisterMainBase: duplicate MainBase for TeamId=1 Existing=…_T1_0 New=…_T1_1`
- Then registry Count=2
- `FindMainBaseForTeam` warned multiple; used first
- Contract: Ok=false Reason=MainBaseRegistryResolveFailed, Complete Failures=1

## Root cause
Error was logged for duplicate, but **Add still executed** after the error. Contract also collided with occupied Team1.

## Registry uniqueness invariant
Exactly one registered MainBase per playable TeamId. Stale/invalid weaks pruned before mutate/query.

## Updated RegisterMainBase result
`EGP_MainBaseRegisterResult`:
- Registered
- AlreadyRegistered (same actor, idempotent, no second Add, no Error)
- RejectedNoAuthority / RejectedInvalidActor / RejectedInvalidTeam
- RejectedDuplicate (other actor; no Add; CountForTeam stays 1; Existing preserved)

## Idempotent same-actor behavior
Re-registering the already-registered actor returns AlreadyRegistered; count unchanged.

## Duplicate rejection
Incoming ≠ existing for same TeamId → RejectedDuplicate; actor not inserted; Find resolves existing.

## Stale cleanup
`PruneInvalidMainBaseRegistrations` removes invalid weaks. Destroyed existing clears team slot so a replacement can Register successfully.

## Contract team isolation
Contract spawn remaps to first free playable TeamId (typically Team2 when Team1 occupied). Contract-owned tag `GP_DiagScenario_OwnedByContract`. Cleanup destroys only contract-owned actors. If all playable teams occupied → `BlockedByOccupiedPlayableTeams` (not masked as production failure).

## Operator scenario preservation
Sequence B (Spawn Team1 then Contract) keeps operator Team1 MainBase/Worker/Node and registry Count=1 for Team1.

## Repeat-spawn policy
`gp.Resource.SpawnDiagnosticScenario 1` cleans prior **operator** diagnostic for that team only, then spawns new; never deletes authored/production MainBase; never creates a second Team1 MainBase (`TeamMainBaseOccupied` if non-diagnostic occupies team).

## New assertions
Contract: FirstMainBaseRegistered, SameActorRegistrationIdempotent, DuplicateMainBaseRejected, RegistryCountRemainsOne, ExistingMainBasePreserved, RejectedBaseCleanupDoesNotRemoveExisting, DestroyedExistingClearsRegistry, ReplacementAfterCleanupSucceeds, plus operator-preservation checks when Team1 was present.

Worker.List ScenarioValidation: MainBaseCountForWorkerTeam, RegistryUniqueForTeam, ResolvedMainBaseMatchesListedBase. ReadyForHaulingTest requires Count=1 and Unique=true.

## Full regression results
GPEditor compile gate: **PASSED**.  
In-editor console suite (`RunDiagnosticScenarioContractTest`, Storage/Hauling/Worker/Mining/Cargo contracts, sequence B + Worker.List): **operator validation pending** (runtime PIE not executed in this pass).

## Files changed
- `GPGameState.h/.cpp` — typed RegisterMainBase, prune/count/unique helpers
- `GPMainBase.cpp` — register flag only on success/idempotent; reject-safe EndPlay
- `GPResourceLoopDiagnostics.h/.cpp` — free TeamId, contract remap, operator re-spawn policy, validation uniqueness fields
- `GPWorker.h/.cpp` — contract isolation stages, hauling ContractTeamId, Worker.List fields
- Docs: task, AI log, this report

## GPEditor / UHT
**PASSED**

## GP Dev / Shipping
**Not run**

## Map / content / LFS
Unchanged

## Correction commit
(see git after push)

## Git state
Branch `feature/gp-s28-storage-threat` pushed; main untouched; no PR
