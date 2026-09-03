# Cursor Work Report

## Status

**MINIMAP_FRIENDLY_BLIPS_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Horizontal FoW/minimap mirror is fixed at the shared widget surface layer. Friendly unit/building/MainBase blips are drawn from the existing local unit presentation registry. Enemy blips, camera rectangle, click-to-pan, and last-known markers remain later checkpoints.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-checkpoint HEAD | `d2eba077097a042a6c8e4f4b4adecd7767f8a082` (camera-bounds) |
| Checkpoint HEAD | `b58ea515c097fa5c3fb1ad9e7ea168454d2ec12d` |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |

Not merge-ready to `main`.

## Exact reason / fix for horizontal mirror

Operator confirmed FoW was mirrored horizontally vs the game world.

**Cause:** widget surface transform only flipped Y (`ScreenY = 1 - NormalizedY`, `ScreenX = NormalizedX`). For the authored/game orientation that left the horizontal axis mirrored.

**Fix (widget/presentation layer only):**

- `PresenterNormalizedToSurfaceUV` → `(1 - X, 1 - Y)`
- `SurfaceUVToPresenterNormalized` → `(1 - X, 1 - Y)` (self-inverse)

**Unchanged presenter contract:**

- `WorldToMinimapNormalized` still maps world `+X/+Y` → normalized `+X/+Y`
- displayed extents remain resolved camera/playable bounds
- FoW gameplay grid (2000×2000 / 100 cm) unchanged

Background, FoW overlay, friendly blips, and future camera-rect / click input must all use this shared surface transform.

## Shared surface transform

```
Presenter normalized: world +X / +Y
Surface: Xscreen = 1 - NormalizedX
         Yscreen = 1 - NormalizedY
```

Paint order in one Slate pass:

1. background
2. FoW
3. friendly blips

## Blip registry / source

Canonical live membership is **reused**, not reinvented:

- `UGP_LocalFoWUnitPresentationSubsystem` (`RegisteredUnits`)
- `AGP_UnitBase` BeginPlay / EndPlay self-register / unregister

No `GetAllActorsOfClass`, no `TActorIterator` in the widget, no widget Tick world scan.

Added registry seams (no duplicate gameplay state):

- `OnUnitRegistryChanged`
- `OnRegisteredUnitsEvaluated` (end of existing 10 Hz evaluation)
- `ForEachRegisteredUnit`

`UGP_MinimapPresenter` builds presentation-only `FGP_MinimapBlip` snapshots:

- `NormalizedPosition` (camera-bounds normalized, unclamped; outside → omit)
- `Kind` Unit / Building
- weak `SourceActor` for lifecycle identity

Friendly filter: `TeamId == LocalTeamId >= 1` and selection type unit or building. Enemy actors produce no friendly blip in this checkpoint.

## Movement update lifecycle

No widget Tick.

| Event | Path |
| --- | --- |
| Register / unregister | `OnUnitRegistryChanged` → `RebuildFriendlyBlips` |
| Movement | existing registry 10 Hz `EvaluateRegisteredUnits` → `OnRegisteredUnitsEvaluated` → bounded O(registered) blip rebuild |
| FoW / camera metadata | `RebuildPresentation` also refreshes blips |

Bounded position refresh over registered friendly minimap actors is acceptable (~500 own units). No per-frame per-actor property delegates.

## Friendly FoW semantics

Friendly units/buildings/MainBase are **always shown** inside displayed playable/camera bounds. They are **not** FoW-gated. Enemy FoW gating remains the next checkpoint.

## Tests

Command pattern: `UnrealEditor-Cmd` `GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam -ExecCmds=<cmd>`

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapFriendlyBlipsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapCameraBoundsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | **PASS** Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS** Complete Failures=0 |

Focused coverage: orientation `(0,0)→(1,1)` / `(1,1)→(0,0)` / round-trip; FoW/background share transform; friendly unit+building blips; enemy excluded; outside bounds omitted; move updates position; destroy removes; rebind no duplicate registry listeners; no widget Tick / world scan.

## GPEditor build

`GPEditor Win64 Development` + UHT for `D:\Progects\RTS\GP\GP.uproject`: **Succeeded**. Not GP Development / Shipping.

## Changed files (this checkpoint)

- `GP/Source/GPRuntime/Public/Presentation/GPLocalFoWUnitPresentationSubsystem.h`
- `GP/Source/GPRuntime/Private/Presentation/GPLocalFoWUnitPresentationSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPMinimapPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPMinimapPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPMinimapWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPMinimapWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapFriendlyBlipsContractTest.cpp` (new)
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapSurfaceContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapCameraBoundsContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

Camera doc (`TDD/11_RTS_Camera.md`) unchanged — orientation wording lives in UI/FoW docs.

## Protected audit

Not committed / not modified for this checkpoint:

- `WBP_GP_HUD`
- `GP/Content/`
- `GP/Config/`
- `L_PrototypeArena`
- DataAssets
- Materials/VFX
- `GP.uproject`
- `Tools/`

Operator dirty/untracked Content/Config preserved.

## Operator test

1. Open ordinary `D:\Progects\RTS\GP\GP.uproject`
2. PIE on `L_PrototypeArena`
3. Confirm FoW is no longer horizontally mirrored vs the game world
4. Confirm friendly units/buildings (and MainBase) appear as blips on the minimap
5. Move a friendly unit and confirm the blip moves in the correct direction

## Out of scope (next checkpoints)

- Enemy blips / FoW gating for enemies
- Last-known markers
- Camera rectangle
- Click-to-pan
- Selection highlighting
- Per-type unit icons
- SceneCapture / terrain / voxel

**INTERMEDIATE / NOT MERGE READY**
