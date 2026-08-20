# Cursor Work Report — Fog of War World Visualization

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Operator decision

Operator stopped the post-process direction.

The post-process terrain fog renderer is rejected: it did not visibly apply (map stayed fully lit)
and stuttered. Enemy LocalFoW hiding still worked and is preserved.

This change reverts conceptually to the earlier simple square blurred renderer and adopts the
previously deferred 4× denser gameplay grid.

SDF / contour / Chaikin / marching-squares reconstruction remains abandoned.

## Final canonical values

- CellSize = **50 cm**
- Dims = **4000 × 4000**
- Interval = **0.10 sec (10 Hz)**
- World origin unchanged: `(-100000, -100000)`

Applied to authority FoW, LocalFoW mirror, visibility queries, auto-acquire, building placement
checks, debug dumps, and world visualization.

BuildGrid remains 200 cm. That is a different grid.

## Active renderer

Exact path now active:

1. `UGP_FoWWorldPresentationSubsystem` (local-player owner)
2. `UGP_FoWWorldOverlayWidget` (hit-test-invisible viewport overlay)
3. `GPFoWPresentationRaster` (`BilinearUpsampleSeparableBoxBlur`)

- **Renderer=`BlurredRasterOverlay`**
- **PostProcessActive=false**
- Viewport-local sampling (max 65536 cells, 262144 presentation pixels, 16384 quads)
- Target supersample 4, separable box blur radius 12
- Unexplored = black, Explored = dim grey, Visible = clear
- Camera pan/zoom/rotate resamples; no post-process injection; no world-position material

This is deliberately a square-based but much finer and strongly blurred look. It is not a
mathematically perfect round fog edge.

## Post-process path

Removed / disabled:

- `GPFoWVisualMask` runtime texture path
- `UGP_LocalFoWComponent::BuildPresentationMaskRGBA`
- `M_GP_FoW_PostProcess` and `GPFoWPostProcessMaterialSeedCommandlet`
- camera blendable binding, debug tint, SceneDepth world reconstruction
- RHI/RenderCore FoW presentation dependencies

No competing terrain FoW renderer remains active.

Enemy visibility gating by LocalFoW is unchanged and still working.

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

The branch-owned unused FoW post-process material was removed from this branch.

## Changed files (implementation)

- `GP/Source/GPRuntime/Public/FogOfWar/GPFogOfWarComponent.h`
- `GP/Source/GPRuntime/Private/FogOfWar/GPFogOfWarComponent.cpp`
- `GP/Source/GPRuntime/Public/FogOfWar/GPLocalFoWComponent.h`
- `GP/Source/GPRuntime/Private/FogOfWar/GPLocalFoWComponent.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPFoWRuntimeFoundationContractTest.cpp`
- `GP/Source/GPUIRuntime/GPUIRuntime.Build.cs`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWWorldPresentationSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWPresentationRaster.h` (restored)
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWPresentationRaster.cpp` (restored)
- `GP/Source/GPUIRuntime/Public/Widgets/GPFoWWorldOverlayWidget.h` (restored)
- `GP/Source/GPUIRuntime/Private/Widgets/GPFoWWorldOverlayWidget.cpp` (restored)
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWClientPresentationFoundationContractTest.cpp`
- deleted `GPFoWVisualMask` + post-process seed commandlet + `M_GP_FoW_PostProcess.uasset`

## Changed files (docs)

- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Cursor_Work_Report.md`

**NOT MERGED. NOT FINALIZED.**
