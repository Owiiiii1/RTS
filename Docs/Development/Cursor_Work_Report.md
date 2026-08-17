# Cursor Work Report

Status: **GP-S36G_PREPLACED_OCCUPANCY_AUTHORING_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`6c61b972993d6b85274ca93b58e8752e136267e5`

## Operator question and canonical answer
Does `PlacementFootprintBounds` apply only while placing a building, or also to occupied/blocking area after construction?

**Same footprint is both.** One resolved size/offset is preview, server validation, reservation, spawned occupancy, and pre-placed occupancy. There is no separate placement size vs blocking size.

## Actual root cause
`TryRegisterWithBuildGrid` already called `ResolveActorFootprint` and registered `SizeCells`. The mismatch was the **source** of those bounds.

Traced on the operator map during contract PIE:

- Level actor `BP_GP_MainBase_C_2` registered **4×2** with offset **(133.5, 0)** — not native 5×5.
- That instance had serialized inherited BoxExtent / Scale / RelativeLocation from an earlier Blueprint state.
- Unreal treats those serialized values as instance data. Later Blueprint viewport edits update the **class CDO**, not the stale level-instance snapshot.
- Old resolver preferred any usable **instance** box. Enlarging the MainBase Blueprint therefore changed the visible CDO box and did **not** grow registered occupancy.

Hub authoring appeared to work because orbital Hub uses `ResolveBuildingFootprint(payload CDO)` for preview / reservation / DropPod `ConfigureGridPlacement`. Pre-placed MainBase used the instance path.

## Instance / CDO behavior found
- Blueprint CDO = building design data (what the operator edits in the MainBase BP).
- Level instances of `BP_GP_MainBase` keep a serialized copy of inherited component transforms. Those copies masquerade as explicit instance overrides after the BP changes.
- Native default heuristic alone is insufficient: the live instance was **not** 500×500 / offset 0; it was a previous authored snapshot (4×2 + X=133.5).
- Runtime-spawned / deferred-spawn actors (tests, DropPod payload) are not net-startup; their instance box is live authoring.

## Exact source policy after fix
`PlacementFootprintBounds` remains the single occupied-ground footprint.

`ResolveActorFootprint`:

1. **Net-startup (pre-placed) building:** use **class CDO** bounds (Blueprint design). Do not trust the level-instance snapshot.
2. **Runtime-spawned instance** that still matches the **native** default (MainBase 500×500 offset 0, Hub 400×400, generic 100×100) while the class CDO does not: use class CDO (stale native snapshot).
3. **Otherwise runtime instance** wins (explicit deferred-spawn / test authoring, including offset).
4. If bounds are unusable: DA `FootprintCells`, then class fallback.

DropPod `ConfigureGridPlacement` still pins spawned Hub reservation/occupancy. Preview and server still share `ResolveBuildingFootprint`.

Offset: CDO RelativeLocation XY is used for pre-placed registration; runtime instance offset is preserved when instance authoring wins.

## MainBase pre-placed registration path
`BeginPlay` → `TryRegisterWithBuildGrid` → `ResolveActorFootprint` (class CDO for net-startup) → snap origin from actor location + CDO offset → `RegisterFootprint`. Non-Shipping log + `GetBuildGridOccupancyDebugString()` report actor name, resolved size/offset, origin, registered size.

## Tests
`gp.Building.RunBuildGridContractTest` extended:

- A pre-placed/runtime MainBase authored 700×600 → 7×6, not fallback 5×5
- B MainBase scale 1.4 / 1.2 on 500×500 → 7×6
- C RelativeLocation (200, -100) shifts origin
- D all 42 cells occupied
- E adjacent outside cells free
- F Hub 4×4 overlapping one edge cell → `CellOccupied` (deploy maps to `GridOccupied`)
- G Hub beside authored footprint valid
- H spawned Hub occupancy/reservation unchanged (contract isolates `BuildingPayloadClass` to native Hub so operator BP scale cannot desync 4×4)
- Stale native instance + authored CDO → class 7×6

All listed regressions Failures=0.

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development / Shipping **not run**.

## Exact changed files
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildGridContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

Map / operator BP / config were not edited.

## Operator retest
1. In MainBase Blueprint enlarge `PlacementFootprintBounds` noticeably.
2. Save BP.
3. PIE.
4. Purchase Hub → Deploy.
5. Move Hub preview near MainBase edge.

Expected: red occupied cells follow the Blueprint box (class CDO), including shrink. No `.umap` reset required.

**NOT MERGED.**  
**NOT FINALIZED.**
