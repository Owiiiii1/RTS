# Cursor Work Report

Status: **GP-S36G_FOOTPRINT_AXIS_ALIGNMENT_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`853e3337bd74d9fce81c1a16302d41a5d5d510a9`

## Operator X/Y swap
After parent-scale isolation, size was almost correct, but a strongly rectangular visible `PlacementFootprintBounds` looked swapped versus the forbidden BuildGrid region.

## Factual rotation-inheritance cause
Previous policy was `SetAbsolute(false, false, true)`: location and **rotation** inherited the parent. The visible box rotated with MainBase (~91.7° on the level instance). BuildGrid size still maps local X→world X and local Y→world Y with no footprint rotation. At ~90° the visible local X axis lay along world Y while occupancy stayed world-axis-aligned.

Level `BP_GP_MainBase_C_2` after this patch: `actorYaw=91.7`, `boundsWorldYaw=0.0`, `authoredHalf=visualHalf=(856.1,486.8)`, registered `9×5`, `absRot=1`.

## Final axis-aligned semantics
- SIZE AXES = world X/Y
- BOX ROTATION = world zero
- CENTER OFFSET = actor-local XY rotated by actor yaw
- actor/root scale ignored for footprint dimensions
- Rotated footprints are **not** supported in GP-S36G (deferred)

## Exact SetAbsolute policy
Verified UE 5.8 `USceneComponent::SetAbsolute(bAbsLoc, bAbsRot, bAbsScale)`:

`PlacementFootprintBounds->SetAbsolute(false, true, true);`

Location stays parent-relative (`Rel * Parent` still applies actor yaw to the authored offset). Rotation and scale are absolute.

## RelativeRotation
Forced to `ZeroRotator` on construction / PostLoad / PostInitialize / BeginPlay / CDO→live sync. Authored component rotation is not a gameplay or visualization source. Details-panel hide was not added (not straightforward on the inherited native transform).

## Center-offset behavior
`GetLivePlacementFootprintCenterWorld` remains `GetComponentLocation()`. After absolute rotation, inherited location still transforms by actor yaw. Proven: local +400 X, yaw 0 → +400 world X; yaw 90 → +400 world Y. Visual center equals that intended center. Box axes stay world-zero while the center moves.

## Yaw 0/90 rectangular tests
Effective 2000×800 (half 1000×400): yaw 0 / 90 / 180 / 270 all keep visual world half `1000×400` and grid `10×4` (not `4×10`). World X/Y Hub edge reject/accept on a yaw-90 `10×4`. Parent scale `2.352` does not change size. Own RelativeScale still authors size.

## Parent scale
Unchanged: absolute scale keeps own `RelativeScale3D`. Actor/root `2.352` does not inflate cells.

## Tests
`gp.Building.RunBuildGridContractTest` Failures=0.

All listed regressions Failures=0:
`RunMultiBuildingDataContractTest`, `RunOrbitalBuildingDropContractTest`, `RunUnitCapLogisticsHubContractTest`, `RunOrbitalUnitDropContractTest`, `RunRTSMovementReconciliationContractTest`, `RunWinLoseContractTest`, `RunS28RegressionSuite`, `RunAttackMoveContractTest`.

## Builds
GPEditor Win64 Development + UHT **PASS**.  
GP Win64 Development / Shipping **not run**.

## Exact changed files
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

**NOT MERGED.**  
**NOT FINALIZED.**
