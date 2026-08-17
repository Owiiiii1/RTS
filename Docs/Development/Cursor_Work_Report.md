# Cursor Work Report

Status: **GP-S36G_FOOTPRINT_VIEWPORT_AUTHORING_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`edae361cfcac625519c0e606111958ad8bfb561d`

## Screenshot-observed zero XY extent
Operator screenshot: `PlacementFootprintBounds` is selectable, `Editable when Inherited` is enabled, but Box Extent is **X=0, Y=0, Z=20**. Viewport Scale gizmo draws a dotted scale vector and **no visible footprint box**.

This is expected mathematically: zero XY extent × any relative scale remains zero, so the box has no volume to draw.

## Old resolver ignored RelativeScale3D
`UGP_BuildGridSubsystem::TryResolveFromPlacementBounds` used `Bounds->GetUnscaledBoxExtent()` only. Viewport Scale therefore could not change BuildGrid cells even if the box were visible.

## Effective extent formula
Component-local authored half-extent (not actor/world scale):

```
EffectiveHalfExtentX = abs(UnscaledBoxExtent.X * RelativeScale3D.X)
EffectiveHalfExtentY = abs(UnscaledBoxExtent.Y * RelativeScale3D.Y)
WidthCm  = 2 * EffectiveHalfExtentX
HeightCm = 2 * EffectiveHalfExtentY
Cells    = ceil(Size / 200), minimum 1×1
```

API: `UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm`.

Explicitly **not** `UBoxComponent::GetScaledBoxExtent()` — that multiplies by `GetComponentTransform().GetScale3D()` and would include actor / parent / map instance scale.

RelativeRotation is ignored (GP-S36G axis-aligned). RelativeLocation XY is the footprint center offset and is **not** multiplied by component scale.

Scale is preserved (not forced back to 1). Resolver uses BoxExtent × RelativeScale3D together.

## Native default extents (visible authoring volumes)
Zero XY is no longer the normal unauthored state.

| Class | Cells | Total cm | BoxExtent XY | Z viz |
| --- | --- | --- | --- | --- |
| Generic `AGP_BuildingBase` | 1×1 | 200×200 | 100,100 | 20 |
| `AGP_LogisticsHub` | 4×4 | 800×800 | 400,400 | 20 |
| `AGP_MainBase` | 5×5 | 1000×1000 | 500,500 | 20 |

Derived constructors call `SetBoxExtent` on the existing subobject after root setup. Component is not recreated. `PostInitializeComponents` / `BeginPlay` do not reset Blueprint overrides. Native values are archetype defaults only; Blueprint-authored BoxExtent / RelativeScale3D / RelativeLocation win.

## BP Scale gizmo behavior
Operator may Scale the component in the Blueprint viewport.

Hub baseline examples (BoxExtent 400,400):

- Scale 1,1 → 800×800 → 4×4
- Scale 0.5,0.5 → 400×400 → 2×2
- Scale 0.75,0.5 → 600×400 → 3×2
- Scale 1.25,0.75 → 1000×600 → 5×3

Building ghost mesh scale is independent of the footprint component. Only occupied grid area changes.

## Offset
`RelativeLocation` XY still defines footprint center vs actor pivot. Not scaled. Rotation unsupported / ignored.

## DA fallback
`UGP_BuildingDefinition.FootprintCells` kept. Usable `PlacementFootprintBounds` (effective XY half-extent ≥ 1 cm) resolve first. Classes without usable bounds still use DA. Current effective defaults match old fallback (MainBase 5×5, Hub 4×4, generic 1×1) so unedited gameplay footprints do not regress.

## Tests
`gp.Building.RunBuildGridContractTest` extended:

- A native visible extents: generic 100,100 → 1×1; Hub 400,400 → 4×4; MainBase 500,500 → 5×5
- B RelativeScale cells (Hub 400,400): 1,1 → 4×4; 0.5,0.5 → 2×2; 0.75,0.5 → 3×2; 1.25,0.75 → 5×3
- C RelativeLocation offset independent of scale
- D CDO/seam resolver reads scale
- E preview and server resolve identical footprint
- F spawned building registers identical cells (including scaled 2×2)
- G DA fallback when bounds unusable
- H `bEditableWhenInherited` remains true
- Actor world scale does not inflate footprint
- Ghost mesh scale independent of footprint scale

All listed regressions Failures=0:

- `gp.Building.RunBuildGridContractTest`
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
- `GP/Source/GPRuntime/Private/Buildings/GPLogisticsHub.cpp`
- `GP/Source/GPRuntime/Private/Buildings/GPMainBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h`
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

Occupancy semantics, reservation, READY, Purchase, DropPod timing, Hub +5, CellSize, snap, and ground-Z were not changed.

## Operator retest
1. Restart/recompile Editor.
2. Open `BP_GP_LogisticsHub`.
3. `PlacementFootprintBounds` should be a visible **800×800** box (not a zero-size dotted vector).
4. Select it; Scale gizmo X/Y; Translate gizmo. No need to type Box Extent first.
5. PIE: Purchase Hub → Deploy. Preview tiles follow effective bounds (e.g. scale 0.5,0.5 → 2×2). Building ghost mesh stays at normal building scale.

If a Blueprint already serialized Box Extent 0,0 as an override, that authored override still wins until the operator resets/edits it.

**NOT MERGED.**  
**NOT FINALIZED.**
