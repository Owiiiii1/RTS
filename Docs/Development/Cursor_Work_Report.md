# Cursor Work Report — FoW High-Resolution Presentation Raster

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`
- SDF/contour implementation head (operator-rejected): `012bd641ed83b06f702c676ca7511ce50471c2c0`
- High-res raster implementation head: `a30c4bb5393ba1f4c31b6c17d54cc368a912ad75`
- Final branch head: report-record commit following the raster implementation head

## Operator rejection of the SDF/contour architecture

The SDF + Chaikin contour path was rejected:

- the FoW silhouette still followed large 200 cm grid steps;
- camera movement could drop the overlay into conservative full-black fallback;
- recovering the visible area by moving the camera was difficult;
- the implementation was more complex than the visual result justified.

A screenshot showed the camera-loss full-black failure. Tuning SDF/Chaikin further was not allowed.

## New visual strategy

Gameplay FoW remains **200 cm** cells. Authority, LocalFoW protocol, and grid dimensions are unchanged.

Presentation now builds a viewport-local high-resolution raster instead of extracting smooth topology
from the coarse grid.

Pipeline: LocalFoW discrete state → viewport-local Known/Visible raster → bilinear upsample →
separable box blur → coalesced Slate overlay.

## Presentation resolution

- Target supersample: **10×** per gameplay-cell axis (minimum 4× if the pixel cap requires it)
- Effective presentation texel: **20 cm** at 10× on a 200 cm cell
- Interpolation: bilinear of cell-center Known/Visible samples
- Blur: separable box, radius **8** presentation texels → **160 cm** transition band
- Mask model: Known = Explored|Visible, Visible = Visible only
- Compose: black outside Known, dim grey for Known-not-Visible, clear for Visible

## Camera stability

The overlay no longer depends on a contour cache tied to an old sample rectangle.

- Pan/zoom/yaw resample the current viewport-local raster.
- Ground deproject uses four corners plus view center; skyward rays fall back to a look-direction
  ground estimate instead of failing the whole frame to full black.
- Extra pad is applied when fewer than four true ground hits succeed.
- If a rebuild still fails, the last successful overlay is kept rather than switching to permanent
  full-black.

## Performance

Viewport-local only. No 10,000×10,000 world mask.

| Item | Bound |
| --- | --- |
| Sampled gameplay cells | max 65,536 |
| Presentation pixels | max 262,144 |
| Overlay quads | max 16,384 |
| Slate batch | 8,000 quads/batch |
| Pad | 2 gameplay cells |
| CPU raster | 3 float planes × raster pixels |

A typical 40×25 cell view at 10× is ~100,000 pixels and is cheap. Overlay uses coalesced horizontal
runs, not one Slate quad per texel. No authored material; no runtime texture.

## Removed obsolete contour code

Deleted `GPFoWContourField` (Felzenszwalb SDF, Chaikin, loop stitch, self-intersection, iso bias,
contour projection cache). Replaced by `GPFoWPresentationRaster`.

## Validation

- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunHealthBarContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunTeamColorContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**

No GP Win64 Development / Shipping (reserved for finalization after operator PASS).

## Changed files

Production:

- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWPresentationRaster.h` (new)
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWPresentationRaster.cpp` (new)
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWContourField.h` (deleted)
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWContourField.cpp` (deleted)
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWWorldPresentationSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPFoWWorldOverlayWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPFoWWorldOverlayWidget.cpp`

Contracts:

- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`

Documentation:

- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/Cursor_Work_Report.md` (this report)

## Protected content

No edits under `GP/Config`, `GP/Content`, or `Tools` were staged or committed.

Existing local Config, map, Blueprint, DataAsset, material, VFX, Tools, and other Content changes
remain unstaged and untouched. The operator-local LongRange Salvage Walker UnitDefinition with
`Fog Of War Sight Radius = 2000` was not committed, reverted, restored, stashed, cleaned, or modified.

## Operator retest

1. Isolated Worker / LongRange sight should read as a soft rounded region (900 cm / 2000 cm), not a
   200 cm staircase.
2. Camera pan, zoom, and yaw must keep the mask aligned; the overlay must not drop to full-black.
3. Moving outside the previous sampled area must resample, not stay black.
4. Unexplored black; Explored dim/grey; Visible clear, with a ~160 cm soft band.
5. Own units always visible; enemies only in LocalFoW Visible; full-HP bars hidden.
6. Two-player masks remain team-isolated.
7. `gp.FoW.VisualDump` shows `BilinearUpsampleSeparableBoxBlur`, PresentationSupersample=10,
   PresentationTexelWorldSize=20, BlurRadiusSamples=8, BlurRadiusCm=160, FallbackActive=false.

**NOT MERGED. NOT FINALIZED.**
