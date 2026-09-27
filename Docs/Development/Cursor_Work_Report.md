# Cursor Work Report

## Status

**SHALLOW_CRATER_AND_TERRAIN_FOLLOW_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A is not complete. Dynamic traversability and NavMesh updates are still Stage 3E. No Content, map, Config, uproject, or plugin file was saved or committed.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `5b16dbd0b8aac7d7a9aecc8faa038f34beb67ed3` |
| Checkpoint commit | `da87845051597ce43792fdb8d900f7daa621309a` |
| Engine | UE 5.8.3 |

## Crater

Operator PIE on `L_VoxelArena_2P` confirmed authored deformation and collision. The previous debug sphere was centered on the click, so the hole read as a hemisphere.

`gp.Voxel.CraterUnderCursor [RadiusCm] [DepthCm]`

| | |
| --- | --- |
| Default radius | 400 cm |
| Default depth | 80 cm |
| Radius clamp | 50..1500 cm |
| Depth clamp | 10 cm .. radius |

Edit center, world centimeters:

`EditCenter.Z = ImpactPoint.Z - (RadiusCm - DepthCm)`

X and Y stay on the impact point. Radius 400 and depth 80 put the center 320 cm below the click. `RemoveSphere` is unchanged: `bMultiThreaded=false`, `bConvertToVoxelSpace=true`, `bUpdateRender=true`. The debug sphere is drawn at that edit center. The vertical collision trace still uses the click column.

`gp.Voxel.FillUnderCursor` still calls `AddSphere` at the impact point. It does not apply the depth offset.

The crater stays in PIE memory. The map is not saved. Recast is not rebuilt.

## Movement

Previous tick kept path-point Z at the owner Z, forced `Step.Z = 0`, and wrote `NextLocation.Z` from the current actor Z. XY came from the nav or straight path. The unit kept its old height over a crater.

XY routing is unchanged. Path-point Z is still the owner Z and is not a second ground projection.

Each authority movement tick:

1. Candidate XY is the same swept step as before.
2. A complex object trace runs from `ReferenceZ + 500` cm to `ReferenceZ - 1000` cm through that XY.
3. Object types are `ECC_WorldStatic` and `ECC_WorldDynamic`. The owner is ignored. Hits whose actor is `AGP_UnitBase` are skipped, which covers units and buildings.
4. Desired Z is `SurfaceZ + support`. A capsule root uses scaled half-height, because `AGP_Unit` / `AGP_Worker` put the actor origin at the capsule center. A box root uses scaled extent Z.
5. Z moves toward that height at 1200 cm/s. A gap of 2 cm or less stays put. A miss, or a non-finite result, keeps the current Z and does not cancel the move.
6. `SetActorLocation` is still swept. Progress and arrival stay 2D.

`gp.Movement.DumpGroundFollow` logs the last sample per movement component. Non-shipping only.

Not in this checkpoint: slope rejection, tank versus infantry, BuildGrid traversability, per-crater NavMesh, path-cost grid.

## Niagara cleanup

`GPWorkerMiningVFXDump.cpp` and the non-shipping Niagara module plus internal include are removed. The mining presentation contract still requires one effect event on enter, none on a repeat `BeginMining`, and one on leave.

## Operator retest

1. PIE `L_VoxelArena_2P`
2. `gp.Voxel.CraterUnderCursor`
3. The hole should be wide and shallow, not a hemisphere
4. Order a Worker across it
5. The Worker should descend and climb instead of holding the old Z
6. Repeat with `gp.Voxel.CraterUnderCursor 600 100`

## Tests / build

| Step | Result |
| --- | --- |
| GPEditor Win64 Development | Succeeded |
| `gp.Voxel.RunPluginCompileProbeContractTest` | Failures=0 (2026.09.27-19.24.02) |
| `gp.Voxel.RunCameraInvokerContractTest` | Failures=0 (2026.09.27-19.24.02) |
| `gp.Movement.RunGroundFollowContractTest` | Failures=0 (2026.09.27-19.24.02) |
| `gp.Resource.RunPresentationContractTest` | Failures=0 (2026.09.27-19.24.02) |
| `gp.Voxel.RunRuntimeCraterProbeContractTest` | Failures=0 (2026.09.27-19.24.29) |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | Failures=0 (2026.09.27-19.25.02) |
| `gp.Worker.RunCommandIntentContractTest` | Failures=0 (2026.09.27-19.25.22) |
| `gp.Mining.RunContractTest` | Failures=0 (2026.09.27-19.25.46) |

Ground-follow cases: radius 400 / depth 80 offset -320; depth clamps to radius; flat floor keeps support Z; lower floor drops Z while XY continues; return climb; a missed trace keeps Z and does not NaN or cancel the move.

## Files changed

- `GP/Source/GPRuntime/Private/Debug/GPVoxelAuthoredCraterProbe.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPMovementGroundFollowContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPWorkerMiningVFXDump.cpp` (removed)
- `GP/Source/GPRuntime/Private/Units/GPMovementComponent.cpp`
- `GP/Source/GPRuntime/Private/Voxel/GPVoxelRuntimeProbeAdapter.h`
- `GP/Source/GPRuntime/Public/Units/GPMovementComponent.h`
- `GP/Source/GPRuntime/GPRuntime.Build.cs`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not modified and not committed: `GP/Content/`, `L_VoxelArena_2P`, `L_PrototypeArena`, `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.
