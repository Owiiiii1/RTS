# Cursor Work Report

## Status

**MINIMAP_SURFACE_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

This checkpoint is native minimap **background image + FoW overlay** only. Do not merge to `main`. Operator inserts `UGP_MinimapWidget` into protected `WBP_GP_HUD`. Blips, camera rectangle, and click-to-pan remain next.

## Branch / SHAs

- Branch: `ui/gp-minimap`
- Checkpoint base `origin/main` (as specified): `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Current `origin/main`: `cfd3d3858993b372ea69bd55865b831584297a83` (SWARM docs; this branch was **not** rebased)
- Merge-base with `origin/main`: `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Previous remote tip (presenter foundation): `73111e162bbca7c1f5352e9f942feaf53d358ea4`
- Head (implementation): `e3d194bb7ce8a2d592f5214cc6374bb43415bfff`
- Ahead of checkpoint base `3b1d3af`: **3** (this report commit makes **4**)
- Behind current `origin/main`: **docs-only SWARM commits; not rebased**
- Fast-forward merge to `main`: **not requested / not merge-ready**

## Widget architecture

`UGP_MinimapWidget : UWidget` (UMG Designer name **GP Minimap**) hosts `SGPMinimapSurface : SLeafWidget`.

Paint layers in this checkpoint:

1. Solid dark fallback square
2. Static authored background texture (letterboxed inside the square)
3. FoW overlay in the **same** dest rect

Hit-test is `Visible` so a later input layer can consume clicks. No widget Tick. No timer polling. No world actor scan. No per-cell UObject/widget.

Self-bind: `SynchronizeProperties` / construct resolves `ULocalPlayer` → `UGP_HUDViewModelSubsystem::GetMinimapPresenter()`, binds `OnMinimapPresentationChanged` once, unbinds in `ReleaseSlateResources` / `BeginDestroy`. Repeated bind is idempotent. Presenter object is subsystem-owned and reused across `PlayerControllerChanged`, so the widget handle stays valid; HUD recreate on travel reconstructs the widget and rebinds.

No authored Blueprint graph is required. Insert the widget; it binds itself.

## Background texture ownership

Owner: existing `UGP_UIPresentationSettings` (Project Settings → Game → GP UI Presentation). No new settings class.

- `TSoftObjectPtr<UTexture2D> MinimapBackgroundTexture`
- Async only via `UAssetManager::GetStreamableManager().RequestAsyncLoad`
- No `LoadSynchronous`
- Empty / still loading → solid dark fallback `FLinearColor(0.02, 0.02, 0.025)`
- Aspect is preserved inside the square viewport (letterbox)
- Source image contract: same world XY bounds as `UGP_MinimapPresenter` / FoW grid; world +Y (NormalizedY = 1) at the **top** of the texture; no hidden rotate/mirror
- Operator assigns the image in the settings asset later. This checkpoint does **not** create an image asset and does **not** write `DefaultGame.ini`

**Not** SceneCapture. **Not** live camera. **Not** render-target terrain.

## Async loading

`StartBackgroundLoad`:

- invalid/empty path → cancel handle, fallback
- already resident → use it, no request
- otherwise `RequestAsyncLoad` + weak-this completion; fallback until complete
- `CancelHandle` on re-request, destruct, and Slate release

## FoW presentation resolution

Presentation-only downsample of the trusted presenter query (`GetMinimapFoWStateNormalized`). Not a new gameplay grid.

- Default **128×128**
- Clamp **32–256** via `UGP_UIPresentationSettings::MinimapFoWPresentationResolution`
- One transient `UTexture2D` (`PF_B8G8R8A8`), not 2000×2000 children
- Overlay colors reuse `GPFoWPresentationRaster::OverlayColorForState`:
  - Unexplored: opaque black
  - Explored: dim (~0.68 alpha)
  - Visible: clear (background seen normally)

## Update lifecycle

