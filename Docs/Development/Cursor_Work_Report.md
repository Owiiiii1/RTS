# Cursor Work Report

## Status

**AUTHORED_VOXELWORLD_DEFORMATION_PROBE_READY**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A is not complete. The crater exists only in PIE/runtime memory. No Content, map, Config, uproject, or plugin file was saved or committed.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `a3df2ec8fce382decab2446df412b2183d269bcc` |
| Engine | UE 5.8.3 |

## Command

`gp.Voxel.CraterUnderCursor`

Optional radius, centimeters:

`gp.Voxel.CraterUnderCursor 500`

Default radius is **300**. Values are clamped to **50..1500**. The command does not spawn a VoxelWorld, does not call `CreateWorld`, and does not save the map.

Optional additive twin, same targeting: `gp.Voxel.FillUnderCursor` (`AddSphere`).

This is an operator debug probe. It is not the multiplayer deformation architecture. A client world is refused. Standalone and listen-server apply the edit on that process's traced `AVoxelWorld`.

## Target resolution

Cursor ray from the first local `PlayerController` (`GetMousePosition` + `DeprojectScreenPositionToWorld`). Trace length 1,000,000 cm. Complex traces on `ECC_Visibility`, `ECC_WorldStatic`, and `ECC_WorldDynamic`. The camera pawn is ignored. The closest blocking hit is the only candidate.

`UVoxelProceduralMeshComponent` is created with outer `UVoxelWorldRootComponent` (`NewObject` in `VoxelRendererMeshHandler::GetNewMesh`) and registered on that actor. `Hit.GetActor()` is therefore that `AVoxelWorld`. The resolver still walks, in order:

1. Hit actor is `AVoxelWorld` (`HitActor`)
2. Hit component `GetTypedOuter<AVoxelWorld>()` (`ComponentOuter`)
3. Attach-parent owner / outer (`AttachParent`)
4. Hit actor outer (`ActorOuter`)

It does not iterate `TActorIterator<AVoxelWorld>`. A non-voxel hit logs the actor and component and does nothing. A world that is not created is refused.

## Edit

Center is the hit `ImpactPoint`.

`UVoxelSphereTools::RemoveSphere` through the private adapter:

| Argument | Value |
| --- | --- |
| Position | world cm (`ImpactPoint`) |
| Radius | clamped cm |
| `bMultiThreaded` | false |
| `bConvertToVoxelSpace` | true |
| `bUpdateRender` | true |

`FillUnderCursor` uses `AddSphere` with the same flags.

Log line includes VoxelWorld name, resolve path, hit component, impact, requested and clamped radius, voxel size, and edited bounds.

## Collision check

Before the edit, a vertical trace runs through the center from **+8000 cm** to **-8000 cm**, limited to the resolved VoxelWorld (`LineTraceHitsProbe`, including `ECC_WorldStatic`).

`BeforeSurfaceZ` is that hit. If the vertical trace misses, the click Z is recorded and a later miss is not treated as a hole.

A timer fires after **0.75 s**. The same vertical trace runs again. There is no immediate PASS.

PASS when:

- the baseline hit and the after trace misses (hole), or
- the after surface is lower by at least `max(50 cm, 0.5 * voxel size)`

The log prints `BeforeSurfaceZ`, `AfterSurfaceZ`, and `DeltaZ`. On screen: `CRATER APPLIED`, then `COLLISION UPDATED` or `collision delta`. A debug sphere marks the edit. The vertical line is drawn before and after.

Dynamic Recast is not rebuilt. The log says navmesh is deferred Stage 3E.

## Operator retest

1. PIE `L_VoxelArena_2P`
2. Point the mouse at empty voxel ground
3. Run `gp.Voxel.CraterUnderCursor`
4. Inspect the crater
5. Walk, click, or trace across the hole
6. Report whether geometry and collision changed

## Tests / build

| Step | Result |
| --- | --- |
| GPEditor Win64 Development | Succeeded |
| `gp.Voxel.RunPluginCompileProbeContractTest` | Failures=0 (2026.09.27-18.56.21) |
| `gp.Voxel.RunCameraInvokerContractTest` | Failures=0 (2026.09.27-18.56.21) |
| `gp.Voxel.RunRuntimeCraterProbeContractTest` | Failures=0 Cancelled=false (2026.09.27-18.57.04) |

No automated edit of `L_VoxelArena_2P`.

## Files changed

- `GP/Source/GPRuntime/Private/Debug/GPVoxelAuthoredCraterProbe.cpp`
- `GP/Source/GPRuntime/Private/Voxel/GPVoxelRuntimeProbeAdapter.h`
- `GP/Source/GPRuntime/Private/Voxel/GPVoxelRuntimeProbeAdapter.cpp`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not modified and not committed: `GP/Content/`, `L_VoxelArena_2P`, `L_PrototypeArena`, `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.
