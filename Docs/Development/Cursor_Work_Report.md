# Cursor Work Report

## Status

**MINIMAP_CLICK_TO_PAN_FOOTPRINT_SYNC_FIXED_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Click-to-pan already moved the camera pawn. The minimap camera footprint now rebuilds from the current CameraComponent view in that same presentation event, without a later WASD/mouse Tick.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-checkpoint HEAD | `12b12accf306f9f2a7feb6b1256378487023a251` (click-to-pan SHA record) |
| Checkpoint HEAD | *(recorded after commit)* |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |

Not merge-ready to `main`.

## Confirmed root cause

`SetCameraAnchorWorldXY` did move the pawn, sync the spring arm, and broadcast `OnCameraPresentationChanged`. The presenter rebuilt the footprint synchronously.

Production `TryDeprojectViewportCornerToGround` then called `AGP_PlayerController::DeprojectScreenPositionToWorld`. That uses the PlayerCameraManager view from earlier in the frame, still aimed at the pre-click camera.

So in the click event:

- actor / CameraComponent already at the new XY
- presentation event already fired
- footprint rays still used the stale PCM view
- presenter stored the old quadrilateral
- the next real camera Tick/input refreshed PCM and the outline finally caught up

Headless contracts hid this: they set `ContractSetViewportSizeOverride` and already used `GetPresentationView` + `FSceneView::DeprojectScreenToWorld`.

## Old stale-view path

If `bHasViewportSizeOverride` was false (PIE / real viewport):

`PlayerController::DeprojectScreenPositionToWorld` → ray vs pawn XY-anchor plane.

## New canonical projection source

One path for production and contracts:

1. Viewport size from the local PlayerController, or the contract override when the viewport is degenerate
2. `AGP_CameraPawn::GetPresentationView()` — current CameraComponent world location, rotation, FOV
3. Perspective `FMinimalViewInfo` + `UGameplayStatics::GetViewProjectionMatrix`
4. `FSceneView::DeprojectScreenToWorld` for corners `(0,0)`, `(W-1,0)`, `(W-1,H-1)`, `(0,H-1)`
5. Existing ray ∩ horizontal plane Z = `GetGroundReferencePlaneZ()`
6. Existing unclamped normalize + convex clip `[0,1]²` + shared surface `(1-X, 1-Y)`

No PCM / `GetPlayerViewPoint` fallback. If `GetPresentationView` fails, the footprint is not drawn.

PlayerController is only for local ownership and viewport pixel size.

`SetCameraAnchorWorldXY` still `SyncSpringArmPresentation(0)` and now also `UpdateComponentToWorld` on RootScene / SpringArm / Camera so `GetPresentationView` is current before `NotifyCameraPresentationChangedIfNeeded`.

## Immediate update sequence

LMB MapDest → `PanCameraToMinimapNormalized` → `SetCameraAnchorWorldXY` → component transform sync → fingerprint change → `OnCameraPresentationChanged` → `RebuildCameraFootprint` from the new CameraComponent view → Slate cache/push. No timer, no next Tick, no fake input.

Ground plane, clip, bounds, surface orientation, click mapping, and paint are unchanged.

## Regression that fails the old implementation

`gp.UI.RunMinimapClickToPanFootprintSyncContractTest`

1. Build a valid footprint at camera A
2. `SetCameraAnchorWorldXY(B)` 
3. No later `Tick` / WASD / mouse
4. The presentation callback has already rebuilt the footprint
5. Revision increased; world-space footprint centroid translated with the anchor (same sign, non-trivial delta)
6. Current `GetPresentationView` corners moved without a PlayerCameraManager refresh
7. Yaw/FOV preserved

On the old PCM production path a real-viewport click left the centroid at A until the next camera Tick. NullRHI still needs a size override when `GetViewportSize` is degenerate; that override now uses the same CameraComponent projection as production.

## Tests

Command pattern: `UnrealEditor-Cmd` `GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam -ExecCmds=<cmd>`

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapClickToPanFootprintSyncContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapClickToPanContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraFootprintContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapEnemyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapFriendlyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | **PASS** Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS** Complete Failures=0 |

## GPEditor build

`GPEditor Win64 Development` + UHT for `D:\Progects\RTS\GP\GP.uproject`: **Succeeded**. Not GP Development / Shipping.

## Protected audit

Not committed:

- `WBP_GP_HUD`
- `GP/Content/`
- `GP/Config/`
- `L_PrototypeArena`
- DataAssets
- Materials/VFX
- `GP.uproject`
- `Tools/`

Operator dirty/untracked Content/Config/Tools/`GP.uproject` preserved.

## Operator test

1. PIE; do not pan/rotate first
2. LMB a distant minimap point
3. World camera jumps
4. Camera footprint jumps to that area in the same click
5. No WASD / mouse move required
6. Repeated clicks update the outline immediately
7. Zoom and yaw unchanged
8. FoW / blips unchanged

## Out of scope

- Drag-pan
- Last-known
- SceneCapture
- Merge to `main`

**INTERMEDIATE / NOT MERGE READY**