- FoW overlay rebuilds only on `UGP_MinimapPresenter::OnMinimapPresentationChanged` (Revision / metadata) and on bind/unbind
- Background load completes independently and only swaps the texture brush
- `SGPMinimapSurface` `SetCanTick(false)`
- No Tick, no poll timer

## Orientation contract

Presenter **unchanged**:

- World `+X` → minimap `+X`
- World `+Y` → minimap `+Y`
- No rotation / mirroring / Slate Y-flip in the presenter

Widget layer **only**:

- `ScreenY = 1 - NormalizedY`
- NormalizedY = 1 is the **top** of the square
- NormalizedY = 0 is the **bottom**
- Background texture and FoW overlay share this transform and `ComputeSharedMapDestLocal`

## Tests

Headless `-game -unattended -nop4 -NullRHI` `L_PrototypeArena`. No quit. Editor killed after Complete.

| Command | Result |
| --- | --- |
| `gp.UI.RunMinimapSurfaceContractTest` | Complete Failures=0 |
| `gp.UI.RunMinimapPresentationContractTest` | Complete Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | Complete Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Complete Failures=0 |

HUD subsystem / `UGP_HUDRootWidget` were **not** modified in this checkpoint; Selection / ContextAction regressions were not required.

Surface contract covers: safe without presenter, fallback background, async path does not sync-load, ready presenter FoW samples, bounded 128², Unexplored/Explored/Visible distinct, Y flip, shared dest/letterbox, revision rebuild, rebind without duplicate delegates, teardown, no Tick, UMG-placeable class.

## GPEditor build

| Command | Result |
| --- | --- |
| `GPEditor Win64 Development` (UHT included) | **Passed** |
| `GP Win64 Development` | **not run** (intermediate checkpoint) |
| `GP Win64 Shipping` | **not run** (intermediate checkpoint) |

## Changed files (`origin/ui/gp-minimap` previous tip `73111e1` → this checkpoint)

- `GP/Source/GPUIRuntime/Public/Widgets/GPMinimapWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPMinimapWidget.cpp`
- `GP/Source/GPUIRuntime/Public/Settings/GPUIPresentationSettings.h`
- `GP/Source/GPUIRuntime/Private/Debug/GPMinimapSurfaceContractTest.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md` (this report commit)

## Protected-file audit

**Not committed:** `WBP_GP_HUD`, `WBP_GP_PurchaseRow`, `WBP_GP_SelectionGroupRow`, `WBP_GP_LaunchContainerRow`, `Content/`, `Config/`, authored maps, Materials, VFX, `GP.uproject`, `Tools/`.

No new image asset. No `DefaultGame.ini` write. No destructive git on the dirty main working copy.

## Exact operator UMG insertion steps

In protected `WBP_GP_HUD` (local, uncommitted):

1. Open `WBP_GP_HUD`.
2. In the bottom-left minimap container, **delete the temporary debug TextBlocks**.
3. Add **GP Minimap** (`UGP_MinimapWidget`) into that container.
4. Anchors: fill the square slot (Anchors min/max 0–1, offsets 0). Size to fill.
5. Project Settings → Game → **GP UI Presentation** → `Minimap Background Texture`: assign a static map image whose XY bounds match the FoW grid. World +Y / NormalizedY=1 at the **top** of the image. Leave empty to PIE with the dark fallback.
6. Save the Widget Blueprint locally. Do not commit it.
7. PIE. Confirm: dark fallback or authored image; FoW overlay Unexplored black / Explored dim / Visible clear as units move. No Tick-driven flicker.

No Blueprint graph is required for bind. Do not expect blips, camera rectangle, or click-to-pan yet.

## Remaining known non-blocking limitations

- Surface ≠ complete minimap.
- Blips, camera rectangle, click-to-pan, last-known actors remain later checkpoints.
- Authored `WBP_GP_HUD` remains operator-local and uncommitted.
- Notifications and production end-of-match remain outside this slice.
