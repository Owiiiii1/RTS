# Cursor Work Report

Status: **GP-S36G_AUTHORABLE_FOOTPRINT_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`0ceab9ec449e0af6024d05a42ed10938c9c3855c`

## Operator feedback items
1. Filled cells still showed per-cell contour lines and an outer AABB border. Wanted fills only; separation from inset/gap and center vs edge opacity.
2. Ground-Z stayed on terrain for MainBase/buildings, but still climbed onto map boundary walls, Ferronite/resource geometry, and other WorldStatic/WorldDynamic actors.
3. Canonical 4×4 DataAsset footprint does not match authored visible building size. Operator wants a Blueprint-resizable boundary zone on the building class.

Server/grid mechanics were not changed (CellSize 200, snap math, occupancy, reservation, READY, Purchase, Deploy, DropPod timing, Hub +5, Walls, FoW, match flow). Server remains authority.

## No-border preview
`AGP_BuildingPlacementGhost` now draws **filled cells only** via `ULineBatchComponent::DrawSolidBox`.

Removed:
- all per-cell contour `DrawLine` outlines
- outer footprint AABB border

Kept:
- per-cell fill colors (invalid red, free green/non-red)
- ~12 cm physical gap (6 cm inset per tile)
- center/inner ring denser and taller; outer ring lighter/more transparent
- building ghost when valid (payload CDO meshes, not scaled to the grid)
- camera-facing status text over the footprint center

`GetPreviewGridLineCount()` / `GetPreviewLineWorldCount()` remain 0 while preview is active.

## PlacementFootprintBounds
Added on `AGP_BuildingBase`:

`UBoxComponent* PlacementFootprintBounds`  
Category: `GP|BuildGrid`

- visible in editor, hidden in game
- no collision, no overlap events, does not affect navigation
- Blueprint children can edit RelativeLocation and BoxExtent
- **not** collision, nav obstacle, selection, or combat bounds
- default XY extent `0,0` means unauthored (Z = 20 for the component)

Operator path: open Logistics Hub BP → `PlacementFootprintBounds` → resize/move XY → save BP → PIE. No DataAsset edit required.

## Bounds → cells
CellSize remains 200 cm. Axis-aligned only; no rotation in this slice.

- WidthCm = 2 * BoxExtent.X
- HeightCm = 2 * BoxExtent.Y
- CellsX = max(1, ceil(WidthCm / 200))
- CellsY = max(1, ceil(HeightCm / 200))

Examples: 200×200 → 1×1; 400×400 → 2×2; 550×380 → 3×2; 800×800 → 4×4; 300×300 → 2×2.

## Footprint precedence
Shared resolver: `UGP_BuildGridSubsystem` + `FGP_ResolvedBuildingFootprint` `{ SizeCells, LocalCenterOffsetCm, bFromAuthoredBounds }`.

1. Payload CDO / instance `PlacementFootprintBounds` when XY extent ≥ 1 cm
2. `UGP_BuildingDefinition.FootprintCells` when both axes > 0 (compatibility / default fallback)

`FootprintCells` is **not** deleted. Native actors, tests, unauthored Blueprints, and definitions without bounds still use it.

If a BuildingDefinition is present but `FootprintCells` is invalid, class fallback is not applied (keeps InvalidFootprint deploy rejection).

Pre-placed buildings: authored bounds if usable; else existing class fallback (MainBase 5×5, Logistics Hub 4×4, generic 1×1). Map assets were not edited in this task.

## Local offset + snap
`PlacementFootprintBounds` RelativeLocation XY is preserved as `LocalCenterOffsetCm`.

Cursor/grid still places the occupied cells (footprint center). Actor pivot is:

`ActorLocation = FootprintCenter − (Offset.X, Offset.Y, 0)`

Client confirm still sends footprint-center transform. Server reconstructs the same actor location. Ghost uses actor pivot; grid fill uses occupied cells. Ghost is not scaled to the grid.

## Client/server shared resolver
One resolver is used by PlayerController, DropAuthority, DropPod landing (actor location), and BuildingBase pre-placed registration. Math is not duplicated.

On accepted deploy, origin cell + footprint size come from that resolve and are passed through DropPod `ConfigureGridPlacement`. Spawned building registers the same cells as preview/server validation.

## Semantic ground-Z
`UGP_BuildGridSubsystem::ResolveDeployGroundZ` (wrapped by `GPBuildingDropAuthority::ResolvePreviewGroundZ`):

1. NavMesh `ProjectPointToNavigation` at snapped XY (accepted only if Dist2D ≤ CellSize)
2. Else multi-hit WorldStatic/WorldDynamic (then WorldStatic / Visibility): keep hits with ImpactNormal.Z ≥ 0.65; pick the **lowest** Z within 4000 cm of the highest such hit
3. Fallback Hint.Z

Not first-hit Visibility/WorldStatic. No growing class-specific ignore lists. Preview height only — WorldBlocked/grid validation still rejects environment overlap. Server deploy uses the same ground Z after snap.

## Tests (all Failures=0)
- `gp.Building.RunBuildGridContractTest` — A no-border preview; B bounds→cells; C authored overrides DA; D no bounds → DA; E local XY offset; F client/server identical footprint + actor transform; G spawned registers same cells; H preplaced MainBase 5×5 fallback; I/J ground resolver stays terrain under elevated static / resource / wall-like geometry; K environment overlap still invalid; L READY exact-once unchanged
- `gp.Building.RunMultiBuildingDataContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest`
- `gp.Resource.RunUnitCapLogisticsHubContractTest`
- `gp.Resource.RunOrbitalUnitDropContractTest`
- `gp.Movement.RunRTSMovementReconciliationContractTest`
- `gp.Match.RunWinLoseContractTest`
- `gp.Resource.RunS28RegressionSuite`
- `gp.Combat.RunAttackMoveContractTest`

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development / Shipping **not run**.

## Exact changed files
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h`
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropAuthority.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingPlacementGhost.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingPlacementGhost.cpp`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

Not committed (operator-local): `GP/Config/DefaultEngine.ini`, `GP/Config/DefaultGame.ini`, `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`, `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`, `GP/Content/GrimProtocol/Blueprint/`, `GP/Content/GrimProtocol/Materials/`, `GP/Content/Basic_VFX/`, `GP/Content/Mixed_Magic_VFX_Pack/`, `GP/Content/RocketThrusterExhaustFX/`, `Tools/`.

## Next operator visual retest
1. Open authored Logistics Hub Blueprint.
2. Find `PlacementFootprintBounds`, resize/move XY, save BP.
3. PIE → Purchase Hub → Deploy.
4. Preview tiles match authored box; no cell/outer borders; center denser, edges softer.
5. Building ghost sits on the real pivot relative to those tiles.
6. Spawned Hub occupies the same cells.
7. Hover MainBase / Hub / Ferronite / boundary wall / elevated static: preview stays on terrain. Placement over blockers remains invalid.

**NOT MERGED.**  
**NOT FINALIZED.**
