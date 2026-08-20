# GP — Fog of War World Visualization

**Status:** `FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION`
**Branch:** `feature/gp-fow-world-visualization`
**Base:** `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Goal

Make the trusted one-team local FoW physically visible over the current MVP arena:

- Unexplored: opaque black;
- Explored: dim/grey terrain;
- Visible: unchanged world.

This slice is presentation-only for the overlay, plus the adopted denser gameplay grid.

## Operator decision (temporary MVP stop)

The post-process texture renderer is **rejected and abandoned**.

We stop on the earlier simple working approach, plus the previously deferred denser grid:

- square/cell-based FoW visualization;
- strong blur / soft edges;
- no post-process material;
- no SDF / contour / marching-squares reconstruction;
- no camera-bound post-process binding.

Canonical gameplay grid:

- CellSize = **50 cm**
- Dims = **4000 × 4000**
- Interval = **0.10 sec (10 Hz)**
- same world origin `(-100000, -100000)`

## Active renderer

Exactly one terrain FoW renderer is active:

`UGP_FoWWorldPresentationSubsystem` + `UGP_FoWWorldOverlayWidget` + `GPFoWPresentationRaster`

- **Renderer=`BlurredRasterOverlay`**
- **PostProcessActive=false**
- Algorithm=`BilinearUpsampleSeparableBoxBlur`
- viewport-local sample cap 65536 cells / 262144 presentation pixels
- target supersample 4 (≈12.5 cm texels) and separable box blur radius 12
- coalesced Slate quads; camera pan/zoom/rotate resamples

Enemy visibility gating by LocalFoW remains a separate local presentation gate.

## Removed / abandoned

- post-process terrain fog renderer
- runtime FoW texture / mask presentation path (`GPFoWVisualMask`, `BuildPresentationMaskRGBA`)
- post-process camera binding and debug tint
- world-position reconstruction material path
- `M_GP_FoW_PostProcess` and its seed commandlet
- SDF / contour / Chaikin / marching squares
- hybrid post-process + overlay competition

## Diagnostics

`gp.FoW.DebugDump`, `gp.FoW.LocalDump`, and `gp.FoW.VisualDump` report:

- CellSize=50
- Dims=4000x4000
- Interval=0.10
- Renderer=BlurredRasterOverlay
- PostProcessActive=false

## Validation

- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunHealthBarContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunTeamColorContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**

No Config, maps, Blueprints, DataAssets, materials, VFX, or Tools were modified.
LongRange UnitDefinition sight=2000 was not touched.

**NOT MERGED. NOT FINALIZED.**
