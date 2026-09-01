# Cursor Work Report

## Status

**MINIMAP_FOUNDATION_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

This checkpoint is native Minimap Presentation Foundation only. Do not merge to `main`. Operator Blueprint wiring of the protected `WBP_GP_HUD` minimap seam is next.

## Branch / SHAs

- Branch: `ui/gp-minimap`
- `origin/main`: `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Merge-base with `origin/main`: `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Head (implementation): `6a76c204f3540dbf0ab7c041c96623933b5cba8e`
- Ahead of `origin/main`: **1** (this report commit makes **2**)
- Behind `origin/main`: **0**
- Fast-forward merge to `main`: **not requested / not merge-ready**

## Exact implementation

`UGP_MinimapPresenter` is owned by `UGP_HUDViewModelSubsystem` (one per LocalPlayer). It is presentation-only and binds `UGP_LocalFoWComponent::OnLocalFoWUpdated` on the owning PlayerController. It never ticks, never polls, never scans the world, never sync-loads, and never copies the FoW cell arrays / 2000×2000 texture.

`UGP_HUDRootWidget` Blueprint seam (do not modify WBP in this checkpoint):

- `IsMinimapReady()`
- `WorldToMinimapNormalized(FVector)`
- `MinimapNormalizedToWorld(FVector2D, float WorldZ)`
- `GetMinimapFoWStateNormalized(FVector2D)`
- `GetMinimapPresentation()` → `FGP_MinimapPresentation`
- `BP_OnMinimapChanged()`

`FGP_MinimapPresentation` fields: `bIsReady`, `LocalTeamId`, `WorldOrigin`, `GridOrigin`, `WorldSizeCm`, `GridDimensions`, `CellSizeCm`, `Revision`.

Bounds source of truth for this checkpoint = trusted FoW metadata (canonical 100 cm cells, 2000×2000, origin currently deterministic on the FoW component). No map-bounds actor.

Not in this checkpoint: blips, camera rectangle, click-to-move-camera, last-known actors, terrain capture/render target, new FoW gameplay, actor world scans, Voxel/Terrain.

## Coordinate orientation contract

Normalized minimap XY is axis-aligned to the current trusted FoW grid:

- `Normalized.X = 0` at `GridOrigin.X`, `1` at `GridOrigin.X + WorldSizeCm.X`
- `Normalized.Y = 0` at `GridOrigin.Y`, `1` at `GridOrigin.Y + WorldSizeCm.Y`
- World `+X` → minimap `+X`
- World `+Y` → minimap `+Y`
- No rotation, mirroring, or Slate Y-flip in the presenter
- `WorldToMinimapNormalized` clamps out-of-grid world XY onto `[0,1]`
- `MinimapNormalizedToWorld` clamps input XY onto `[0,1]` before mapping; `WorldZ` is passed through
- `GetMinimapFoWStateNormalized` does **not** clamp: XY outside `[0,1]` or not-ready → `Unexplored`; inside `[0,1]` queries the trusted mirror at the corresponding cell center (inclusive `1.0` uses the last cell)

## Event / binding lifecycle

- Subsystem creates the presenter in `Initialize` and `Shutdown`s it in `Deinitialize` / controller unbind.
- `BindMinimapPresenter` / `Initialize` always unbinds first, then adds a single `OnLocalFoWUpdated` handle. Repeated Initialize/Rebind does not duplicate delegates.
- Presentation metadata rebuilds only when readiness / grid metadata / Revision change, then `OnMinimapPresentationChanged` fires.
- HUD root binds that multicast in `NativeConstruct` and forwards `BP_OnMinimapChanged`. No Blueprint polling.
- Controller teardown / presenter `Shutdown` returns not-ready (`LocalTeamId = -1`, FoW queries `Unexplored`) and ignores later mirror updates.

Architecture unchanged: `GPUIRuntime` → `GPRuntime` → `GPGASRuntime`.

## Tests

Headless `-game -unattended -nop4 -NullRHI` `L_PrototypeArena`. No quit. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapPresentationContractTest` | Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |
| `gp.UI.RunSelectionViewModelContractTest` | Complete Failures=0 |
| `gp.UI.RunContextActionPresentationContractTest` | Complete Failures=0 |

## Builds

| Command | Result |
| --- | --- |
| `GPEditor Win64 Development` (UHT included) | **Passed** |
| `GP Win64 Development` | **not run** (intermediate checkpoint) |
| `GP Win64 Shipping` | **not run** (intermediate checkpoint) |

## Changed files (`origin/main...HEAD`)

- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/Development/Cursor_Work_Report.md` (this report commit)
- `GP/Source/GPUIRuntime/Public/ViewModels/GPMinimapPresenter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPMinimapPresenter.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapPresentationContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPHUDViewModelBridgeContractTest.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPHUDViewModelSubsystem.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPHUDRootWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPHUDRootWidget.cpp`

## Protected-file audit

**Not committed:** `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `WBP_GP_SelectionGroupRow`, `WBP_GP_LaunchContainerRow`, `Content/`, `Config/`, authored maps, Materials, VFX, `GP.uproject`, `Tools/`.

Local dirty/untracked Content/Config/maps/Tools/`GP.uproject` left unstaged. No destructive git.

## Next operator Blueprint seam

In protected `WBP_GP_HUD` (local, uncommitted):

- Bind `BP_OnMinimapChanged` on the HUD root.
- Read `GetMinimapPresentation()` / `IsMinimapReady()` for the bottom-left square.
- Use `WorldToMinimapNormalized` / `MinimapNormalizedToWorld` / `GetMinimapFoWStateNormalized` for FoW-aware presentation.
- Do not poll. Do not expect blips or camera rectangle yet.

## Remaining known non-blocking limitations

- Minimap foundation ≠ complete minimap.
- Blips, camera rectangle, click-to-pan, last-known actors, and terrain/render-target remain later checkpoints.
- Authored `WBP_GP_HUD` remains operator-local and uncommitted.
- Notifications and production end-of-match remain outside this slice.
