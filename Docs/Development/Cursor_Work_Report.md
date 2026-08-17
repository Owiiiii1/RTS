# Cursor Work Report

Status: **GP-S36G_FOOTPRINT_PARENT_SCALE_ISOLATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`PENDING_IMPLEMENTATION`

## Operator X-only multiplicative mismatch
After live-source reconciliation, center/offset and Y were acceptable, but forbidden **X** stayed several times smaller than the visible `PlacementFootprintBounds`. Enlarging the box kept the same X factor. The forbidden region stayed centered inside the visible box. That is a scale-space mismatch, not snap/source divergence.

## Factual traced scales (contract + registration log)
Native `AGP_MainBase` CDO: actor `(1,1,1)`, capsule relative `(1,1,1)`, bounds relative `(1,1,1)`, unscaled `(500,500)`.

Operator `BP_GP_MainBase` CDO:
- actor `(1.000, 1.000, 1.000)`
- capsule relative `(2.352, 2.352, 2.352)`
- bounds relative `(0.885, 0.377, 3.995)`
- unscaled `(500, 500)`
- authored half `(442.3, 188.6)`

Level instance `BP_GP_MainBase_C_2`:
- actor / capsule world `(2.352, 2.352, 2.352)`
- bounds relative still `(0.885, 0.377, 3.995)`
- before isolation, visible XY half ≈ authored × **2.352**
- runtime used authored only → X (and Y) visually larger than occupied cells by that factor

X looked worse because the authored bounds scale is already non-uniform (`0.885` vs `0.377`) and the operator widened X. The parent multiplier itself is uniform.

## Root cause
`PlacementFootprintBounds` is attached to Capsule/root, so viewport visualization inherited actor/root scale. `GetAuthoredPlacementHalfExtentCm()` uses only `UnscaledBoxExtent × RelativeScale3D`. Parent scale `2.352` inflated the visible box and not BuildGrid.

## Exact UE 5.8 API
Verified in `USceneComponent`:

- `SetAbsolute(bool bNewAbsoluteLocation, bool bNewAbsoluteRotation, bool bNewAbsoluteScale)`
- `CalcNewComponentToWorld_GeneralCase`: when `IsUsingAbsoluteScale()`, `CopyScale3D(NewRelativeTransform)` — world scale = `RelativeScale3D`, not parent × relative
- Location and rotation stay parent-relative

Call used:

`PlacementFootprintBounds->SetAbsolute(false, false, true);`

Applied in constructor defaults, `OnConstruction`, `PostLoad`, `PostInitializeComponents`, `BeginPlay`, and after CDO→live sync.

## Why runtime does NOT adopt actor scale
Footprint size is gameplay design data. Mesh/capsule presentation scale must not change occupied cells. Multiplying cells by actor/world scale would couple occupancy to art scale.

## Own component scale
Absolute scale does **not** lock size to 1. Operator-authored `RelativeScale3D` remains the visible and gameplay size. Example: parent `(3,1,1)` + own `(2,1,1)` + extent `500` → authored `1000×500` → `10×5`. Parent X=3 does not make visible X=6000.

## Center / offset
`GetLivePlacementFootprintCenterWorld` is still `Bounds->GetComponentLocation()`. Absolute scale replaces only scale; relative location, yaw, and live center are unchanged.

## NavigationObstacle
Unchanged. It may follow actor/root presentation scale; this task isolates only `PlacementFootprintBounds`.

## Tests
`gp.Building.RunBuildGridContractTest` Failures=0: parent `(3,1,1)` → `5×5` not `15×5`; own scale `(2,1,1)` → `10×5`; parent `(0.5,2,1)` still `5×5`; visual half == authored; live center unchanged; yaw/offset; huge `10×8`; actor scale 1; Hub path unchanged; level MainBase visual matches authored.

All listed regressions Failures=0.

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development / Shipping **not run**.

## Exact changed files
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

## Operator retest
1. MainBase BP: very wide X, reasonable Y. Save/compile.
2. Level instance visible box must match that BP size (no parent-scale stretch).
3. PIE → Hub Deploy: forbidden X/Y match that visible box modulo 200 cm. No stable ×2/×3 X gap. Center/offset unchanged.

**NOT MERGED.**  
**NOT FINALIZED.**
