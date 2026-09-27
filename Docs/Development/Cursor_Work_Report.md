# Cursor Work Report

## Status

**STAGE_3A_FINAL_MERGE_CANDIDATE**

Stage 3A deformation foundation is complete. Stage 3B Worker terrain leveling is not implemented.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `cf47d24f65239e2ba97003095a18c7779f3ace3c` |
| Feature commit | `92ae64780c34689fd044ac8716f60c80f737be0d` |
| Engine | UE 5.8.3 |
| Ahead / behind `origin/terrain/gp-voxel-foundation` | 0 / 0 after push |

## Operator PASS

Authored `L_VoxelArena_2P`:

- Voxel crater works
- Collision updates
- Shallow radius/depth geometry works
- Units descend and climb the deformed surface

Two-player PIE, Play As Listen Server (2026-09-28):

- The authority debug command created the crater on the host
- The client received the replicated compact deformation event
- The client reconstructed the same crater automatically
- No duplicate application was observed
- Worker terrain-follow stayed correct

Late join is not claimed. Snapshot compaction is not claimed.

## Architecture

Owner: `UGP_TerrainDeformationComponent`, default subobject of `AGP_GameState`, replicated.

API: `RequestDeformation`. Authority only. Clients get `NoAuthority`. Gameplay headers do not expose `UVoxelSphereTools`. The private adapter is the Voxel seam.

Profile: `ShallowSphereCap` only. Center Z = `WorldLocation.Z + (RadiusCm - DepthCm)`. `RemoveSphere` uses `bConvertToVoxelSpace=true`, `bUpdateRender=true`, `bMultiThreaded=false`.

Validation: non-finite rejected; radius or depth ≤ 0 rejected; other profiles rejected; radius clamped to 50..1500 cm; depth clamped to 10..radius.

World resolution: vertical trace ±8000 cm. Closest hit that resolves to an `AVoxelWorld`. No world-actor scan.

## Replication

Replicated `TArray` of compact events, max 32, `OnRep` applies locally. No voxel payload.

`MaxEventHistory=32` is an intermediate reconstruction buffer. It is not a late-join solution. A match with more than 32 historical deformations does not keep the older events for a client that never applied them. Connected clients that already applied a trimmed event are unaffected. Snapshot compaction remains future network work.

Duplicate guard: a `SequenceId` is applied at most once per machine. The listen-server host applies inside `RequestDeformation` and does not apply that id again from the log. The operator saw no duplicate on the client.

`gp.Voxel.CraterUnderCursor` is the non-shipping debug producer of `RequestDeformation`.

## Cleanup

Removed:

- `GPVoxelRuntimeVisualProbe.cpp` (`gp.Voxel.SpawnRuntimeProbe`, `gp.Voxel.ApplyProbeCrater`). The runtime crater contract and the authored command already cover that path.
- `GPNavAndWorkerDump.cpp` (`gp.Nav.Dump`). The `0x20` lock was already identified as `AsyncLoadLock`.

Kept:

- Production terrain component, types, and GameState ownership
- Voxel adapter seam
- `gp.Terrain.RunDeformationContractTest`
- `gp.Voxel.RunPluginCompileProbeContractTest`
- `gp.Voxel.RunRuntimeCraterProbeContractTest`
- `gp.Voxel.RunCameraInvokerContractTest`
- `gp.Voxel.CraterUnderCursor`
- `gp.Voxel.FillUnderCursor` (direct add spike, not a second crater path)
- `gp.Movement.DumpGroundFollow`

## Deferred (does not block 3A)

- Irregular crater profiles
- Material scars
- Niagara debris
- Vegetation reaction
- Foundation destruction
- Dynamic traversability (Stage 3E)
- World FoW terrain-surface adaptation (Stage 3E)
- Late-join snapshot / compaction
- Worker leveling (Stage 3B, not started)

## Tests

All **Failures=0** on `L_PrototypeArena`, `-game -NullRHI`, 2026-09-27 UTC:

- `gp.Terrain.RunDeformationContractTest` 23.01.05
- `gp.Voxel.RunPluginCompileProbeContractTest` 23.01.14
- `gp.Voxel.RunRuntimeCraterProbeContractTest` 23.01.23
- `gp.Voxel.RunCameraInvokerContractTest` 23.01.32
- `gp.Movement.RunGroundFollowContractTest` 23.01.41
- `gp.Movement.RunRTSMovementReconciliationContractTest` 23.02.04
- `gp.Worker.RunCommandIntentContractTest` 23.02.15
- `gp.Mining.RunContractTest` 23.02.25
- `gp.Match.RunWinLoseContractTest` 23.02.34

Minimap and world-FoW contracts were not re-run. This checkpoint does not change camera presentation, FoW, or minimap code. The voxel camera-invoker contract was run.

## Builds

- GPEditor Win64 Development + UHT: **succeeded** (exit 0)
- GP Win64 Development: **succeeded** (`GP\Binaries\Win64\GP.exe`, exit 0)
- GP Win64 Shipping: **succeeded** (`GP\Binaries\Win64\GP-Win64-Shipping.exe`, exit 0)

## Files changed

- `GP/Source/GPRuntime/Public/Terrain/GPTerrainDeformationComponent.h`
- `GP/Source/GPRuntime/Private/Debug/GPVoxelRuntimeVisualProbe.cpp` (deleted)
- `GP/Source/GPRuntime/Private/Debug/GPNavAndWorkerDump.cpp` (deleted)
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not staged: `GP/Content/` (including `L_VoxelArena_2P` and `L_PrototypeArena`), `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/VoxelFree/`, `Tools/`. The plugin stays operator-local and untracked.
