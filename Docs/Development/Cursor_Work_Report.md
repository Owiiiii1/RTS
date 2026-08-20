# Cursor Work Report — Fog of War World Visualization

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Correction

50 cm / 4000×4000 accidentally created a 16× cell count versus the original 200 cm / 1000×1000 grid
and caused excessive slowdown. Canonical gameplay FoW is now 4× the original cell count:

- CellSize = **100 cm**
- Dims = **2000 × 2000**
- Interval = **0.10 sec (10 Hz)**

Renderer remains **PerCellBlurredQuadRenderer**. No post-process. No fullscreen mask. No SDF/contour.
Feather fraction stays 0.45 of cell size (45 cm at 100 cm cells).

## Viewport cropping removed

`UGP_FoWWorldOverlayWidget::RebuildProjectedOverlay` no longer shrinks MinCell++/MaxCell-- when the
visible region exceeds the sample cap. That crop was leaving the top/bottom of the screen without fog.

Safety cap:

- `GetMaximumSampledCells() = 65536`
- `GetMaximumOverlayQuads() = 262144`
- if the complete required viewport region exceeds the sampled-cell cap, or quad emission would
  overflow, that frame uses conservative full-black fallback
- the visible FoW region is never partially revealed

## Tests / build

| Check | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASS** |
| GP Win64 Development / Shipping | **not run** (not requested) |

Focused contract re-run was not requested for this correction. Contract expects were updated to
CellSize=100 / Dims=2000×2000 / Interval=0.10 and the 65536 sample cap.

## Protected content confirmation

Not modified:

- `GP/Config/*`
- maps / Blueprints / DataAssets
- VFX / Tools
- LongRange UnitDefinition sight radius 2000

## Changed files

- `GP/Source/GPRuntime/Public/FogOfWar/GPFogOfWarComponent.h`
- `GP/Source/GPRuntime/Private/Debug/GPFoWRuntimeFoundationContractTest.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWPresentationRaster.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWPresentationRaster.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPFoWWorldOverlayWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWClientPresentationFoundationContractTest.cpp`
- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

**NOT MERGED. NOT FINALIZED.**
