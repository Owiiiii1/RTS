# GP-S36G — BuildGrid MVP

## Status
**GP-S36G_VISUAL_FEEDBACK_READY_FOR_OPERATOR_VALIDATION**

This is **not** finalization. **NOT MERGED.**

## Slice Group
Post-GP-S35B (Multi-Building Data Architecture is on verified `main` @ `6f258a1069fd92a45f99faf7c877c941528beb2a`)

## Branch
`feature/gp-s36g-buildgrid-mvp`  
Base: `main` @ `6f258a1069fd92a45f99faf7c877c941528beb2a`  
Logic implementation: `0acd4930a43b11d7065b56fea75781dec3f12e2d`  
Prior remote feature head (before visual patch): `5c05cd68034eb44ef9d38ce5708f73449693e854`

## Goal
Replace GP-S32R/GP-S35B interim free-form building placement with canonical BuildGrid occupancy + snap. Orbital Purchase → READY → Deploy → DropPod semantics stay unchanged. This is a grid/placement architecture slice, **not** Wall gameplay.

Visual feedback in this patch is presentation-only: operator must see which cells will be occupied and why a placement is valid/invalid.

## Operator logic validation (PASS — do not change mechanics)

Operator manually confirmed:

| ID | Result |
| --- | --- |
| A | Logistics Hub ghost snaps discretely on the 200 cm grid — **PASS** |
| B | First Hub deploys on free cells — **PASS** |
| C | Second Hub cannot be deployed overlapping an already-landed Hub — **PASS** |
| D | Rejected overlap does not consume READY — **PASS** |
| E | Second Hub can be deployed on adjacent free cells — **PASS** |
| F | In-flight reservation: while first Hub DropPod is still descending and payload has not spawned, second Hub cannot be deployed onto the same footprint — **PASS** |
| G | MainBase occupancy: Hub cannot be deployed through/over MainBase footprint — **PASS** |
| H | Occupancy release: after placed Hub is destroyed/removed, previous cells become available and another Hub can be deployed there — **PASS** |

Occupancy, reservation lifecycle, WorldToCell, footprint anchor math, READY, Purchase, DropPod timing, Hub +5, MainBase fallback, NavigationObstacle, Walls, and FoW were **not** changed in the visual patch.

## Implemented grid facts (unchanged)

| Topic | Implementation |
| --- | --- |
| Class | `UGP_BuildGridSubsystem : UWorldSubsystem` (`Buildings/Grid/`) |
| Cell size | `200.0f` cm |
| Grid origin XY | World `(0,0)` — no per-frame NavMesh origin projection; Z is never encoded in cell coordinates |
| WorldToCell | `Floor(World/CellSize + 0.5)` per axis (half-open Voronoi around cell centers at `n * 200`) |
| OriginCell | Min/anchor cell of an axis-aligned footprint; cells `Origin .. Origin+Size-1` |
| Footprint center | `Origin * 200 + (Size-1) * 100` — even footprints (4×4) sit between cells |
| Occupancy | Server-only `FGuid → cells` + `cell → {Guid, weak actor, reservation flag}`. Subsystem is **not** replicated |
| Reservation | `FGuid` created after deploy accept, bound to DropPod, promoted to building on payload spawn; EndPlay / skipped payload releases |
| MainBase | Compatibility fallback footprint **5×5** from actor location; no BuildingDefinition asset required |
| Ferronite Deposit | `AGP_ResourceNode` is **not** `AGP_BuildingBase`. No 3×3 grid registration. Existing WorldStatic/WorldDynamic overlap remains the environmental blocker |
| Rotation | Axis-aligned only. Ghost/pod/building yaw forced to `0`. No footprint rotation |
| ClearanceCells | **Deferred** (not added to `UGP_BuildingDefinition`) |
| NavMesh | Project footprint **center** with extent `(100,100,300)`. Success = navigable. Fail + WorldStatic ground hit = `NotNavigable`. Fail + empty void = allow (contract isolation) |
| World collision | Raised footprint box vs WorldStatic/WorldDynamic, ignoring buildings/pods/pawns. Structure-vs-structure SoT is grid occupancy |
| FoW | **Deferred to FoW integration slice.** No fake visibility |
| Walls | **Deferred.** No `AGP_Wall`, drag, A*, mounting, wall clearance |
| READY | Invalid grid placement does not consume READY and does not spend Orbital again. Accepted deploy consumes READY exactly once |
| Hub +5 | Unchanged native `AGP_LogisticsHub` live/operational apply/remove |
| NavigationObstacle | Unchanged authored box / `NavArea_Null` dynamic obstacle. BuildGrid is not a Recast replacement |

