# Cursor Work Report

## Status

**STAGE_3B1_LEVELING_PLANNING_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A stays complete. Stage 3B is not complete. This slice does not level terrain.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-worker-leveling` |
| Base `origin/main` | `98cc07592eb7bd52e0eea89460ca7646ac723f13` |
| Feature commit | recorded in the follow-up commit |
| Engine | UE 5.8.3 |

## Owner

`UGP_EngineeringJobSubsystem` (`UWorldSubsystem`), same lifetime pattern as `UGP_BuildGridSubsystem`.

Jobs are world-scoped. They are not attached to a Worker and they are not `GP.Command.Build`. Authority is the server world, and a simulated `GameState` is rejected. The store is not replicated. The job struct is the domain record a later client mirror can copy.

## Job data

`CreateTerrainLevelingJob(OriginCell, SizeCells, TargetPlaneZ)`.

- `JobId` is an `FGuid`
- Type is `TerrainLeveling` only
- State is `Planned`. `Completed` exists and nothing sets it
- Rectangle comes from `UGP_BuildGridSubsystem::EnumerateFootprintCells`
- Size must be 1..32 on each axis
- `TargetPlaneZ` is stored as given. The service does not average or median the terrain
- Each cell stores state, mean / min / max sample Z, max absolute deviation, and sample count
- Find, enumerate, and cancel are available. Cancel does not deform terrain
- Zero Workers is the only mode. Assignment, progress, work positions, and pulses are not fields yet

An all-`Level` zone is rejected with `NothingToLevel` and is not stored.

## Sampling

Five traces per cell: center and four points inset by 25% of the BuildGrid cell. Traces run ±8000 cm through `WorldStatic`, `WorldDynamic`, and `Visibility`. `AGP_UnitBase` hits are skipped, which also skips buildings. A hit counts only when it resolves to a VoxelWorld. NavMesh Z is not used.

The center sample must hit. A cell whose center misses is `NoTerrain`, and the plan is rejected. Quadrant hits that exist participate in the deviation test.

## Classification

`LevelHeightToleranceCm` is **15**. It is a named tunable, not a final balance value.

`NeedsLeveling` when any hit sample is more than 15 cm from `TargetPlaneZ`. Otherwise `Level`.

## Debug

`gp.Engineering.PlanLevelingUnderCursor [WidthCells] [HeightCells]`, defaults `4 4`.

Authority viewport only. The cursor must hit voxel terrain. `ImpactPoint.Z` is `TargetPlaneZ`. The zone is snapped with `SnapOriginCell`. The command logs the job id, origin, size, plane, level count, needs-leveling count, and reject reason. It draws grey and yellow debug boxes for 8 seconds. No tick. No content assets.

`gp.Engineering.DumpJobs` lists stored plans.

## Tests / build

`gp.Engineering.RunLevelingPlanningContractTest` at 2026.09.28-17.44.08: **Failures=0**.

- A. 2×3 BuildGrid enumeration
- B. Authority create
- C. Simulated proxy rejected
- D. Zero size and 33-wide size rejected
- E. Explicit `TargetPlaneZ` stored unchanged
- F / H. Flat plane is all `Level` and `NothingToLevel`
- G. Shallow crater cell becomes `NeedsLeveling` after collision updates
- I. Stored job can be found, listed, and cancelled
- J. Planning does not append a terrain deformation event

Regressions, all **Failures=0** on the same editor build:

- `gp.Building.RunBuildGridContractTest` 17.40.55
- `gp.Terrain.RunDeformationContractTest` 17.41.04
- `gp.Voxel.RunRuntimeCraterProbeContractTest` 17.41.14
- `gp.Movement.RunGroundFollowContractTest` 17.41.22
- `gp.Worker.RunCommandIntentContractTest` 17.41.32

Build: **GPEditor Win64 Development succeeded**. GP Development and Shipping were not run.

## Still unresolved in 3B

- Player UX for choosing `TargetPlaneZ`
- Drag rectangle versus a fixed-size plan
- Worker assignment and multi-worker scaling
- Work positions
- Leveling speed
- Cut / fill deformation
- Interrupt / resume
- Niagara work presentation
- Foundation and Wall jobs

## Operator test

1. PIE `L_VoxelArena_2P` on the authority viewport.
2. On flat ground run `gp.Engineering.PlanLevelingUnderCursor 4 4`.
3. Expect grey cells, or `NothingToLevel` when the patch is already within 15 cm.
4. Run `gp.Voxel.CraterUnderCursor 400 80`.
5. Point into and around the crater and run `gp.Engineering.PlanLevelingUnderCursor 4 4`.
6. Crater cells should be yellow (`NeedsLeveling`). Surrounding flat cells should be grey (`Level`).
7. The terrain shape does not change because of the plan.

## Files changed

- `GP/Source/GPRuntime/Public/Engineering/GPEngineeringJobTypes.h`
- `GP/Source/GPRuntime/Public/Engineering/GPEngineeringJobSubsystem.h`
- `GP/Source/GPRuntime/Private/Engineering/GPEngineeringJobSubsystem.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPEngineeringLevelingPlan.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPEngineeringLevelingPlanningContractTest.cpp`
- `Docs/GDD/13_Terrain_Engineering_And_Foundations.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not staged: `GP/Content/` (including `L_VoxelArena_2P` and `L_PrototypeArena`), `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/VoxelFree/`, `Tools/`. The plugin stays untracked.
