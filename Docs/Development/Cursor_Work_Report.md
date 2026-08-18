# Cursor Work Report — GP-S41M First-Move Path Start

## Status
**GP-S41M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head
- Branch: `feature/gp-s41m-movement-shortest-yaw`
- Base: `origin/main` @ `d9df23143f256b2b2143fe66f5a0444f727452ae`
- Head: recorded after commit

## Operator result
**FAIL** on first Move after PIE/spawn.

- Static shortest-yaw correction appears to work after the unit has already moved.
- First Move still visibly turns along the wrong / long-looking arc.
- Subsequent Moves rotate correctly by the shortest path.

Operator first-Move log:

- Unit=`BP_GP_SalvageWalkerLONGRAGE_C_1`
- Destination=`(-302.70,-2181.63)`
- StartLocation=`(-733.49,-2229.23,88)`
- PathPoints=`6`
- UsedNav=`true`
- Serial=`1`

## Factual root cause
**Confirmed** on the production path:

`RequestMove` → `TryBuildNavigationPath` → `ProjectPointToNavigation(Start)` → `PathStart = ProjectedStart` → `FindPathSync` / projected-straight → Recast/`PathPoints[0]` copied as the query-start anchor → `PathIndex = 0` → first Tick `DesiredDir2D` / yaw.

Recast / projected-straight **always** returns the FindPathSync query start as path point 0. That point is the nav-mesh **query anchor**, not a destination the already-existing actor must travel toward.

If the first runtime point stays at `ProjectedStart`, the first Tick steers from `ActualStart` toward that anchor. After the unit is already on the mesh, later Moves project close to the actor, so the defect disappears.

Contract proof (`gp.Movement.RunShortestYawContractTest` case J):

| Field | Value |
| --- | --- |
| ActualStart XY | `(80.00, -1400.00)` Z=`50` |
| ProjectedStart XY | `(80.00, -1400.00)` Z=`10` |
| DistActualProjected XY | `0.0` (Z-only snap; XY offset not practical on this arena) |
| Raw Recast Path0 | `(80.00, -1400.00)` **== ProjectedStart** |
| Runtime PathIndex | `0` |
| Runtime Path0 after strip | `(1280.00, -1400.00)` — **not** the query anchor |
| First movement | `+X` toward dest, not toward ProjectedStart |
| Initial yaw | `90.00` |
| First applied yaw | `-6.00` (shortest toward dest yaw `0`) |

`J_RawFirstNavPointWasProjectedStart` **PASS**. The first returned nav point **is** the query-start anchor and **would** be the first Tick target if left in `PathPoints`.

## Exact correction
`StripProjectedStartAnchor` / `FinalizeNavRuntimePath`:

- Keep `ProjectedStart` for `FindPathSync`.
- After a valid nav path is built, if `PathPoints.Num() > 1` and `PathPoints[0]` is within `max(AcceptanceRadius, 25)` of `ProjectedStart`, remove that query-start anchor.
- Do **not** drop a genuine first corner that is not the projected start.
- First runtime movement direction is from the current actor location toward the first meaningful forward waypoint.

Unchanged: obstacle routing, dest projection, acceptance radius, repath, serial/result, straight-line fallback, mine/haul paths. Facing still follows actual movement direction, not the click independently.

## Shortest-yaw helper
**Retained.** `ComputeShortestYawStep` still uses `FMath::FindDeltaAngleDegrees` + `FRotator::NormalizeAxis`. Not reverted.

Both invariants now hold:

1. Runtime path begins in the correct meaningful direction.
2. Rotation toward that direction uses shortest signed yaw.

## Tests actually run
| Command | Result |
| --- | --- |
| `gp.Movement.RunShortestYawContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | `Complete Failures=0 Cancelled=false` |

Combat / resource / economy: **NOT RUN**.

## Candidate build
`GPEditor Win64 Development` + UHT **PASS**.  
`GP` Win64 Development / Shipping: **NOT RUN**.

## Unrelated error (recorded only, not fixed)
`LogGPBuildGridRegister` `GP BuildingDefinitionLoadFailed`  
Building=`BP_GP_MainBase...`  
Path=`/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_MainBase...`  
Reason=`ResolveFailedUsingFallback`  

Separate DataAsset/load issue. **Out of scope for GP-S41M.**

## Merge
**NOT MERGED. NOT FINALIZED.** Await operator PIE.
