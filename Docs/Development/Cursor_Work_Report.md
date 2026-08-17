# Cursor Work Report

Status: **GP-S36G_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`0acd4930a43b11d7065b56fea75781dec3f12e2d`

## BuildGrid class / API
`UGP_BuildGridSubsystem : UWorldSubsystem` in `GPRuntime` (`Buildings/Grid/`).

Core API: `GetCellSize`, `WorldToCell`, `CellToWorld`, `SnapOriginCell`, `GetFootprintCenterWorld`, `EnumerateFootprintCells`, `ResolveSnappedPlacement`, `CanPlaceFootprint`, `RegisterFootprint`, `UnregisterFootprint` / `UnregisterOccupant`, `IsCellOccupied`, `GetActorAtCell`, `TryReserveFootprint`, `BindReservationOwner`, `PromoteReservationToBuilding`, `ReleaseReservation`, `IsFootprintNavigable`, `IsFootprintEnvironmentBlocked`.

Subsystem is **not** replicated.

## Cell size
`200.0f` cm (`UGP_BuildGridSubsystem::DefaultCellSizeCm`).

## Grid origin semantics
`GridOriginXY = (0,0)`. No per-frame NavMesh origin projection and no map-definition asset. Z is taken from the cursor/ground trace, not encoded in `FIntPoint`.

## WorldToCell rules
Per axis: `Floor(WorldCoord / 200 + 0.5)`. Cell centers sit at `n * 200`. Half-open Voronoi: `+100` belongs to the next positive cell; `-100` belongs to cell 0. Negative coordinates use the same Floor rule (no trunc-toward-zero bug). Roundtrip `Cell → World → Cell` holds for integer cells.

## OriginCell semantics
Min/anchor cell of an axis-aligned footprint. Cells occupy `Origin.X .. Origin.X+Width-1` and `Origin.Y .. Origin.Y+Height-1`.

## Footprint center math
World XY = `Origin * 200 + (Size - 1) * 100`. Even footprints (4×4) center between cells (e.g. Origin `(0,0)` → center `(300,300)`).

## Occupancy representation
Server-only:
- `TMap<FIntPoint, FGP_GridCellRecord>` (`FGuid OccupantId`, weak actor, `bIsReservation`)
- `TMap<FGuid, TArray<FIntPoint>>` reverse lookup

Identity is `FGuid`, not a raw pointer address. Stale weak actors are swept and cannot permanently hold cells.

## Reservation model for in-flight pods
After deploy is accepted: `FGuid` reservation covers the exact footprint. Bound to the DropPod. Payload spawn promotes reservation → building occupancy with no gap. Pod EndPlay / `DebugForceSkipPayloadSpawn` / failed spawn releases the reservation. Purchase does **not** reserve cells. Two accepted in-flight deploys cannot overlap.

## Pre-placed MainBase handling
Compatibility fallback footprint **5×5** derived from actor location at BeginPlay. No BuildingDefinition content asset required. Occupancy blocks overlapping Hub drops.

## Ferronite Deposit handling
`AGP_ResourceNode` is an `AActor`, **not** `AGP_BuildingBase`. GP-S36G does **not** register a 3×3 grid footprint. Existing environmental WorldStatic/WorldDynamic overlap still blocks placing a Hub through deposit collision.

## Server validation sequence
DropDef → BuildingDef → FootprintCells > 0 → payload class → finite transform → MainBase → **server snap** → max deploy radius on snapped XY → cells free/unreserved → NavMesh MVP → environmental overlap → reserve → spawn pod at snapped ground / yaw 0 → consume READY once → init pod with OriginCell / Footprint / ReservationId.

Reject reasons: `GridOccupied`, `InvalidFootprint`, `NotNavigable` (plus existing radius / READY / definition reasons). World geometry uses `PlacementOverlap`. Client OriginCell is not trusted.

## Client ghost snap behavior
Ground trace → shared `ResolveSnappedPlacement` (no second formula in PlayerController) → ghost at snapped center. Engine cube scaled to `FootprintCells * 200 cm`. Local occupancy/nav/world query tints green/red; server remains authority. Confirm still sends a transform intent; server re-snaps.

## Rotation policy
No rotation. Camera yaw ignored. Pod/building yaw canonical `0`.

## NavMesh validation rule
Project footprint **center** with extent `(100, 100, 300)`. Success → navigable. Fail + WorldStatic ground hit → `NotNavigable`. Fail + empty void → allow (isolated contract locations). Runs before the new building's NavigationObstacle exists.

## World collision rule
Raised footprint box vs WorldStatic/WorldDynamic, ignoring buildings / pods / pawns. Environmental sanity only. Structure-vs-structure SoT is grid occupancy.

## FoW
`FoW placement validation deferred to FoW integration slice.`

## Walls
Explicitly deferred. No `AGP_Wall`, connection component, 8-dir neighbors, drag, A*, wall-mounted turret, mount slots, wall-specific clearance, sequential wall pods.

## READY exact-once preservation
Invalid grid placement does not consume READY and does not spend Orbital again. Accepted deploy consumes READY exactly once after reservation + pod spawn.

## Logistics Hub +5 preservation
Unchanged native `AGP_LogisticsHub` apply when live/operational; remove on destroy.

## NavigationObstacle preservation
Unchanged authored `UBoxComponent` / `NavArea_Null` dynamic obstacle. BuildGrid is not a Recast replacement.

## Tests (all Failures=0)
- `gp.Building.RunBuildGridContractTest`
- `gp.Building.RunMultiBuildingDataContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest`
- `gp.Resource.RunUnitCapLogisticsHubContractTest`
- `gp.Resource.RunOrbitalUnitDropContractTest`
- `gp.Movement.RunRTSMovementReconciliationContractTest`
- `gp.Match.RunWinLoseContractTest`
- `gp.Resource.RunContainerLaunchContractTest`
- `gp.Resource.RunContainerLaunchHUDContractTest`
- `gp.Resource.RunS28RegressionSuite`
- `gp.Combat.RunAttackMoveContractTest`
- `gp.Combat.RunAutoAcquireContractTest`

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development / Shipping **not run** (candidate stage).

## Exact files changed during this slice
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropAuthority.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPDropPod.h`
- `GP/Source/GPRuntime/Private/Orbital/GPDropPod.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingPlacementGhost.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingPlacementGhost.cpp`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildGridContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalBuildingDropContractTest.cpp`
- `Docs/Development/Claude_Tasks/GP-S36G_BuildGrid_MVP.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/Development/Cursor_Work_Report.md`

Not committed (operator-local): `DefaultEngine.ini`, `DefaultGame.ini`, `L_PrototypeArena.umap`, `BP_ResourceNode_AuthoredExample.uasset`, Blueprint/Materials/VFX packs, `Tools/`, AutoAcquire CRLF noise.

## Explicit
**NOT MERGED.**
