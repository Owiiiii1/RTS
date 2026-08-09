# Cursor Work Report — GP-S33M RTS Movement Reconciliation

## Status
**GP-S33M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Branch
`feature/gp-s33m-rts-movement-reconciliation`  
Prior remote head: `71184d1db098149086a73faa65317e084815dada`  
This revision: `bf9bdd1430246a7b7527a5a7c9ef08b2ea16c18a`

## Operator fourth-pass facts
| Check | Result |
| --- | --- |
| Building NavigationObstacle | **PASS** (unchanged) |
| CargoFull → `HaulReturnToBase` | **PASS** (all workers) |
| Haul approach candidate generation | **FAIL** — `HaulApproachNoReachableCandidate` before any candidate loop |

### Exact supplied values
| Field | Value |
| --- | --- |
| ClearanceHalfXY | 218.2 |
| DropOffRange | 400 |
| Acceptance | 50 |
| Safety | 25 |
| CandidateCount | 0 |
| Reason | ApproachGeometryFailed |

Horizontal room exists: `218.2 + 50 + 25 = 293.2 < 400`. Failure was inside `TryComputeDesiredHorizontalDistance()` before the 8-candidate loop.

### Proven runtime DeltaZ (contract instrumentation)
`GeometryRepro: DeltaZ=-280.0` with Clearance=218.2 / Range=400 / Acc=50 / Safety=25.  
Old 3D budget: `MaxHorizontal = sqrt(400^2 - 280^2) ≈ 285.7` < `MinOutside(228.2) + Acc + Safety (303.2)` → geometry false, CandidateCount stays 0.

## Formula bug
`TryComputeDesiredHorizontalDistance` used `DeltaZ = PathStart.Z - TargetLocation.Z` and `MaxHorizontalBudget = sqrt(Range^2 - DeltaZ^2)`.

MainBase actor/root Z (capsule / BP placement) must not consume the horizontal drop-off budget. NavigationObstacle is an XY footprint; DropOffRange is a planar interaction radius; pathfinding is ground-plane.

## Fix — GroundPlane2D building semantics
- `EGP_RangeApproachDistanceMode::{ThreeDimensional, GroundPlane2D}`
- MainBase haul approach: **GroundPlane2D** (`MaxHorizontalBudget = InteractionRangeCm`)
- ResourceNode mining: **ThreeDimensional** preserved
- Canonical MainBase metric: `AGP_MainBase::ComputeDropOffDistance2D` / `IsWithinDropOffRange2D` (`FVector::Dist2D` to actor center)

### MainBase checks unified to Dist2D
- `StartHaulReturnToBase` in-range gate + logs (`Distance2D`, `DeltaZ`, `DistanceMode=GroundPlane2D`)
- `BeginDropOffAtMainBase` arrival validation
- Approach candidate / projected WithinRange checks for GroundPlane2D
- Predicted worst-case for buildings = `DesiredHorizontal + Acceptance` (no DeltaZ)

### Diagnostics (one-shot)
Failure/selection logs include WorkerLocation, MainBaseLocation, DeltaZ, Distance2D, ClearanceHalfXY, DropOffRange, Acceptance, Safety, DesiredHorizontal, MaxHorizontalBudget, DistanceMode. CandidateCount is 8 when geometry is valid.

## Contracts
1. **`gp.Resource.RunHaulNavApproachContractTest`** — operator numbers (Clearance≈218.2, Range=400, Acc=50, Safety=25) + elevated MainBase Z; old 3D fails; GroundPlane2D CandidateCount=8; Dist2D-in-range / Dist3D-out-of-range unload via storage delta; alternate-candidate + all-unreachable cases retained.
2. **`gp.Resource.RunMineReassignmentHaulContractTest`** — elevated MainBase Z + clearance ~218.2; natural chain unload still PASS.

## Building NavigationObstacle
Operator PASS. Not altered (extent only in transient contract harness).

## Test results (Failures=0)
| Test | Result |
| --- | --- |
| gp.Resource.RunHaulNavApproachContractTest | PASS |
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

## Operator retest
Same map / MainBase / NavigationObstacle — no operator tuning.  
Expected: `HaulApproachCandidate…` → `HaulApproachSelected…` → move → arrival → `HaulDropOffComplete`.

## NOT MERGED
