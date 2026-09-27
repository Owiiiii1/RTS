# Cursor Work Report

## Status

**SHALLOW_CRATER_DEPTH_GEOMETRY_FIXED**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A is not complete. Dynamic traversability and NavMesh updates remain Stage 3E. No Content, map, Config, uproject, or plugin file was saved or committed. Movement code was not changed.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `19adbadf3577c23edabeb9838717f94851834241` |
| Engine | UE 5.8.3 |

## Geometry

Solid terrain is below the click. A sphere centered under the surface carves almost its whole volume downward, so the hole is far deeper than `Depth`.

`gp.Voxel.CraterUnderCursor` now places the subtract sphere above the original surface:

`EditCenter.Z = ImpactPoint.Z + (RadiusCm - DepthCm)`

X and Y stay on the impact point. `Depth` is the lower cap's penetration under that surface. `RemoveSphere` flags are unchanged: `bMultiThreaded=false`, `bConvertToVoxelSpace=true`, `bUpdateRender=true`. The debug sphere is drawn at `EditCenter`.

| Radius | Depth | Center Z offset | Sphere bottom vs surface |
| --- | --- | --- | --- |
| 400 | 80 | +320 | -80 |
| 600 | 100 | +500 | -100 |
| 400 | 400 | 0 | hemisphere |

Depth still clamps to 10 cm through the radius. Radius still clamps to 50..1500 cm. Defaults remain 400 and 80.

The log prints `CenterZOffset=+320` and `ExpectedMaxDepth=80`.

`FillUnderCursor` still adds a sphere at the impact point.

## Operator retest

PIE `L_VoxelArena_2P`.

`gp.Voxel.CraterUnderCursor 400 80`

Expected: a wide shallow depression, maximum depth about 80 cm, not a deep sphere pit.

Then `gp.Voxel.CraterUnderCursor 600 100`

Expected maximum depth about 100 cm.

## Tests / build

| Step | Result |
| --- | --- |
| GPEditor Win64 Development | Succeeded |
| `gp.Movement.RunGroundFollowContractTest` | Failures=0 (2026.09.27-22.04.52), including offset +320, bottom -80, offset +500, bottom -100, depth clamp offset 0 |
| `gp.Voxel.RunRuntimeCraterProbeContractTest` | Failures=0 Cancelled=false (2026.09.27-22.05.17) |

## Files changed

- `GP/Source/GPRuntime/Private/Voxel/GPVoxelRuntimeProbeAdapter.h`
- `GP/Source/GPRuntime/Private/Debug/GPVoxelAuthoredCraterProbe.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPMovementGroundFollowContractTest.cpp`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not modified and not committed: `GP/Content/`, maps, `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.
