# Cursor Work Report — GP-S28 Diagnostic Nav-Reachability Correction

## Task
GP-S28 — Diagnostic Nav-Reachability Correction

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28-storage-threat`

## Base
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e`

## Prior commits
- Candidate: `cd83858390db086c6913669f348a7402ae0a5ad3`
- TeamId diagnostic correction: `61f69dff98bb2b79f795a74d93e0b2c8a2b12b76`

## Operator nav failure
Scenario actors TeamId=1 / Node=5000 / Storage 0/500 correct, but:
- NavWorkerToNode=false, NavNodeToBase=false
- ReadyForHaulingTest=false, Warnings=2
- Hardcoded layout (-45000 / -43000 / -42850) outside PrototypeArena NavMesh (~origin ±2km)

## False-positive contract test
`RunDiagnosticScenarioContractTest` completed Failures=0 without requiring nav path assertions — fixed.

## Old hardcoded layout
Rejected: west-strip (-45000,0,100) family without ProjectPointToNavigation success.

## Actual nav-anchor discovery
Order:
1. Authored non-diagnostic ResourceNode projected to nav
2. Existing MobileUnit projected to nav
3. PlayerStart / PrototypeArena candidates (0,0,100) and nearby
4. `GetRandomPoint` fallback

## Projection extents
Primary project extents 800–1200 cm XY/Z; approach re-project 600–800 cm.

## Approach endpoint policy
Paths use approach points, not actor origins:
- Mine approach ≈ InteractionRange − AcceptanceRadius − Safety (125 cm from Node)
- Drop-off approach ≈ DropOffRange − AcceptanceRadius − Safety (325 cm from MainBase)

## MainBase drop-off endpoint
Outside collision / within DropOffRange=400; AcceptanceRadius=50 accounted.

## Three path checks
- Worker → NodeApproach
- NodeApproach → BaseDropOff
- BaseDropOff → NodeApproach

## Atomic spawn / cleanup
Validate layout + paths **before** spawn; on post-spawn failure destroy all created actors; Ok=false, Created*=false.  
Tag-scoped cleanup: `GP_DiagScenario_T{N}` operator vs `GP_DiagScenario_OwnedByContract`.

## ReadyForHaulingTest rules
Requires NavSystemPresent + all projections + all three paths.  
Missing NavSystem = error (not warning); Ready cannot be true without nav.

## Contract assertions
NavigationSystemPresent, WorkerProjected, NodeApproachProjected, BaseDropOffProjected, WorkerToNodePathReachable, NodeToBasePathReachable, BaseToNodePathReachable, ReadyForHaulingTest.  
Nav=false ⇒ Failures>0.

## Regression
TeamId registration lifecycle preserved; hauling contract stage-0 now uses navigable scenario spawn.

## GPEditor / UHT
**PASSED**

## GP Dev / Shipping
**Not run**

## Files changed
- `GPResourceLoopDiagnostics.h/.cpp` — nav discovery, atomic spawn, tags, approach paths
- `GPWorker.cpp` — List/SuggestedCommand, contract assertions, hauling navigable spawn
- `GPStorageComponent.cpp` — full scenario call signature
- Docs: task, AI log, this report

## Map / content / LFS
Unchanged (no NavMeshBoundsVolume edit)

## Correction commit
*(filled after commit)*

## Git state
Branch pushed; main untouched; no PR
