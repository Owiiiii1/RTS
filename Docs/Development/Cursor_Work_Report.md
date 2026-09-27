# Cursor Work Report

## Status

**VOXEL_CAMERA_INVOKER_INTEGRATION_PROVEN**

**INTERMEDIATE / NOT MERGE READY**

`AGP_CameraPawn` now owns one local-gated `UVoxelSimpleInvokerComponent`. Runtime crater contracts still pass. Stage 3A is not complete. Do not start 3B. Do not vendor `GP/Plugins/VoxelFree`.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Remote | `origin/terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Merge-base with `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent before this checkpoint | `fd8556b91b248ef5ba9dcea138ef9dfd7cf8c37f` |
| Checkpoint commit | see git HEAD after push |

No rebase, reset, stash, or clean. Operator dirty/untracked preserved. Engine on this machine is UE **5.8.3** (changelist `58210709`, CompatibleChangelist `55116800`).

## Exact component type / header

`UVoxelSimpleInvokerComponent` in `VoxelComponents/VoxelInvokerComponent.h`.

Default subobject name `VoxelInvoker` on `AGP_CameraPawn`, attached to `RootScene`. `VisibleAnywhere`, `BlueprintReadOnly`, category `GP|Voxel`. Forward-declared in the public header. `Voxel` stays a **PrivateDependency**.

## Exact configured ranges

| Field | Value |
| --- | --- |
| `LODRange` | 20000 cm |
| `CollisionsRange` | 20000 cm |
| `bUseForLOD` | true |
| `LODToSet` | 0 |
| `bUseForCollisions` | true |
| `bUseForNavmesh` | false |
| `NavmeshRange` | 0 |

Camera movement, zoom, rotation, bounds, minimap callbacks, replication, and input were not changed.

## Plugin registration behavior

`UVoxelInvokerComponentBase::OnRegister` enables the component when `bStartsEnabled` (default true) and inserts it into a per-world list. `IsLocalInvoker()` returns `!Pawn || Pawn->IsLocallyControlled()`. The LOD manager applies that only to `bUseForLOD`. Collision, navmesh, and priority still follow every registered invoker.

`AGP_CameraPawn::SyncVoxelInvokerLocalActivation` runs from `BeginPlay`, `PossessedBy`, `UnPossessed`, and `OnRep_Controller`. It enables the invoker only when `IsLocallyControlled()`, and disables it otherwise. Dedicated-server pawns stay disabled. Non-local replicated camera pawns do not keep collision ranges active.

## Multiplayer observation

| Launch | Result |
| --- | --- |
| `L_PrototypeArena?listen` `-game -NullRHI` | `GP_GameMode`, `IpNetDriver` port 7777. `LogVoxel: Voxel Invoker enabled; Name: VoxelInvoker; Owner: GP_CameraPawn_0`. Contract `localPawns=1 remoteEnabled=0`. No fatal. No voxel world on this map, so the camera-fallback warning was not applicable. |
| `L_VoxelArena_2P?listen` `-game -NullRHI` | `VoxelWorld_1` generated in ~0.05s. Log game class was `GameModeBase`, so `AGP_CameraPawn` was not spawned. Warning string `Can't use camera as invoker in multiplayer` was **absent**. Map/game-mode override was not modified. |

Operator already turned off VoxelWorld **Use camera if no invokers found**. That setting is what emits the warning when the invoker list is empty outside Standalone.

## Tests / build

| Step | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **Succeeded** (full rebuild after 5.8.3 `Build.version`) |
| `gp.Voxel.RunPluginCompileProbeContractTest` | Failures=0 |
| `gp.Voxel.RunRuntimeCraterProbeContractTest` | Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Failures=0 |
| `gp.Voxel.RunCameraInvokerContractTest` | Failures=0 (CDO one invoker, ranges, nav off, local enabled) |
| Fatal / assertion | none |

## Files changed

- `GP/Source/GPRuntime/Public/Camera/GPCameraPawn.h`
- `GP/Source/GPRuntime/Private/Camera/GPCameraPawn.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPVoxelCameraInvokerContractTest.cpp`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- this report

## Protected audit

Not committed:

- `GP/Config/`, `GP/Content/` including `L_PrototypeArena` and `L_VoxelArena_2P`
- `GP/GP.uproject`
- `GP/Plugins/` (`VoxelFree`)
- `Tools/`

## Exact next action

Authored VoxelWorld crater-under-cursor validation in PIE. If that map stays on `GameModeBase`, `AGP_CameraPawn` will not spawn until the map uses `GP_GameMode`. Do not vendor the plugin. Do not start 3B.
