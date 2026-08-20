# Cursor Work Report — Fog of War World Visualization

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Operator decision

The previous attempt was rejected because it restored mask/overlay behavior.

`BlurredRasterOverlay` sampled a presentation field and projected coalesced strips across the view.
That reintroduced the old left-side striping. It is not the requested visual.

The operator wants FoW drawn as many small cells: each Unexplored/Explored cell is its own quad tile
with feathered edges. Visible cells are not drawn. A still-cellular look is acceptable.

## Final active renderer

**PerCellBlurredQuadRenderer**

Exact path:

1. `UGP_FoWWorldPresentationSubsystem` (local-player owner)
2. `UGP_FoWWorldOverlayWidget` (hit-test-invisible viewport overlay)
3. Per-cell world-space quads with neighbor-aware edge/corner feathers

- **PostProcessActive=false**
- **MaskProjectionActive=false**
- No fullscreen mask
- No sampled raster bands
- No coalesced row-strip surface
- No post-process material
- No SDF / contour / marching squares

Unexplored tiles are black with feathered edges. Explored tiles are dim grey with feathered edges.
Same-state neighbors abut solidly so they merge; edges toward Visible (and Unexplored toward Explored)
feather out.

## Canonical values

- CellSize = **50 cm**
- Dims = **4000 × 4000**
- Interval = **0.10 sec (10 Hz)**
- World origin unchanged: `(-100000, -100000)`

BuildGrid remains 200 cm. That is a different grid.

## Tests / build

| Check | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASS** |
| `gp.FoW.RunRuntimeFoundationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunWorldVisualizationContractTest` | **PASS**, Failures=0 |
| `gp.Combat.RunHealthBarContractTest` | **PASS**, Failures=0 |
| `gp.Combat.RunTeamColorContractTest` | **PASS**, Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | **PASS**, Failures=0 |
| `gp.Building.RunBuildGridContractTest` | **PASS**, Failures=0 |
| GP Win64 Development / Shipping | **not run** (not requested) |

UnrealEditor-Cmd `-ExecCmds` still hangs after Complete; process kill / exit `4294967295` is the
known harness artifact, not a contract failure.

## Protected content confirmation

Not modified:

- `GP/Config/*`
- maps / Blueprints / DataAssets
- VFX / Tools
- LongRange UnitDefinition sight radius 2000

## Changed files (implementation)

- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWPresentationRaster.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWPresentationRaster.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWWorldPresentationSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPFoWWorldOverlayWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPFoWWorldOverlayWidget.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`
- `GP/Source/GPRuntime/Private/FogOfWar/GPFogOfWarComponent.cpp`
- `GP/Source/GPRuntime/Private/FogOfWar/GPLocalFoWComponent.cpp`

## Changed files (docs)

- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

**NOT MERGED. NOT FINALIZED.**
