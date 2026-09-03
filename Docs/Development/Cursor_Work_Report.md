# Cursor Work Report

## Status

**MINIMAP_CAMERA_FOOTPRINT_CRASH_FIXED_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

PIE no longer hits the UE 5.8 TArray self-reference assertion when painting the minimap camera footprint outline.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-checkpoint HEAD | `40c8ca33a8c3cf66cf55b31ebd320ccbdb6293ba` (camera-footprint SHA record) |
| Checkpoint HEAD | `b2e552692b1510821c11cf48c452ecc1bdddf059` |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |

Not merge-ready to `main`.

## Exact crash root cause

`SGPMinimapSurface::OnPaint` closed the camera-footprint polyline by appending the first outline point back onto the same `TArray`:

```
OutlinePoints.Add(OutlinePoints[0]);
```

UE 5.8 `TArray::Add` asserts if the argument is an element reference from the array being modified, because `Add` may reallocate and invalidate that reference.

This ran on the real Slate paint path with a valid footprint. Headless NullRHI contract tests did not execute that allocator scenario.

## Exact unsafe line / pattern

- File: `GP/Source/GPUIRuntime/Private/Widgets/GPMinimapWidget.cpp`
- Function: `SGPMinimapSurface::OnPaint`
- Pattern: `TArray<T>::Add(SameArray[Index])` (self-referential element)

## Exact fix

Extracted `UGP_MinimapWidget::BuildClosedCameraFootprintOutlinePoints`.

After mapping presenter corners through the existing shared surface transform, it copies the first point by value, then adds that copy:

```
if (OutOutlinePoints.Num() < 3) { reset; return false; }
const FVector2f FirstPoint = OutOutlinePoints[0];
OutOutlinePoints.Add(FirstPoint);
```

Guard remains `Num >= 3` before `[0]`. Semantics unchanged: closed outline is N+1 points, last equals first. Projection, clipping, orientation, and paint order are unchanged. `OnPaint` now calls this helper.

## Similar-pattern audit

Searched this minimap checkpoint / touched code for:

- `Array.Add(Array[Index])`
- `Array.Emplace(Array[Index])`
- `Insert` of an element reference from the same container
- self-`Append`

Results:

| Location | Finding |
| --- | --- |
| `GPMinimapWidget.cpp` | Only the crash site above. Fixed. |
| `GPMinimapPresenter.cpp` clip helpers | `OutPolygon.Add` copies from `InPolygon` / local intersection values, not from `OutPolygon` itself. `Previous = InPolygon.Last()` is a value copy. Safe. |
| Footprint tests | No self-referential `Add`. |

No other matches in GPUIRuntime for this pattern. No broad refactor.

## Regression test

New command: `gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest`

Exercises the paint-safe helper (not a source-string-only check):

- 4 corners → N+1 closed points, last == first
- mapping uses shared `(1-X, 1-Y)`
- `< 3` corners rejected / cleared
- 3-corner triangle also closes by copied first point

That construction is the same path `OnPaint` now uses.

## Tests

Command pattern: `UnrealEditor-Cmd` `GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam -ExecCmds=<cmd>`

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraFootprintContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapEnemyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapFriendlyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | **PASS** Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS** Complete Failures=0 |

## GPEditor build

`GPEditor Win64 Development` + UHT for `D:\Progects\RTS\GP\GP.uproject`: **Succeeded**. Not GP Development / Shipping.

## Smoke / PIE

Interactive editor PIE was not driven from this session.

Additional `-game` D3D12 smoke (no `-NullRHI`) on `L_PrototypeArena`: map loaded, engine initialized, session reached frame 665 and closed the log with **no** `already comes from the container` / assertion / fatal. This is not a substitute for operator PIE (HUD/minimap visual check still required).

## Changed files (this checkpoint)

- `GP/Source/GPUIRuntime/Public/Widgets/GPMinimapWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPMinimapWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapCameraFootprintOutlineCrashContractTest.cpp` (new)
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not committed:

- `WBP_GP_HUD`
- `GP/Content/`
- `GP/Config/`
- `L_PrototypeArena`
- DataAssets
- `GP.uproject`
- `Tools/`

Operator dirty/untracked Content/Config/Tools/`GP.uproject` preserved.

## Operator test

1. PIE on `L_PrototypeArena`
2. Confirm the game no longer crashes on start
3. Confirm the light camera-view outline is present
4. Pan → outline moves
5. Zoom in/out → outline shrinks/grows
6. Rotate → outline rotates
7. FoW and blips still behave as before

## Out of scope

- Click-to-pan
- Last-known
- SceneCapture
- Merge to `main`

**INTERMEDIATE / NOT MERGE READY**
