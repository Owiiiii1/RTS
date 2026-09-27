# Cursor Work Report

## Status

**VOXEL_MAP_NAV_WORKER_MINIMAP_DIAGNOSTICS_COMPLETE**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A is not complete. Stage 3B and dynamic crater navigation (3E) were not started. `L_VoxelArena_2P` was not modified.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `026c0457fc2f6eb738035976515ae08345c52df8` |
| Checkpoint commit | recorded in the following docs commit |

## A. NAVMESH

`0x20` is `ENavigationBuildLock::AsyncLoadLock` (`1 << 5`) in UE 5.8.3 `NavigationSystem.h`.

`UNavigationSystemV1::DoInitialSetup` adds it only in editor mode when `bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically` is true (engine default) and auto-update is enabled. `UpdateDelayedUnlockeRegistration` waits until `FAssetCompilingManager` has zero remaining assets. `RegisterDelayedUnlock` then waits 16 frames and 2 seconds, resets dirty areas, and calls `RemoveNavigationBuildLock(AsyncLoadLock, NoRebuild)`. `UNavigationSystemV1::Build` refuses while any lock other than `NoUpdateInEditor` is set and prints `flags: 0x%x`.

GP and VoxelFree do not set this flag. `bInitialBuildingLocked` is `InitialLock` (`1 << 3`), not `0x20`.

`UCrowdManager::CreateCrowdManager` logs `Unable to find RecastNavMesh instance` when no `ARecastNavMesh` is registered. GP movement does not use crowd following. With no main nav data, movement uses `NO_NAVDATA_STRAIGHT_FALLBACK`.

Recast can consume authored voxel terrain only from sections with `bEnableNavmesh`. `UVoxelProceduralMeshComponent::DoCustomNavigableGeometryExport` skips every other section. `AVoxelWorld::bEnableNavmesh` defaults **false**, so collision meshes alone are not nav geometry. Camera `bUseForNavmesh` stays false. Static baseline is world `bEnableNavmesh=true` plus `bComputeVisibleChunksNavmesh` (default true), then Build Paths after `AsyncLoadLock` clears. Runtime crater nav remains Stage 3E.

Operator fix, in the editor, on the authored Voxel World:

1. **Voxel - Navmesh → Enable Navmesh = true**. Leave **Compute Visible Chunks Navmesh** on.
2. Wait until asset compilation is idle, then about 2 seconds.
3. Build Paths.
4. `gp.Nav.Dump` should show `LockedAsyncLoad=false` and `RecastCount>=1`.

## B. WORKER

Cargo full calls `StartHaulReturnToBase`. That is not a stuck `CargoFull + Haul Idle` by itself.

Haul approach uses nav candidates. It falls back to a straight radial only for `NoNavSystem` and `PathStartProjectionFailed`. Other nav failures (`CandidateProjectionFailed`, path fail, partial, too long) enter `WaitingForDropOff` and do not straight-line. Manual `RequestMove` is different: no nav data, or start projection failure, is `NO_NAVDATA_STRAIGHT_FALLBACK` / `NAVDATA_EXISTS_START_PROJECTION_FAILED_STRAIGHT_FALLBACK`. A destination that does not project while the start does is `NAVDATA_EXISTS_DEST_PROJECTION_FAILED` and the move is **rejected** (`bRequireNavigationWhenAvailable`). The new held Move is then cleared. That looks like the click did nothing. It is an explicit reject, and mine/haul are already cleared.

`HandleCommand` for a non-queued ground Move sets the new held command, then `ResetMineExecutorForReplacement` (mine, haul, mining stop, drop-off timer). A ground Move does not retarget haul. `TryStartOneShotMainBaseDeposit` runs only when the Move's `TargetActor` is the friendly Main Base.

Regression on `L_PrototypeArena` (nav present): `gp.Worker.RunCommandIntentContractTest` Case F, `haulWas=true`, path mode `NAVDATA_EXISTS_PATH_OK`, `Failures=0`. Held becomes ground Move, haul/mine idle, movement serial matches. After 0.25 s haul stays clear.

The authored-map stall was not replayed in this process (`L_VoxelArena_2P` was not saved or launched as a mutating test). With no Recast, the same replacement path should straight-line. If the worker is still frozen, `gp.Worker.DumpState` during the stall is the next reading (`HaulState`, `PathMode`, cargo).

## C. MINIMAP

Canonical design is unchanged: world-axis fixed, not camera-rotated.

| World | Presenter | Slate surface (Y down) |
| --- | --- | --- |
| `+X` | normalized `+X` | **left** (`ScreenX = 1 - NormalizedX`) |
| `+Y` | normalized `+Y` | **top** (`ScreenY = 1 - NormalizedY`) |

Both flips are intentional. The Y flip puts world `+Y` at the top of a Y-down widget. The X flip matches the authored image. Together they are a mirror of a typical east-right map, not a 180° rotation (`+Y` would be bottom if it were 180°).

Background, FoW, blips, click-to-pan, and the camera footprint use that one transform. Footprint corners are the deprojected viewport, so camera yaw shows up as the polygon. A screenshot where the 3D view is not north-up is the expected difference. No footprint-opposite bug was found. No transform change.

`gp.UI.RunMinimapSurfaceContractTest` includes `N_WorldPlusXIsLeft` and `N_WorldPlusYIsTop`. `Failures=0`.

## Tests / build

| Step | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | Succeeded |
| `gp.Voxel.RunPluginCompileProbeContractTest` | Failures=0 |
| `gp.Voxel.RunRuntimeCraterProbeContractTest` | Failures=0 |
| `gp.Voxel.RunCameraInvokerContractTest` | Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | Failures=0 |
| `gp.UI.RunMinimapClickToPanFootprintSyncContractTest` | Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | Failures=0 |
| `gp.Worker.RunCommandIntentContractTest` | Failures=0 (Case F `NAVDATA_EXISTS_PATH_OK`) |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | Failures=0 |

## Files changed

- `GP/Source/GPRuntime/Public/Units/GPMovementComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPMovementComponent.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPNavAndWorkerDump.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPWorkerCommandIntentContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapSurfaceContractTest.cpp`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- this report

## Protected audit

Not committed: `GP/Content/` including `L_VoxelArena_2P` and `L_PrototypeArena`, `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.

## Exact operator retest

1. On the Voxel World, set **Enable Navmesh** true. Wait out asset compilation plus about 2 seconds. Build Paths.
2. PIE. `gp.Nav.Dump`. Expect `LockedAsyncLoad=false` and a Recast actor after that build.
3. If a worker stalls after full cargo, `gp.Worker.DumpState` and read `HaulState` and `PathMode` before issuing another click.
4. Minimap stays north-up. World `+Y` is the top edge, world `+X` is the left edge. Camera direction is the footprint. `gp.UI.MinimapOrientationDump` prints yaw and the surface points.
