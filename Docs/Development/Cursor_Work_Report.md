# Cursor Work Report

## Status

**MINIMAP_CLICK_TO_PAN_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

LMB click inside the actual minimap MapDest instantly pans the local camera pawn XY-anchor. Zoom, yaw, pitch, and Z are preserved. Selection, marquee, unit commands, and gameplay authority are not involved. Drag-pan is not in this checkpoint.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-checkpoint HEAD | `b4edf92d20ea8e2b193f7e0fae5a53bd5609b3d6` (camera-footprint crash-fix SHA record) |
| Checkpoint HEAD | `8f252a6d26ed7d0ede1a6341188f611772c8d04a` |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |

Not merge-ready to `main`.

## Exact input route

`FInputModeGameAndUI` is unchanged. No `SetInputModeUIOnly`. No global mouse capture. Camera rotate remains MMB. Selection remains Enhanced Input `IA_Select` (LMB). RMB command IMC is unchanged.

1. `SGPMinimapSurface` is `EVisibility::Visible` (hit-testable), `IsInteractable() == true`, no Tick, no keyboard focus.
2. `OnMouseButtonDown` runs only for pointer routing. `EKeys::LeftMouseButton` is consumed (`FReply::Handled()`). Any other button returns `FReply::Unhandled()` (RMB is not click-to-pan).
3. MapDest is `UGP_MinimapWidget::ComputeSharedMapDestLocal(...)` — the same helper `OnPaint` uses for background / FoW / blips / camera footprint.
4. Local cursor = `AllottedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())`.
5. Outside MapDest (letterbox / padding): `ConsumedNoPan` — handled, no world mapping, no camera move. Prevents the click falling through the Visible surface into world selection/marquee.
6. Inside MapDest: `PanRequested` → Slate delegate `OnMapLeftClick(SurfaceUV)`.

## Slate callback lifetime

`FOnGPMinimapMapLeftClick` is a Slate `DECLARE_DELEGATE_OneParam`. Bound in `RebuildWidget` with `CreateUObject(this, &UGP_MinimapWidget::HandleMapLeftClick)` (weak UObject). `OnMouseButtonDown` uses `ExecuteIfBound`. `ReleaseSlateResources` clears the delegate, then `MySurface.Reset()`. No raw UObject pointer is stored on the Slate widget.

`UGP_MinimapWidget::HandleMapLeftClick` rejects non-finite / out-of-range UV, then `BoundPresenter` (`TWeakObjectPtr`). Missing presenter: no-op.

## Coordinate conversion

Shared inverse is unchanged:

- Presenter: world +X/+Y → normalized +X/+Y
- Surface: `SurfaceUVToPresenterNormalized` = `(1-X, 1-Y)` (same as paint)

Flow: Local point → SurfaceUV in MapDest `[0..1]` → shared inverse → `UGP_MinimapPresenter::PanCameraToMinimapNormalized` → `MinimapNormalizedToWorld` on current displayed camera/playable bounds → camera seam.

## Camera API

`AGP_CameraPawn::SetCameraAnchorWorldXY(const FVector2D& TargetXY)`

- Local presentation only. No RPC. No gameplay state.
- Rejects non-finite XY.
- Click target = **camera pawn actor location XY** (canonical RTS pan anchor). No viewport-footprint-center compensation.
- Preserves actor Z, yaw (`CurrentYaw` / root yaw), zoom (`CurrentArmLength` / spring arm), pitch.
- `SetActorLocation` XY, then existing `ClampToBounds(*GetActiveConfig())`, then restore Z if clamp moved it.
- `SyncSpringArmPresentation(0)` + `NotifyCameraPresentationChangedIfNeeded()` so the footprint event can follow.

Presenter: `UGP_MinimapPresenter::PanCameraToMinimapNormalized`. Requires ready + usable displayed bounds + finite `[0..1]` + valid bound `AGP_CameraPawn`. Widget does not call `SetActorLocation`.

## Preserved state

| State | Click-to-pan |
| --- | --- |
| Camera XY-anchor | Moves to mapped world point, then canonical clamp |
| Z | Preserved |
| Yaw | Preserved |
| Pitch | Preserved |
| Zoom / arm length | Preserved |
| Selection | Unchanged |
| Unit commands | Not issued |
| Gameplay authority | Untouched |

## Bounds semantics

A normalized MapDest click is inside the displayed camera/playable rect. `SetCameraAnchorWorldXY` still runs the pawn's canonical `ClampToBounds`. Widget/presenter do not duplicate clamp math. Missing presenter/camera/NaN: ignored.

## Input conflict audit

| Channel | Result |
| --- | --- |
| LMB inside MapDest | Slate handled; camera pan only |
| LMB letterbox on the Visible surface | Slate handled; no pan; should not start world click-select/marquee |
| RMB on surface | Unhandled; not click-to-pan; no new minimap RMB commands |
| MMB rotate | Unchanged (`SetRotateActive` / `IA_Camera` rotate) |
| `IA_Select` / marquee | PC code unchanged; consume is Slate `FReply::Handled` under GameAndUI |
| Command targeting / placement | Unchanged |

## Letterbox semantics

Input region == paint MapDest. Authored background aspect letterbox: clicks in the dark bars do not produce map UV. No background (fallback square): MapDest is the current square contract. Both cases are in `gp.UI.RunMinimapClickToPanContractTest`.

## Tests

Command pattern: `UnrealEditor-Cmd` `GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam -ExecCmds=<cmd>`

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapClickToPanContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraFootprintContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapEnemyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapFriendlyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | **PASS** Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS** Complete Failures=0 |

Focused contract covers: MapDest center → SurfaceUV 0.5/0.5 and shared inverse; visual left/right/top/bottom vs `(1-X,1-Y)`; letterbox reject / inside accept / RMB ignored; fallback square vs textured letterbox; camera XY move; Z/yaw/zoom preserved; clamp; footprint follow; no presenter/camera/invalid UV; Slate release; LMB-only; no Tick.

## GPEditor build

`GPEditor Win64 Development` + UHT for `D:\Progects\RTS\GP\GP.uproject`: **Succeeded**. Not GP Development / Shipping.

`GPUIRuntime` now public-depends on `InputCore` (`EKeys` in the minimap surface/contract).

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

1. PIE: LMB center of minimap → camera jumps there
2. LMB corners → camera goes to the matching world side
3. LMB next to a friendly blip → camera footprint around that area
4. Orientation: visual left/top still match the already-validated world mapping
5. Zoom, then click-to-pan → zoom unchanged
6. Rotate, then click-to-pan → yaw unchanged
7. LMB on minimap does not select world units and does not start marquee
8. RMB on minimap does not click-to-pan
9. FoW / blips / camera footprint still update

## Out of scope

- Drag-pan / click-and-drag camera on minimap
- RMB minimap commands, minimap selection, ping
- Last-known
- Minimap zoom / rotate
- SceneCapture
- Merge to `main`

**INTERMEDIATE / NOT MERGE READY**
