# Cursor Work Report

## Status

**CAMERA_DEFAULT_YAW_CANONICALIZED**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A is not complete. Minimap XY transform was not changed. `L_VoxelArena_2P` was not modified.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `914809c65984e82c6cdee88801ed13aaf0f3c8c9` |
| Checkpoint commit | `ffb348f9025675894a0ea97f1e7397edf3c08568` |

## Old startup behavior

`AGP_CameraPawn::BeginPlay` set `CurrentYaw` from `RootScene->GetRelativeRotation().Yaw`. That relative yaw is the pawn spawn rotation, so an authored PlayerStart yaw became the RTS camera yaw. The minimap stayed world-fixed and the footprint matched that rotated view, which looked like a 90° minimap error.

## New startup behavior

`BeginPlay` calls `ApplyCanonicalStartupYaw` before the first bounds and presentation notifies. `CurrentYaw` is `FMath::UnwindDegrees` of the config yaw, and `RootScene` relative rotation is `(0, CurrentYaw, 0)`. PlayerStart still supplies spawn position. Its rotation is overwritten.

`DefaultYaw` lives on `UGP_CameraConfigDataAsset`, category `GP|Camera|Rotation`, units degrees, C++ default **90**.

If `ConfigRef` is already resident, that asset's `DefaultYaw` is the source. Otherwise the active config at `BeginPlay` is used, which is the CDO until async load.

## Async config

`HandleConfigLoaded` still remaps arm length from the old zoom fraction and does not write yaw. A late asset load cannot snap yaw after rotate input and does not add a second yaw-driven presentation change. Zoom and bounds notifies on load are the previous path.

## Tests / build

| Step | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | Succeeded |
| `gp.Camera.RunDefaultYawContractTest` | Failures=0 (spawn yaw 0 / 90 / -90 / 180 / 37 all become 90; rotate input still changes yaw) |
| `gp.UI.RunMinimapCameraBoundsContractTest` | Failures=0 |
| `gp.UI.RunMinimapClickToPanFootprintSyncContractTest` | Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | Failures=0 |
| `gp.Voxel.RunCameraInvokerContractTest` | Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Failures=0 |

## Files changed

- `GP/Source/GPRuntime/Public/Camera/GPCameraConfigDataAsset.h`
- `GP/Source/GPRuntime/Public/Camera/GPCameraPawn.h`
- `GP/Source/GPRuntime/Private/Camera/GPCameraPawn.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPCameraDefaultYawContractTest.cpp`
- `Docs/TDD/11_RTS_Camera.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- this report

## Protected audit

Not committed: `GP/Content/` including `L_VoxelArena_2P` and `L_PrototypeArena`, `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.

## Exact operator retest

PIE `L_VoxelArena_2P` without rotating PlayerStart. The 3D view should open at yaw 90°, matching the world-fixed minimap. MMB rotate should still yaw away from that default. PlayerStart rotation can stay whatever the map needs for other actors; it no longer sets the RTS camera.