## Visual feedback (this patch)

Operator issue: cube MID tint via vector parameter `"Color"` is not a reliable validity signal. Engine BasicShapes Cube material is **not assumed** to expose `Color`. That tint remains an optional extra only.

Primary path is asset-independent (no `.uasset`, no `/Game/...` materials):

- `AGP_BuildingPlacementGhost::UpdateGridPreview` draws the snapped footprint through a local `ULineBatchComponent` (outer border + internal 200 cm cell divisions). Lines use direct green `FColor(32,220,72)` / red `FColor(230,36,32)`, slightly above ground (`local Z = 24`) to avoid z-fighting.
- `UTextRenderComponent` above the ghost shows `VALID` or compact `BLOCKED: …` text in the same color.
- Translucent Engine Cube fill remains as a flat footprint mass. Line batch and status text attach to an unscaled `SceneRoot` so cube scale cannot distort them.
- 4×4 Hub = 800×800 cm outer AABB, 16 cells, 10 lines (4 border + 3 vertical + 3 horizontal internals). Math comes from `OriginCell` / `FootprintCells` / `UGP_BuildGridSubsystem::GetCellSize` / `GetFootprintWorldAABB`. No second footprint formula in PlayerController. No world-wide grid overlay.

Local preview result (`GPBuildingDropAuthority::FPlacementPreview`): `bValid` + `EGP_BuildingDropRejectReason`. Player-facing labels:

| Result | Text |
| --- | --- |
| valid | `VALID` |
| `GridOccupied` | `BLOCKED: OCCUPIED` |
| `OutOfDeployRadius` | `BLOCKED: OUT OF RANGE` |
| `NotNavigable` | `BLOCKED: NOT NAVIGABLE` |
| `PlacementOverlap` | `BLOCKED: WORLD` |
| other reject | `BLOCKED` |

Client preview predicts. Server confirm still re-snaps and re-validates. If local says VALID and server rejects (race), server wins. Occupancy maps are not replicated.

In-flight reservation preview: DropPod replicates `BuildingGridOriginCell` + `BuildingGridFootprintSize` (tiny seam, not full BuildGrid replication). Local preview overlaps those replicated building pods plus replicated landed `AGP_BuildingBase` footprints. If those facts have not arrived yet, preview may lag; server reservation reject remains authoritative.

Out-of-range uses `UGP_OrbitalDeliverySettings::BuildingMaxDeployRadiusFromMainBaseCm` (same as server). No duplicate magic number.

Invalid local confirm: LMB does **not** send deploy RPC; placement mode stays active; READY unchanged. Contracts still call `AuthorityDeployBuilding` directly, so server rejection tests are intact.

Visuals exist only while building placement is active. Esc / RMB / successful confirm / leaving mode / PlayerController EndPlay destroy the ghost (`ClearGridPreview` + actor destroy). No stale line batch after cancel.

## Next operator visual test (not a merge gate)

1. Purchase Hub → Deploy.
2. Free area: footprint cells visible, GREEN, `VALID`.
3. Move over MainBase / existing Hub: RED, `BLOCKED: OCCUPIED`.
4. Move outside deploy radius: RED, `BLOCKED: OUT OF RANGE`.
5. Move back to valid: GREEN again.
6. Cancel Esc/RMB: all preview visuals disappear.
7. Close Editor: no crash.

## Tests
`gp.Building.RunBuildGridContractTest` extended for presentation seams (4×4 outer 800, cell/line counts, label mapping, local occupied/out-of-range, cancel clears state). No pixel tests.

Listed regressions **Failures=0**. GPEditor Win64 Development + UHT **PASS**. GP Development / Shipping **not run**.

## Stop condition
Await operator visual PIE validation. **NOT MERGED.** Do not mark finalization-ready. Human merge only after visual PASS + later finalization slice.
