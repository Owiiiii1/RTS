# Cursor Work Report

Status: **GP-S36G_FOOTPRINT_OFFSET_TRANSFORM_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`PENDING_IMPLEMENTATION`

## Operator symptom
Size authoring works. MainBase `PlacementFootprintBounds` can be shifted in Blueprint relative to Capsule/root, but PIE occupied/red cells stay centered as if the authored offset were ignored.

## Factual raw-local-as-world bug
`TryRegisterWithBuildGrid` did:

`ActorLocation + (Resolved.LocalCenterOffsetCm.X, Y, 0)`

`LocalCenterOffsetCm` is `PlacementFootprintBounds->GetRelativeLocation()` — component-local / root-relative, not world. Raw world-axis addition is wrong whenever the building/root has rotation, and does not represent the authored box center.

## Exact local→world transform rule
Shared helpers on `UGP_BuildGridSubsystem`:

- `TransformFootprintLocalOffsetToWorld(LocalXY, ActorRotation)`
- `MakeWorldFootprintCenter(ActorLocation, ActorRotation, LocalXY)`
- `MakeActorLocationFromFootprintCenter(Center, LocalXY, ActorRotation)` (inverse)

Implementation: `FTransform(Rotation, 0, Scale=1).TransformVectorNoScale(Local)`.

World footprint center = live `ActorLocation` + that world offset. Then snap the center to BuildGrid (200 cm).

Used by pre-placed registration, preview actor-location reconstruction, and DropPod landing.

## Rotation handling
Footprint **size** stays world-axis-aligned (no rotated cells).

Footprint **center offset** follows actor/root orientation:

- local +400 X, yaw 0 → world +400 X
- local +400 X, yaw 90 → world +400 Y
- local +400 X, yaw 180 → world −400 X

## Actor scale policy
Same as size: actor/world scale does **not** inflate cells and does **not** multiply the center offset. Rotation only; scale forced to 1 on the helper transform.

## Grid quantization
CellSize remains 200 cm. Occupancy is snapped to cells. Offsets that stay inside the same snap bucket can resolve to the same origin. Shifts of ≥ one cell (tests use 400 cm) move registered cells.

## CDO + live actor transform
Unchanged source policy: net-startup/pre-placed SIZE and local OFFSET come from Blueprint class CDO. That CDO local XY is then transformed by the **live** actor location/rotation.

## Tests
`gp.Building.RunBuildGridContractTest` covers A–L (yaw 0/90/180, axis-aligned AABB, actor scale does not change size or offset, origin from transformed center, occupancy, freed opposite cells, Hub edge reject / adjacent free). Spawned Hub preview/reservation/occupancy unchanged.

All listed regressions Failures=0.

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development / Shipping **not run**.

## Exact changed files
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

## Operator retest
1. MainBase BP: shift `PlacementFootprintBounds` ≥ 300–400 cm to one side.
2. Save.
3. PIE → Purchase Hub → Deploy preview around MainBase.

Expected: red cells shift toward the authored box; opposite side frees; 200 cm grid; authored size unchanged.

**NOT MERGED.**  
**NOT FINALIZED.**
