# Cursor Work Report — FoW Post-Process Texture Mask

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`
- Implementation head: `f715ac71a7450255e36bb997c8f43987ffad989c`
- Final branch head: this report-record commit

## Aborted 50 cm / 10 Hz plan

The previous prompt to change canonical gameplay FoW from 200 cm / 5 Hz to 50 cm / 10 Hz
(4000×4000) was **aborted by the operator before any implementation changes**.

This correction did **not** implement that plan.

Gameplay FoW remains:

- CellSize = **200 cm**
- Grid origin = (-100000, -100000)
- Dims = **1000×1000**
- Authoritative recompute = **0.20 sec / 5 Hz**
- LocalFoW mirror protocol unchanged
- Authored UnitDefinition values unchanged (operator-local LongRange sight radius 2000 preserved)

Visual smoothness is now a presentation-only problem. A future 50 cm / 10 Hz grid is deferred until
**after** operator evaluation of this renderer.

## Rejected Slate / SDF history

Operator testing rejected the native world-geometry path:

- visible 200 cm staircase;
- square silhouette despite smoothing;
- excessive blur;
- camera-dependent full-black failures;
- horizontal striping / invalid projected artifacts;
- too much custom geometry for a simple mask.

Deleted for this slice:

- SDF / marching squares / Chaikin contour code (already gone from the raster rewrite);
- viewport-local 10× presentation raster (`GPFoWPresentationRaster`);
- projected Slate run geometry (`UGP_FoWWorldOverlayWidget`);
- screen-projection / sample-rect cache.

Do not continue tuning that renderer.

## New post-process texture architecture

LocalFoW → client runtime world-space mask textures → spatial smoothing + temporal interpolation →
post-process material → SceneColor obscuration.

Owner: `UGP_FoWWorldPresentationSubsystem` (`ULocalPlayerSubsystem` + `FTickableGameObject`).

Per local view, `FGP_FoWSceneViewExtension` adds that player's MID to
`FSceneView::FinalPostProcessSettings` when the view family render target matches the LocalPlayer
viewport. No map `PostProcessVolume`. No Material Parameter Collection.

## Material asset

- `/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess`
- Seeded reproducibly by `-run=GPFoWPostProcessMaterialSeed`
- Domain: Post Process
- Blendable location: `BL_SceneColorAfterTonemapping`
- Dynamic instance created per LocalPlayer (`UMaterialInstanceDynamic`)

No existing material asset was modified.

## Runtime texture ownership

Per LocalPlayer subsystem:

- `PreviousMaskTexture` — `UTexture2D` 1024×1024 `PF_B8G8R8A8`, bilinear, clamp, non-sRGB
- `TargetMaskTexture` — same
- CPU `FGP_FoWVisualMaskRuntime` Previous/Target Known+Visible float buffers
- Upload only on LocalFoW revision via `UpdateTextureRegions`

Texture resolution is presentation-only (~195 cm / texel over 200000 cm). 2048² was not used;
1024² is the starting MVP size.

## Known / Visible encoding

- R = KnownMask (Explored OR Visible)
- G = VisibleMask (currently Visible)
- B unused, A=255

Composition (CPU helper matches the material):

- Ready=0 or Known=0 → black
- Known=1, Visible=0 → SceneColor × 0.35 (dim/grey)
- Visible=1 → unchanged SceneColor

Gameplay queries continue to use exact LocalFoW / authority bits.

## Spatial smoothing

- Filter: separable box
- Radius: 1 texel
- Passes: 2
- Approximate band: ~195–400 cm depending on bilinear sampling
- Operates on the texture-domain mask, not screen geometry
- Does not mutate LocalFoW

## Temporal interpolation

- Blend duration: **0.20 s**
- On LocalFoW revision: bake current lerp into Previous, upload new Target, `BlendAlpha=0`
- Tick advances alpha; material lerps Previous→Target
- Gameplay visibility stays exact at the new revision; only the terrain edge interpolates
- Not per-frame FoW recompute

## Per-player isolation

- One subsystem / two textures / one MID per `ULocalPlayer`
- Scene-view extension skips views whose render target is not that LocalPlayer viewport
- Team 1 listen host and Team 2 remote client cannot share a mask resource
- Contract proves two mirrors encode distinct Known/Visible buffers

## Camera integration

Camera pan / zoom / yaw / viewport resize do **not** rebuild the mask.

The shader reconstructs world XY and samples the world-mapped texture. This eliminates, by
architecture:

- left-side striping from projected Slate spans;
- losing the mask when the camera leaves a sampled rectangle;
- permanent conservative black after pan;
- invalid projected geometry.

NotReady still forces full black (`FoWReady=0`).

## Performance

| Item | Value |
| --- | --- |
| Texture resolution | 1024×1024 |
| Bytes per texture | 4,194,304 (RGBA8) |
| GPU mask textures | 2 (Previous + Target) |
| CPU float planes | Previous/Target × Known/Visible |
| Rebuild trigger | LocalFoW revision only (5 Hz authority → client deltas) |
| Spatial filter | 2-pass box, r=1, on 1024² |
| Temporal cost | scalar `BlendAlpha` per tick |
| Post-process | one after-tonemap blendable per local game view |
| Per-frame whole-world CPU | none |

Focused visualization contract ExecCmd wall time was **183 ms** for the entire proof (spawns + CPU
masks + resource checks), not a per-frame cost. Exact live `MaskBuildMs` / `MaskUploadMs` are
printed by `gp.FoW.VisualDump` after PIE.

## Validation

| Command | Result |
| --- | --- |
| `gp.FoW.RunWorldVisualizationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunRuntimeFoundationContractTest` | **PASS**, Failures=0 |
| `gp.Combat.RunHealthBarContractTest` | **PASS**, Failures=0 |
| `gp.Combat.RunTeamColorContractTest` | **PASS**, Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **PASS**, Failures=0 |
| `gp.Building.RunBuildGridContractTest` | **PASS**, Failures=0 |
| GPEditor Win64 Development + UHT | **PASS** |

No dedicated camera contract exists; camera integration is covered by the visualization contract
(`Y_CameraMotionDoesNotRebuildLocalFoWMask`) and the scene-view extension design.

No GP Win64 Development / Shipping (reserved for finalization after operator PASS).

## Changed files

Production:

- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWVisualMask.h` (new)
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWVisualMask.cpp` (new)
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWWorldPresentationSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWPresentationRaster.h` (deleted)
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWPresentationRaster.cpp` (deleted)
- `GP/Source/GPUIRuntime/Public/Widgets/GPFoWWorldOverlayWidget.h` (deleted)
- `GP/Source/GPUIRuntime/Private/Widgets/GPFoWWorldOverlayWidget.cpp` (deleted)
- `GP/Source/GPUIRuntime/GPUIRuntime.Build.cs`
- `GP/Source/GPEditor/Public/FogOfWar/GPFoWPostProcessMaterialSeedCommandlet.h` (new)
- `GP/Source/GPEditor/Private/FogOfWar/GPFoWPostProcessMaterialSeedCommandlet.cpp` (new)
- `GP/Source/GPEditor/GPEditor.Build.cs`

Contracts:

- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`

