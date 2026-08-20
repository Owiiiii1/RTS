# GP — Fog of War World Visualization

**Status:** `FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION`
**Branch:** `feature/gp-fow-world-visualization`
**Base:** `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`

## Goal

Make the trusted one-team local FoW physically visible over the current MVP arena:

- Unexplored: opaque black tiles;
- Explored: dim/grey tiles;
- Visible: not drawn.

## Operator decision

The previous sampled/projected mask overlay is **wrong**. It restored a large mask/strip surface
across the view and reintroduced left-side striping.

Correct target: **simple per-cell renderer**.

- FoW is drawn as many small cell tiles;
- each non-Visible cell is its own quad with feathered edges;
- neighboring tiles blend because of the feather;
- the look may remain cell-based;
- no fullscreen mask, post-process, SDF, contours, or coalesced strip projection.

## Canonical gameplay grid

- CellSize = **50 cm**
- Dims = **4000 × 4000**
- Interval = **0.10 sec (10 Hz)**
- world origin unchanged `(-100000, -100000)`

## Active renderer

`UGP_FoWWorldPresentationSubsystem` + `UGP_FoWWorldOverlayWidget` + per-cell feathered quads

- **Renderer=`PerCellBlurredQuadRenderer`**
- **PostProcessActive=false**
- **MaskProjectionActive=false**
- Algorithm=`PerCellFeatheredQuads`
- only Unexplored and Explored cells emit tiles
- neighbor-aware edge/corner feathers
- viewport-local sample cap 16384 cells / 98304 quads

Enemy LocalFoW gating remains a separate presentation gate.

## Abandoned

- post-process FoW
- projected / sampled fullscreen mask
- coalesced row-strip overlay
- SDF / contour / marching squares
- debug tint path

## Diagnostics

`gp.FoW.DebugDump`, `gp.FoW.LocalDump`, and `gp.FoW.VisualDump` report:

- CellSize=50
- Dims=4000x4000
- Interval=0.10
- Renderer=PerCellBlurredQuadRenderer
- PostProcessActive=false
- MaskProjectionActive=false

## Validation

- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunHealthBarContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunTeamColorContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**

No Config, maps, Blueprints, DataAssets, VFX, or Tools were modified.
LongRange UnitDefinition sight=2000 was not touched.

**NOT MERGED. NOT FINALIZED.**
