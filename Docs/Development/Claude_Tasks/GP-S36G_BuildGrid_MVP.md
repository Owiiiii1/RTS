# GP-S36G — BuildGrid MVP

## Status
**GP-S36G_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.** Await human merge.

## Slice Group
Post-GP-S35B (Multi-Building Data Architecture is on verified `main` @ `6f258a1069fd92a45f99faf7c877c941528beb2a`)

## Branch
`feature/gp-s36g-buildgrid-mvp`  
Base: `main` @ `6f258a1069fd92a45f99faf7c877c941528beb2a`  
Finalization: `e76cc5e106eb6783547de6a0dfbb185171798b24`

## Goal
Replace GP-S32R/GP-S35B interim free-form building placement with canonical BuildGrid occupancy + snap. Orbital Purchase → READY → Deploy → DropPod semantics stay unchanged. This is a grid/placement architecture slice, **not** Wall gameplay.

## Operator PASS (final)

Operator confirmed the final PASS:

- 200 cm BuildGrid snap works
- landed Hub overlap rejects, READY preserved
- adjacent free Hub placement succeeds
- in-flight DropPod reservation blocks overlap
- MainBase occupancy blocks placement
- destroy Hub releases cells
- editor shutdown / close without attachment ensure
- per-cell filled placement preview works
- invalid/free coloring works
- building ghost works
- camera-facing status text works
- PlacementFootprintBounds editable in Blueprint
- BoxExtent / own RelativeScale affect footprint
- RelativeLocation offset works
- parent/root scale does not distort gameplay footprint
- pre-placed MainBase uses live visible footprint
- footprint tied to building root/Capsule by position + rotation
- rotated MainBase footprint follows actor rotation
- arbitrary yaw oriented footprint occupies corresponding BuildGrid cells
- forbidden cells visually match authored footprint
- spawned/orbital Hub yaw-0 path did not regress
- ground preview stays on terrain with obstacles
- cancel/shutdown regression PASS

## Final canon

### PlacementFootprintBounds
Live gameplay source on `AGP_BuildingBase`:

- location inherits parent
- rotation inherits parent
- parent/root scale ignored
- own BoxExtent / RelativeLocation / RelativeRotation / RelativeScale3D are design data

### Pre-placed occupancy
`ResolveOccupiedCellsFromBounds` → explicit deterministic `TArray<FIntPoint>` → OBB-vs-grid-cell SAT → `RegisterCells` → exact unregister by OccupantId.

`GridOriginCell` / `GridFootprintSize` are the occupied-set AABB for rotated buildings (legacy/debug). **Not** the occupancy SoT.

### Yaw-0 orbital placement
Existing rectangular reservation / `ConfigureGridPlacement` path retained. No orbital rotation UI in GP-S36G.

Do **not** revert to hidden CDO occupancy, world-zero footprint rotation, manual X/Y swap, parent-scale multiplication, or free-form pre-grid placement.

## Implemented grid facts

| Topic | Implementation |
| --- | --- |
| Class | `UGP_BuildGridSubsystem : UWorldSubsystem` (`Buildings/Grid/`) |
| Cell size | `200.0f` cm |
| Grid origin XY | World `(0,0)` — Z is never encoded in cell coordinates |
| WorldToCell | `Floor(World/CellSize + 0.5)` per axis |
| Occupancy | Server-only `FGuid → cells`. Subsystem is **not** replicated |
| Reservation | `FGuid` after deploy accept, bound to DropPod, promoted on payload spawn |
| PlacementFootprintBounds | Live source; position+rotation follow building; parent scale ignored |
| Pre-placed MainBase | Live visible oriented footprint when usable; 5×5 fallback if bounds unusable |
| Ferronite Deposit | Not on grid. Existing WorldStatic/WorldDynamic overlap remains |
| Orbital rotation | Yaw 0 rectangle. No rotation UI |
| ClearanceCells / Walls / FoW | **Deferred** |
| READY | Invalid grid placement does not consume READY |
| Hub +5 | Unchanged native `AGP_LogisticsHub` live/operational apply/remove |

## Oriented occupancy
Yaw ~0°/180°: snapped SizeCells rectangle. Any other yaw: OBB world AABB → candidate cells → 2D SAT. Occupied when overlap > `OccupancyOverlapEpsilonCm = 1.0`. Exact edge touch does not occupy.

## Tests
All Failures=0:

- `gp.Building.RunBuildGridContractTest`
- `gp.Building.RunMultiBuildingDataContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest`
- `gp.Resource.RunUnitCapLogisticsHubContractTest`
- `gp.Resource.RunOrbitalUnitDropContractTest`
- `gp.Movement.RunRTSMovementReconciliationContractTest`
- `gp.Match.RunWinLoseContractTest`
- `gp.Resource.RunS28RegressionSuite`
- `gp.Combat.RunAttackMoveContractTest`
- `gp.Combat.RunAutoAcquireContractTest`
- `gp.Resource.RunContainerLaunchContractTest`
- `gp.Resource.RunContainerLaunchHUDContractTest`

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development **PASS**.  
GP Win64 Shipping **PASS**.

## Stop condition
**NOT MERGED.** Human merge only. Do not start Wall gameplay / FoW placement / turret combat without explicit assignment.
