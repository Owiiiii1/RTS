# Cursor Work Report — FoW Smooth Contour Reconstruction

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`
- Prior correction head (enemy hiding / health-bar composition / square feather): `857a97a4abe49ae25e9dabfa33746712d5248acc`
- Contour rewrite implementation/report head: commit containing this report

## Operator retest of the previous slice

- Enemy presentation hiding = PASS
- Full-health health-bar policy = PASS
- Three FoW states = PASS

## Previous square-feather approach — rejected

The 0.22-cell (44 cm) projected-run edge feather only blurred square corners. The FoW silhouette
still read as a 200 cm checker/grid at normal gameplay zoom. Isolated circular sight still looked
like a block of squares, not a circle. That is not the requested visual result.

The feather path was removed. Gameplay CellSize, LocalFoW revision/state, and sight simulation were
not changed to compensate.

## New contour reconstruction algorithm

`ConservativeDualMarchingSquares` in `GPFoWContourField`.

Pipeline:

1. Discrete LocalFoW grid remains the only source of truth (`UGP_LocalFoWComponent`).
2. Viewport-local sample rectangle + 1 LocalFoW pad, plus a virtual Unexplored ring so dual quads
   cover viewport cell edges.
3. Scalar presentation field at **cell centers**: Visible=0.0, Explored=0.68, Unexplored=1.0.
4. Dual marching squares interpolates between neighboring centers. Mixed dual quads are subdivided
   4× and bilinear-sampled so the iso is a curve, not one diagonal cut per square.
5. Uniform interiors coalesce. Geometry is projected through the existing world→screen Slate path.

No sight-source query, actor scan, second FoW simulation, CellSize change, or authored material.

## Why this produces genuinely rounded boundaries

Neighboring cell-center samples form a continuous scalar field. A discrete circular Visible mask
therefore yields an interpolated iso that crosses cell edges at sub-cell positions, including
non-axis-aligned directions. 4× subdivision traces the bilinear iso inside each mixed dual quad, so
overlapping disks merge as a smooth union and a diagonal explored trail reads as a rounded corridor
rather than a square-run silhouette.

## Conservative visibility rule

`ConservativeBoundaryT = 0.42` (must remain `< 0.5`).

Each iso vertex is placed 42% of the way from the clearer cell center toward the darker neighbor.
The shared dual edge is at 50%, so the contour stays inside the less-obscured cell:

- visual Visible may shrink slightly (~0.08 cell / ~16 cm on a 200 cm grid at a 1-cell boundary);
- it does not create a useful information leak into gameplay Unexplored/non-Visible cells;
- a gameplay Unexplored cell center remains fully obscured (sample = 1.0).

Saddles stay separated. LocalFoW data and revision are never written.

## Visual quality tests (synthetic LocalFoW masks only)

`gp.FoW.RunWorldVisualizationContractTest` W1–W9:

1. discrete circle → interpolated contour (not four rectangle sides)
2. non-axis-aligned edges exist
3. more than four direction buckets; not a cell-rectangle trace
4. radial distance variance bounded (mean ~6–9 cells for r=8; range `< 2.2` cells)
5. overlapping circular masks produce a smooth union
6. diagonal explored trail is not a square-run silhouette
7. Unexplored cell centers stay ≥ 0.999; interior Visible stays ~0
8. LocalFoW data/revision unchanged
9. sampled-cell / triangle / iso-segment caps remain bounded

H1–H7 enemy/health presentation proofs remain.

## Performance bounds

| Item | Bound / rule |
| --- | --- |
| Sampled LocalFoW cells | viewport rectangle, max 65,536 |
| LocalFoW pad | +1 cell |
| Virtual Unexplored ring | +1 presentation-only dual-coverage ring |
| Mixed-quad subcells | 4 |
| Iso segments | max 32,768 |
| Overlay triangles | max 65,536 |
| Slate batch | 8,000 quads/batch equivalent |
| Rebuild | LocalFoW render serial **or** camera/view projection change |
| Cache | reuse while serial + view unchanged |

No per-cell UObject/component, no world scan, no 1M-cell per-frame processing.

`gp.FoW.VisualDump` reports algorithm, sampled/padded cells, pad, contour segments, overlay
vertices/triangles, mixed/coalesced counts, conservative T, subcells, dirty/cached serials.

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

- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWContourField.h` (new)
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWContourField.cpp` (new)
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
   square block.
2. Overlapping sources should merge into a smooth union.
3. A moving unit's explored trail should look like a rounded corridor/capsule, not a checker.
4. Unexplored stays black; Explored dim/grey; Visible normal.
5. Own units always visible; enemies only in LocalFoW Visible; full-HP bars hidden.
6. Two-player masks remain team-isolated. Building placement and authoritative FoW unchanged.
7. `gp.FoW.VisualDump` shows `ConservativeDualMarchingSquares`, T=0.42, SubcellsPerCell=4.

**NOT MERGED. NOT FINALIZED.**
