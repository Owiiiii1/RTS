# Cursor Work Report — GP-S28 Hauling Contract Local Geometry / Timeout Fix

## Task
GP-S28 — Hauling Contract Local Geometry / Timeout Fix

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28-storage-threat`

## Base
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e`

## Prior correction
Contract isolation: `4b5331cb3333b46bb952453540dab6d268bff9cd`

## Isolation test PASS
`gp.Resource.RunContractIsolationContractTest` → Complete Failures=0  
(mutual exclusion, ownership cleanup, Team1 preserve / Team2 remap, async actor-loss null-safety)

## Suite progression
`gp.Resource.RunS28RegressionSuite`:
- Cargo / Mining / Worker → Complete Failures=0
- WorkerHauling → FAIL PartialStorageHaulTimeout → suite Complete Failures=1 (correct stop)

## Exact PartialStorageHaulTimeout log
- FreshNode `X=-53000 Y=200 Z=100`
- MainBase ~`X=0 Y=-1500`
- HaulReturnToBase Distance=`52947.4`
- MoveStarted from `X=-52920` toward ~`X=-314`
- Then `FAIL: PartialStorageHaulTimeout`

## Root cause
Hauling contract case 5 spawned `SpawnNode(FVector(-53000, 200, 100))` — legacy absolute test strip far from the navigable diagnostic scenario MainBase (~origin). Production haul semantics were fine; contract geometry was wrong.

## Why increasing timeout is rejected
~53 km-uu travel is bad test geometry, not a slow-move flake. Raising timeout would hide the bug and keep later stages on the same broken far strip.

## Local navigable geometry solution
- Store `ScenarioBaseLocation` / `ScenarioNodeLocation` from stage-0 navigable spawn
- `SpawnNavigableNodeNearScenario`: candidate offsets around scenario node; NavMesh project; approach path Node→Base; distance band ~800–4000 cm; spawn only after validation; post-spawn re-check + destroy on fail
- PartialStorage logs `PartialStorageGeometry` (Distance / projections / Nav / ExpectedTravelSeconds / TimeoutSeconds) and asserts travel budget << timeout
- Controlled FAIL `PartialStorageLocalGeometryFailed` (no timeout mask)

## Audit of later hauling stages
Replaced absolute far coords:
- case 5 FreshNode `-53000` → local helper
- case 7 replace node `-53500` → local helper
- case 9 enemy MainBase `-54000` → `ScenarioBaseLocation + (0,700,0)`
- case 10 restore MainBase `-52400` → projected `ScenarioBaseLocation`  
Worker-contract off-map geometry tests (`-48000`…`-51000`) left unchanged (out of scope).

## Path validation
Pre-spawn + post-spawn NodeApproach↔BaseDropOff reachability required before PartialStorage haul begins.

## Suite result
Compile gate PASSED. Full PIE `RunS28RegressionSuite` Complete Failures=0 — **operator validation pending**.

## GPEditor / UHT
**PASSED**

## GP Dev / Shipping
**Not run**

## Files changed
- `GPWorker.h/.cpp` — `SpawnNavigableNodeNearScenario`, local geometry in hauling late stages, PartialStorageGeometry logging
- Docs: task, AI log, this report

## Map / content / LFS
Unchanged

## Correction commit
(see git after push)

## Git state
Branch `feature/gp-s28-storage-threat` pushed; main untouched; no PR
