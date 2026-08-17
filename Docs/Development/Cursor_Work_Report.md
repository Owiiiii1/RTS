# Cursor Work Report

Status: **GP-S36G_ORIENTED_FOOTPRINT_OCCUPANCY_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`97debf278a642c2cabe819dca2ad9d76383b792a`

## Operator failure after axis-alignment
`SetAbsolute(false, true, true)` plus forced `RelativeRotation = 0` kept the cyan box world-axis-aligned. Rotating the level MainBase left the footprint behind. That is wrong for gameplay authoring.

## Why absolute rotation was wrong
BuildGrid occupancy can be an oriented cell set. The visible box must stay attached to Capsule/root (location **and** rotation). Forcing world-zero rotation only hid orientation instead of occupying the rotated footprint.

## Final component attachment
`PlacementFootprintBounds->SetAbsolute(false, false, true);`

- location follows building
- rotation follows building
- parent/root scale does not change gameplay size
- own BoxExtent / RelativeLocation / RelativeRotation / RelativeScale3D are design data
- CDO→live copies all four; RelativeRotation is **not** zeroed

## Scale-only isolation
`bounds component world scale == own RelativeScale3D`, not parent × own. Operator Capsule/root ~2.352 does not enlarge cells.

## Oriented cell-set occupancy
Authoritative occupancy is `TArray<FIntPoint>` per `OccupantId`.

- `RegisterCells` stores that set
- `RegisterFootprint` remains a rectangle wrapper (`EnumerateFootprintCells` → `RegisterCells`)
- `UnregisterOccupant` removes exactly those cells

Pre-placed / unconfigured buildings: `ResolveOccupiedCellsFromBounds` → `RegisterCells`.  
`GridOriginCell` / `GridFootprintSize` are the occupied-set AABB (legacy/debug), **not** the SoT for rotated actors.

Yaw-0 orbital deploys still use `ConfigureGridPlacement` + rectangular reserve/register.

## OBB / cell policy
Shared `ResolveOccupiedCellsFromBounds`:

- center = `Bounds->GetComponentLocation()`
- yaw = `Bounds->GetComponentRotation().Yaw`
- half = `|UnscaledBoxExtent × RelativeScale3D|`
- CellSize 200

Yaw ~0° / 180° (±0.5°): snapped `SizeCells` rectangle — same cells as the old Origin+Size path.

Any other yaw: world AABB of the OBB → candidate cells → 2D SAT (world X/Y + OBB axes). A cell is occupied when projected overlap exceeds `OccupancyOverlapEpsilonCm = 1.0`. Exact edge touch does not occupy. No physics queries. No SizeCells X/Y swap.

## Tests
Yaw 0 = old 10×4. Yaw 90 / 270 = tall oriented coverage, not an unswapped 10×4. Yaw 91.7: footprint yaw follows actor. Yaw 45: stair-step. Rotate-then-resolve changes cells. Offset follows live center. Parent 2.352 does not inflate. Own scale still authors size. Hub overlap of an occupied rotated cell rejects. Destroy releases all oriented cells. Spawned yaw-0 Hub path unchanged.

`gp.Building.RunBuildGridContractTest` Failures=0.

All listed regressions Failures=0.

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development / Shipping **not run**.

## Exact changed files
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

**NOT MERGED.**  
**NOT FINALIZED.**
