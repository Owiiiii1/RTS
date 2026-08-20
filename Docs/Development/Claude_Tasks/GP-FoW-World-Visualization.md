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

- CellSize = **100 cm**
- Dims = **2000 × 2000**
- Interval = **0.10 sec (10 Hz)**
- world origin unchanged `(-100000, -100000)`
- 4× the original 200 cm / 1000×1000 cell count (not 16×)

## Active renderer

`UGP_FoWWorldPresentationSubsystem` + `UGP_FoWWorldOverlayWidget` + per-cell feathered quads

- **Renderer=`PerCellBlurredQuadRenderer`**
- **PostProcessActive=false**
- **MaskProjectionActive=false**
- Algorithm=`PerCellFeatheredQuads`
- only Unexplored and Explored cells emit tiles
- neighbor-aware edge/corner feathers
- viewport-local sample cap 65536 cells / 262144 quads
- visible region is never cropped; over-cap frames are full black

Enemy LocalFoW gating remains a separate presentation gate.

## Abandoned

- post-process FoW
- projected / sampled fullscreen mask
- coalesced row-strip overlay
- SDF / contour / marching squares
- debug tint path

## Diagnostics

`gp.FoW.DebugDump`, `gp.FoW.LocalDump`, and `gp.FoW.VisualDump` report:

- CellSize=100
- Dims=2000x2000
- Interval=0.10
- Renderer=PerCellBlurredQuadRenderer
- PostProcessActive=false
- MaskProjectionActive=false

## Validation

- GPEditor Win64 Development + UHT — **PASS** (this correction)
- Focused contract re-run not requested for this grid/crop correction; expects updated to 100 cm / 2000×2000 / 0.10

No Config, maps, Blueprints, DataAssets, VFX, or Tools were modified.
LongRange UnitDefinition sight=2000 was not touched.

**NOT MERGED. NOT FINALIZED.**