New authored asset:

- `GP/Content/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess.uasset`

Documentation:

- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md` (this report)

## Protected content

No edits under `GP/Config`, existing maps, Blueprints, DataAssets, materials, VFX, or `Tools` were
staged or committed.

Existing local Config, map, Blueprint, DataAsset, material, VFX, Tools, and other Content changes
remain unstaged and untouched. The operator-local LongRange Salvage Walker UnitDefinition with
`Fog Of War Sight Radius = 2000` was not committed, reverted, restored, stashed, cleaned, or modified.

The only new authored asset is `/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess`.

## Operator retest

1. Team 1 PIE: starting vision normal; Unexplored black; no full-map flash.
2. Move Worker / LongRange: Visible clears, Explored stays dim/grey, never returns to Unexplored.
3. Pan, zoom, yaw, fast pan past the previous view: world-locked mask, no striping, no full-black.
4. Isolated sight should read softer than a hard 200 cm square (spatial + 0.20 s temporal).
5. Own damaged health bar policy unchanged; enemy mesh/bar hidden in Explored/Unexplored.
6. Two-player listen PIE: distinct Team 1 / Team 2 masks; no shared volume.
7. `gp.FoW.VisualDump`: `Renderer=PostProcessTextureMask`, `MaskModel=Known+Visible`,
   `TextureResolution=1024`, `OldSlateRendererActive=false`, `PostProcessBound=true`, `CellSize=200`.
8. `gp.FoW.VisualEnable 0` then `1`.

**NOT MERGED. NOT FINALIZED.**
