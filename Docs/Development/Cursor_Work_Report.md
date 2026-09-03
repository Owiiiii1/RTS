# Cursor Work Report

## Status

**MINIMAP_CAMERA_FOOTPRINT_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

The minimap camera indicator is the real projected viewport footprint on the gameplay XY-anchor plane, not a fixed square around the camera pawn. Pan / zoom / yaw update it from camera presentation events. Widget Tick remains off.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-checkpoint HEAD | `55385fa1077c4143773da25ea052429545c07a43` (enemy-blips SHA record) |
| Checkpoint HEAD | *recorded in follow-up SHA commit* |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |

Not merge-ready to `main`.

## Exact projection algorithm

1. Local `AGP_PlayerController` viewport size (or shipping-disabled contract override).
2. Four viewport corners: `(0,0)`, `(W-1,0)`, `(W-1,H-1)`, `(0,H-1)`.
3. Production: `DeprojectScreenPositionToWorld` → origin + direction.
4. Ray vs horizontal plane `Z = AGP_CameraPawn::GetGroundReferencePlaneZ()`. Reject failed deproject, `|Dir.Z| ≈ 0`, `t ≤ KINDA_SMALL_NUMBER`, non-finite hits.
5. World XY → presenter normalized (unclamped) using camera/playable bounds (`MapWorldMin` / `MapWorldSizeCm`). No surface flip here.
6. Convex clip of the quad against the unit square `[0,1]²` (Sutherland–Hodgman). Not independent per-corner clamp.
7. If clip has fewer than 3 finite points, or any earlier step fails: `bIsValid = false`, do not draw.
8. Widget: shared `Xscreen = 1 - X`, `Yscreen = 1 - Y`, then Slate outline.

Headless contract tests with a 0-size NullRHI viewport use a shipping-disabled viewport override plus camera-view unproject (`GetPresentationView` + `FSceneView::DeprojectScreenToWorld`) with the same ray-plane step. PIE / production with a real viewport uses `DeprojectScreenPositionToWorld`.

## Ground reference Z

`AGP_CameraPawn::GetGroundReferencePlaneZ()` = camera pawn actor location Z.

Pan already preserves actor Z (`PreservedZ`). This is the XY-anchor / gameplay reference plane, **not** hardcoded World Z=0.

Terrain-surface sampling / voxel-aware footprint is deferred with terrain integration.

## Camera update event seam

After `AGP_CameraPawn` Tick applies zoom, pitch, yaw, pan, clamp, the pawn syncs the spring arm so the camera component matches that frame, then compares a presentation fingerprint:

- actor location
- yaw
- spring-arm length
- spring-arm pitch
- camera location / rotation
- viewport width / height

`OnCameraPresentationChanged` broadcasts **only if** that fingerprint changed. Idle ticks do not fire.

`UGP_MinimapPresenter` binds/unbinds that delegate (rebind does not duplicate). On change it rebuilds `FGP_MinimapCameraFootprint` and broadcasts `OnMinimapCameraFootprintChanged`.

No widget Tick. No UI polling timer.

## Viewport resize handling

Viewport size is part of the same camera presentation fingerprint, compared on the existing local camera Tick path (the pawn already calls `GetViewportSize` for edge-scroll). Resize rebuilds the footprint without a dedicated UI timer.

## Footprint clipping semantics

Unclamped normalized corners may lie outside `[0,1]` when the viewport sees world beyond playable camera bounds (pawn is clamped; the view frustum is not).

MVP clips the convex quad against the unit square. Result stays inside the minimap. Self-crossing independent clamps are not used. If the intersection has `< 3` points, nothing is drawn. No NaN / huge coordinates.

## Paint layer

One Slate paint pass, no child widgets:

1. background
2. FoW
3. blips
4. **camera footprint outline** (on top)

Visual: thin neutral line `~1.5 px`, color `(0.92, 0.92, 0.92, 0.95)`, no fill, not team color.

Shared orientation with FoW and blips: presenter world `+X/+Y` → surface `(1-X, 1-Y)`.

## Tests

Command pattern: `UnrealEditor-Cmd` `GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam -ExecCmds=<cmd>`

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapCameraFootprintContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapEnemyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapFriendlyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | **PASS** Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS** Complete Failures=0 |

Focused coverage: valid local PC+camera footprint; no camera / invalid viewport → no draw; rebind does not duplicate delegates; pan moves all points; zoom-out unclamped extent grows / zoom-in shrinks; yaw is not an axis-aligned fixed rect; shared `(1-X,1-Y)`; edge clip finite inside unit square; idle Tick does not bump revision; shutdown clears; plane Z = pawn Z; widget/presenter have no Tick UFUNCTION; `SGPMinimapSurface` `SetCanTick(false)`.

No dedicated `gp.Camera.*` contract command exists in this tree.

## GPEditor build

`GPEditor Win64 Development` + UHT for `D:\Progects\RTS\GP\GP.uproject`: **Succeeded**. Not GP Development / Shipping.

## Changed files (this checkpoint)

- `GP/Source/GPRuntime/Public/Camera/GPCameraPawn.h`
- `GP/Source/GPRuntime/Private/Camera/GPCameraPawn.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPMinimapPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPMinimapPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPMinimapWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPMinimapWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapCameraFootprintContractTest.cpp` (new)
- `Docs/TDD/11_RTS_Camera.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not committed / not modified for this checkpoint’s source work:

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

1. PIE on `L_PrototypeArena`
2. Confirm a light neutral camera-view outline on the minimap
3. WASD / edge pan → outline moves
4. Zoom in → outline smaller; zoom out → larger
5. Rotate camera → outline rotates with real camera orientation (not a fixed axis-aligned square)
6. Near camera bounds the outline stays on the minimap (clipped, no NaN / overshoot)
7. FoW and blips still behave as in the previous PASS

## Out of scope (next checkpoints)

- Click-to-pan / minimap drag-pan
- Last-known enemy markers
- Selection highlighting
- Per-unit icons
- SceneCapture / RenderTarget
- Terrain / voxel-aware footprint
- Gameplay authority changes

**INTERMEDIATE / NOT MERGE READY**
