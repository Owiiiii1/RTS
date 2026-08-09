# Cursor Work Report — GP-S33M RTS Movement Reconciliation

## Status
**GP-S33M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Branch
`feature/gp-s33m-rts-movement-reconciliation`  
Prior remote head: `e9e3f6aca3bffc13710572a87c780856e1699ae6`  
This revision: (see commit after push)

## Operator third-pass facts
| Check | Result |
| --- | --- |
| Building NavigationObstacle | **PASS** (unchanged; keep authored footprint) |
| Natural reassignment → CargoFull → `HaulReturnToBase` | **PASS** (proven by operator logs) |
| Haul `RequestMove` after CargoFull | **FAIL** — `Reason=PathNotFound` → `HaulApproachMoveRejected` → `DropOffWait Enter Reason=PathRejected` → StillUnreachable |

### Log fact (third failure)
```
HaulReturnToBase ... Cargo=50
MoveRejected ... Reason=PathNotFound
HaulApproachMoveRejected
DropOffWait Enter Reason=PathRejected
Retry → StillUnreachable
```
CargoFull → haul transition is proven working. Failure is isolated to navigation approach selection around MainBase after NavigationObstacle (`NavArea_Null`) was added.

## Root problem
Haul used a single deterministic radial destination from `TryMakeRangeApproachDestination(Owner, MainBase, DropOffRange, …)` then `UGP_MovementComponent::RequestMove` → project + `FindPathSync`.

With a building NavArea_Null footprint that one point may land on a disconnected polygon, behind the obstacle relative to approach lanes, or project on-nav with **no complete path**. Observed result is `PathNotFound`, not `DestinationOffNav`.

Worker only needs `DistanceToMainBase <= DropOffRangeCm` — not one predetermined XY.

Do **not** weaken pathfinding with straight-line through buildings. Do **not** remove NavigationObstacle. Do **not** tell operator to shrink the nav box.

## Fix — nav-aware reachable approach
Ownership: `UGP_UnitCommandComponent::TryFindReachableRangeApproachDestination` (interaction-range semantics).

Shared evaluator: `GPResourceApproach::EvaluateRangeApproachPath` (8 directions).

### Candidate algorithm
- Index 0 = direct radial toward worker (legacy geometry)
- ±45°, ±90°, ±135°, 180° (8 total)
- Candidate radius: outside NavigationObstacle world-bounds XY clearance + unit/acceptance safety, and **inside** `DropOffRangeCm`
- Clearance from `AGP_BuildingBase::GetNavigationObstacle()` bounds (`Bounds.BoxExtent`); ResourceNode uses collision box
- If obstacle nearly fills DropOffRange → `HaulApproachConfigFailure` (no silent route into Null)

### Validation / scoring
Per candidate: project to nav → still within DropOffRange → complete `FindPathSync` (no partial) → score by **shortest nav path length**, then lowest candidate index.

No global scans. Generic `UGP_MovementComponent` PathNotFound at far distance unchanged.

### Fallback
- Prefer reachable alternate if radial fails
- All candidates fail → existing `WaitingForDropOff` / `PathRejected` + rate-limited retry
- No-nav / start projection fail → legacy single radial (may still reject at RequestMove)

### Two workers
Distinct Worker positions yield distinct radials (index 0). No docking-slot architecture this slice.

### ResourceNode mine approach
Same helper reused via `TryMakeMineApproachDestination`. Mining slot semantics unchanged.

### Diagnostics (one-shot, not per-frame)
- `HaulApproachCandidate` (Unit, Serial, CandidateIndex, Candidate, Projected, WithinRange, PathResult, PathLength)
- `HaulApproachSelected` (… Destination, PathLength, MainBase, DropOffRange)
- `HaulApproachNoReachableCandidate` / `HaulApproachConfigFailure`

## Retained prior worker fixes
Natural reassignment ownership, SelfSupersede Mine/Haul, post-BeginMining reaffirmation, stale-node fix, orphan CargoFull safety, no-teleport/no-second-Mine contract methodology. Manual Mine with full cargo remains rejected.

## Building NavigationObstacle
Operator PASS. Not altered this revision (getter/bounds only for clearance).

## Contract coverage
1. **`gp.Resource.RunMineReassignmentHaulContractTest`** — MainBase NavigationObstacle activated; `DebugSkipCandidateMask` forces radial unavailable; asserts alternate candidate index > 0; unload succeeds. Fails if implementation only retries the same PathNotFound radial.
2. **`gp.Resource.RunHaulNavApproachContractTest`** (new) — A direct fails / B alternate succeeds / C dest on-nav, in DropOffRange, outside obstacle / D all unreachable → WaitingForDropOff / E no straight-line Null fallback.

## Test results (Failures=0)
| Test | Result |
| --- | --- |
| gp.Resource.RunMineReassignmentHaulContractTest | PASS |
| gp.Resource.RunHaulNavApproachContractTest | PASS |
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

## Operator retest
Same current map, same authored MainBase NavigationObstacle, same two Workers, occupied deposit A, one Mine:
automatic B → CargoFull → HaulReturnToBase → path chooses reachable side of MainBase → workers route around building/nav obstacle → enter DropOffRange → unload → continue chain.  
No operator NavigationObstacle changes required.

## NOT MERGED
