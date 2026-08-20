# Cursor Work Report — FoW SDF World Visualization

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`
- Prior correction head (enemy hiding / health-bar composition): `857a97a4abe49ae25e9dabfa33746712d5248acc`
- Rejected marching-squares contour head: `9b7f62632992eaf673b4113703f649e4b16a220f`
- SDF implementation head: `012bd641ed83b06f702c676ca7511ce50471c2c0`
- Final branch head: report-record commit following the validated SDF implementation head

## Operator retest of the previous slice

- Enemy presentation hiding = PASS
- Full-health health-bar policy = PASS
- Three FoW states = PASS

## Previous marching-squares approach — rejected

`ConservativeDualMarchingSquares` still produced a 200 cm staircase with a wide blur. Isolated circular
sight did not read as a circle. Operator rejected further blur, more MS subdivisions, changing only
`ConservativeBoundaryT`, extra feather quads, and raising gameplay grid resolution.

That path was removed. Gameplay CellSize, LocalFoW revision/state, and sight simulation were not changed
to compensate.

## New contour reconstruction algorithm

`ConservativeKnownVisibleSDFChaikin` in `GPFoWContourField`.

Mask model: `KnownMask+VisibleMask`. Distance transform: `FelzenszwalbParabolicEDT`.

Pipeline:

1. Discrete LocalFoW grid remains the only source of truth (`UGP_LocalFoWComponent`). No unit/sight/actor scan.
2. Viewport-local sample rectangle + 6 LocalFoW pad cells.
3. Two binary masks: Known = Explored|Visible, Visible = Visible. Separate signed distance fields.
4. Inward iso-contours (`VisibleInwardBiasCells=0.40`, `KnownInwardBiasCells=0.35`, both `< 0.5`).
5. Closed loops are Chaikin-smoothed (3 iterations) and snapped back onto/inside the iso.
6. Sample-rect black with Known holes, Known grey with Visible holes, plus a **narrow** AA ribbon
   (`EdgeFeatherCm=28`, about 5–15 px / ~28 cm). Not a wide blur of a staircase.
7. World triangles are cached on LocalFoW revision (or when the view leaves the padded sample). Camera
   motion only reprojects cached world triangles.

No sight-source query, actor scan, second FoW simulation, CellSize change, or authored material.

## Why this produces genuinely rounded boundaries

A Euclidean SDF of a discrete disk has a near-circular iso. Chaikin then shortens remaining raster
corners. Overlapping disks union in the SDF before the iso is extracted. A diagonal explored trail
becomes a rounded corridor; a 90° raster L-corner is rounded rather than two cell walls.

Radial variance of an r=8 discrete circle, after arc-length resampling, is substantially tighter than
the raw raster boundary. Long 200 cm horizontal/vertical staircase runs are not present.

## Conservative visibility rule

Inward iso bias 0.35–0.40 cell (`< 0.5`):

- visual Visible/Known may shrink slightly;
- hidden cell centers stay fully obscured (`SamplePresentationObscuration >= 0.999`);
- LocalFoW data and revision are never written.

## Visual quality tests (synthetic LocalFoW masks only)

`gp.FoW.RunWorldVisualizationContractTest` W1–W9 (+ W6B corner, S2 mask/projection split):

1. discrete circle → closed Chaikin contour, no self-intersections
2. broad tangent-direction coverage
3. no long cell-length HV staircase; smoothed vertex count exceeds raw
4. resampled radius stddev / max-min improved vs raw raster boundary (mean ~6–9 cells for r=8)
5. overlapping disks → one smooth union loop
6. diagonal explored trail is not a square-run silhouette
6B. 90° raster L-corner is rounded
7. Unexplored cell centers stay ≥ 0.999; interior Visible stays ~0
8. LocalFoW data/revision unchanged
9. sampled-cell / triangle / iso / SDF-pixel caps remain bounded; pad=6

H1–H7 enemy/health/team isolation proofs remain.

## Performance bounds

| Item | Bound / rule |
| --- | --- |
| Sampled LocalFoW cells | viewport rectangle, max 65,536 |
| LocalFoW pad | +6 cells |
| SDF pixels | max 262,144 (2× supersample of the padded sample) |
| Iso segments | max 32,768 |
| Overlay triangles | max 65,536 |
| Slate batch | 8,000 quads/batch equivalent |
| Edge AA | 28 cm (~5–15 px), not a wide staircase blur |
| Mask rebuild | LocalFoW revision **or** view leaving the padded sample |
| Projection rebuild | camera/view change only; reprojects cached world triangles |

No per-cell UObject/component, no world scan, no 1M-cell per-frame processing.

`gp.FoW.VisualDump` reports Algorithm, MaskModel, DistanceTransform, Origin, Dims, CellSize,
MaxSampledCells, SampledCells, PaddedCells, PadCells, DistanceField, DistanceFieldBytes,
ContourRawVertices, ContourSmoothedVertices, OverlayVertices, OverlayTriangles,
VisibleInwardBiasCm, KnownInwardBiasCm, EdgeFeather, LastMaskRevision, MaskRebuilt,
ProjectionRebuilt, MaskRebuildMs, MaxTriangles, MaxSdfPixels, plus dirty/cached serials.

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

- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWContourField.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWContourField.cpp`
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

## Updated operator retest

1. Isolated Worker / LongRange sight should read as a round region (900 cm / 2000 cm), not a 200 cm
   staircase with a wide blur.
2. Overlapping sources should merge into a smooth union.
3. A moving unit's explored trail should look like a rounded corridor/capsule, not a checker.
4. Unexplored stays black; Explored dim/grey; Visible normal. AA at the edge is a narrow 28 cm band.
5. Own units always visible; enemies only in LocalFoW Visible; full-HP bars hidden.
6. Two-player masks remain team-isolated. Building placement and authoritative FoW unchanged.
7. `gp.FoW.VisualDump` shows `ConservativeKnownVisibleSDFChaikin`, `KnownMask+VisibleMask`,
   `FelzenszwalbParabolicEDT`, pad=6, EdgeFeather=28cm, and MaskRebuilt vs ProjectionRebuilt.

**NOT MERGED. NOT FINALIZED.**
