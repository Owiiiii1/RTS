# Cursor Work Report

## Status

**MINIMAP_CAMERA_BOUNDS_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Minimap displayed world extents now follow the same resolved camera/playable bounds `AGP_CameraPawn` uses for `ClampToBounds`. The FoW gameplay grid (2000×2000 / 100 cm) is unchanged. Minimap is a crop/projection over trusted LocalFoW. No SceneCapture. Not a complete minimap (no blips / camera rectangle / click-to-pan / last-known).

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `ui/gp-minimap` |
| Remote | `origin/ui/gp-minimap` |
| Pre-checkpoint HEAD | `d54db93b5e22ed2e88af3c98c15e555c83369a63` |
| Checkpoint HEAD | `d2eba077097a042a6c8e4f4b4adecd7767f8a082` |
| Merge-base with `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` |
| `origin/main` | `cfd3d3858993b372ea69bd55865b831584297a83` (behind main = 0) |

Not merge-ready to `main`.

## Canonical bounds source

Same `FBox` `AGP_CameraPawn::ClampToBounds` uses:

1. Valid `AGP_CameraBoundsVolume::GetCameraBounds()` AABB, if the pawn has resolved a volume.
2. Else the active camera config `FallbackBounds` (CDO via `GetActiveConfig()` until async DA load, then authored `CachedConfig`).

FoW grid origin / 2000×2000 / 100 cm are **not** minimap visual bounds.

## Camera API seam

`GPRuntime` public seam on `AGP_CameraPawn` (GPUIRuntime does not scan the world):

- `bool GetResolvedCameraBounds(FBox& OutBounds) const`
- `static bool IsUsableResolvedCameraBounds(const FBox& Bounds)`
- `FOnGPResolvedCameraBoundsChanged OnResolvedCameraBoundsChanged`

Notify only after `BeginPlay` (volume find + first clamp) and after async `HandleConfigLoaded`. Contract-only `ContractSetCameraBoundsVolume` / `ContractClearCameraBoundsVolume` (`!UE_BUILD_SHIPPING`) so UI tests do not iterate actors.

## Presenter mapping before / after

**Before:** `WorldToMinimapNormalized` / `MinimapNormalizedToWorld` used `GridOrigin` + full FoW `WorldSizeCm`. On `L_PrototypeArena` that is the 2000×2000 visibility field, so the playable camera area was a small patch.

**After:**

- FoW metadata stays: `GridOrigin`, `WorldSizeCm`, `GridDimensions`, `CellSizeCm`
- Displayed rect: `MapWorldMin` + `MapWorldSizeCm`
- Normalized `[0,1]` maps to that displayed rect (world +X/+Y, no presenter Y-flip)
- `GetMinimapFoWStateNormalized` maps displayed normalized → world inside camera bounds, then `UGP_LocalFoWComponent::GetStateAtWorldLocation`
- Widget still `ScreenY = 1 - NormalizedY`; background and FoW share that transform

`UGP_MinimapPresenter::Initialize(PC)` binds the possessed `AGP_CameraPawn` (no `TActorIterator` in GPUIRuntime).

## Fallback semantics

If camera bounds are missing or degenerate (non-finite / Min ≥ Max XY):

- displayed fields fall back to the trusted FoW-grid rect
- mapping stays finite (no divide-by-zero)
- presentation can still become ready from FoW metadata

This keeps isolated 4×4 presenter/surface contracts green when no pawn is bound.

## Async / event lifecycle

No widget Tick. No presenter Tick. No polling.

1. HUD `BindMinimapPresenter` → `Initialize(PC)`: bind LocalFoW + current pawn; rebuild immediately from `GetResolvedCameraBounds` if already valid.
2. If pawn `BeginPlay` / async config has not finished, first snapshot may be CDO fallback (or FoW-grid if pawn is null).
3. `OnResolvedCameraBoundsChanged` rebuilds presentation once bounds are ready. Re-`Initialize` / rebind does not duplicate camera or mirror delegates (`GetBoundCameraBoundsDelegateCount()` / `GetBoundDelegateCount()`).

MVP bounds are static after initialization; this is not a dynamic bounds system.

## Background image contract

`UGP_UIPresentationSettings::MinimapBackgroundTexture` is authored to **camera/playable bounds**, not the full FoW grid. Left/right/top/bottom = those extents. World +Y / NormalizedY=1 is the top of the image. FoW overlay uses the same mapping. No new texture asset created in this checkpoint.

## Tests

Command: `UnrealEditor-Cmd` `D:\Progects\RTS\GP\GP.uproject` `/Game/GrimProtocol/Maps/L_PrototypeArena` `-game -unattended -nop4 -NullRHI -nosplash -nosteam`

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapCameraBoundsContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapSurfaceContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | **PASS** Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS** Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS** Complete Failures=0 |

Focused contract covers: volume → displayed; no volume → exact Config `FallbackBounds`; displayed ≠ full FoW grid; normalized (0,0)/(1,1)/center round-trip; FoW query through crop; degenerate safe; no Tick; rebind does not duplicate camera delegates; background/FoW share orientation.

No dedicated `gp.Camera.*` contract exists; volume vs fallback is asserted against `GetResolvedCameraBounds` in the new UI contract.

## GPEditor build

`GPEditor Win64 Development` + UHT for `D:\Progects\RTS\GP\GP.uproject`: **Succeeded** (79.68 s). Not GP Development / Shipping.

## Changed files (this checkpoint)

- `GP/Source/GPRuntime/Public/Camera/GPCameraPawn.h`
- `GP/Source/GPRuntime/Private/Camera/GPCameraPawn.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPMinimapPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPMinimapPresenter.cpp`
- `GP/Source/GPUIRuntime/Public/Settings/GPUIPresentationSettings.h`
- `GP/Source/GPUIRuntime/Public/Widgets/GPMinimapWidget.h`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapCameraBoundsContractTest.cpp` (new)
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapPresentationContractTest.cpp`
- `Docs/TDD/11_RTS_Camera.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not edited / not committed:

- `WBP_GP_HUD`
- `GP/Content/`
- `GP/Config/`
- `L_PrototypeArena`
- DataAssets
- Materials/VFX
- `GP.uproject`
- `Tools/`

Operator dirty/untracked left in the working tree (same set as before this checkpoint):

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/GP.uproject`
- `GP/Content/Basic_VFX/`
- `GP/Content/GrimProtocol/Blueprint/` (includes local `WBP_GP_HUD`)
- `GP/Content/GrimProtocol/DataAssets/Buildings/`
- `GP/Content/GrimProtocol/DataAssets/Game/`
- `GP/Content/GrimProtocol/DataAssets/Units/`
- `GP/Content/GrimProtocol/Materials/`
- `GP/Content/Mixed_Magic_VFX_Pack/`
- `GP/Content/RocketThrusterExhaustFX/`
- `Tools/`

## Operator validation

1. Open ordinary `D:\Progects\RTS\GP\GP.uproject` (this primary tree; already on `ui/gp-minimap` with this GPEditor build).
2. PIE on `L_PrototypeArena`.
3. Confirm the black minimap square is the same UI size, but the logical world extent now matches the camera movement area (playable clamp), not the full FoW grid. FoW/vision should look much larger on the minimap. Panning the camera into a playable edge should correspond to the minimap edge. If camera bounds are symmetric, world center stays ~0.5/0.5.
4. Move a unit and confirm Visible → Explored still works at the new scale.

## INTERMEDIATE / NOT MERGE READY

Blips, camera rectangle, click-to-pan, last-known, and terrain/voxel remain later. Authored `WBP_GP_HUD` stays operator-local.
