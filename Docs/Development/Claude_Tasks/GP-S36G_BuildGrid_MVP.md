# GP-S36G — BuildGrid MVP

## Status
**GP-S36G_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

## Slice Group
Post-GP-S35B (Multi-Building Data Architecture is on verified `main` @ `6f258a1069fd92a45f99faf7c877c941528beb2a`)

## Branch
`feature/gp-s36g-buildgrid-mvp`  
Base: `main` @ `6f258a1069fd92a45f99faf7c877c941528beb2a`  
Implementation: `0acd4930a43b11d7065b56fea75781dec3f12e2d`

## Goal
Replace GP-S32R/GP-S35B interim free-form building placement with canonical BuildGrid occupancy + snap. Orbital Purchase → READY → Deploy → DropPod semantics stay unchanged. This is a grid/placement architecture slice, **not** Wall gameplay.

## Implemented facts

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

## Server validation sequence
1. DropDefinition valid
2. BuildingDefinition loaded
3. FootprintCells > 0
4. Payload class resolves
5. Finite transform
6. MainBase exists
7. Snap requested location on server (client OriginCell is not trusted)
8. Max deploy radius from MainBase on **snapped** XY
9. All footprint cells free / unreserved
10. NavMesh rule above
11. Environmental overlap sanity
12. Reserve → spawn DropPod at snapped ground + yaw 0 → consume READY → init pod with OriginCell/Footprint/ReservationId

## Client ghost
Cursor ground trace → shared `UGP_BuildGridSubsystem::ResolveSnappedPlacement` → ghost at snapped center. Cube scaled to `FootprintCells * 200 cm`. Local occupancy/nav/world query tints green/red; server remains authority.

## Operator validation target
1. Purchase Logistics Hub
2. Enter Deploy — ghost snaps in 200 cm steps
3. Place on free cells — DropPod lands on snap, Hub appears, MaxUnits +5
4. Purchase another Hub; overlapping first is rejected; READY stays 1
5. Adjacent free cells accepted
6. Stop PIE and close Editor — no shutdown crash regression

## Tests
`gp.Building.RunBuildGridContractTest` + listed regressions: **Failures=0**. GPEditor Win64 Development + UHT **PASS**. GP Development / Shipping **not run** (candidate stage).

## Stop condition
Await operator PIE validation. **NOT MERGED.** Human merge only. Do not start Wall gameplay / FoW / turret combat.
