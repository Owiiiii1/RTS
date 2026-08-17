# Cursor Work Report

Status: **GP-S36G_FOOTPRINT_BP_AUTHORING_FIX_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`f393fda3a9fe9a2e03df0490b23fafc75fb32850`

## Operator symptom
`PlacementFootprintBounds` appears in the Logistics Hub Blueprint Components tree but is effectively read-only as an inherited native component. Operator cannot edit Box Extent / Relative Transform.

## Factual Unreal cause
Blueprint Details treats a native inherited component as editable only when the **archetype** reports `IsEditableWhenInherited()`.

UE 5.8 (`ActorComponent.h` / `ActorComponent.cpp`):

- Public bit `UActorComponent::bEditableWhenInherited` (no setter API; Engine itself assigns the field, e.g. PackedLevelActor).
- Constructor default is `true`, but Blueprint inspector gates on `GetArchetype()->IsEditableWhenInherited()`.
- For native instances, `IsEditableWhenInherited()` also requires the owning actor UPROPERTY to carry `CPF_Edit` (`FComponentEditorUtils::GetPropertyForEditableNativeComponent`).
- `CanEditChange` on a BP child refuses edits when the parent component archetype has `bEditableWhenInherited == false`.

The native subobject was never explicitly marked authorable, so inherited Hub BP templates could not edit BoxExtent / RelativeTransform.

## Exact editable-when-inherited fix
In `ConfigurePlacementFootprintBoundsDefaults()` / `ConfigureNavigationObstacleDefaults()`:

```
PlacementFootprintBounds->bEditableWhenInherited = true;
NavigationObstacle->bEditableWhenInherited = true;
```

Verified against installed UE 5.8 headers: property is public `uint8 bEditableWhenInherited:1`; no `SetEditableWhenInherited` exists. Same assignment style as Engine.

## UPROPERTY decision
Kept:

`VisibleAnywhere, BlueprintReadOnly, Category=..., meta=(AllowPrivateAccess="true")`

This is the standard native component pattern (`ACharacter::CapsuleComponent`):

- `VisibleAnywhere` → `CPF_Edit` so the native component is discoverable as authorable, **without** making the pointer replaceable.
- `BlueprintReadOnly` / no `EditAnywhere` on the `TObjectPtr` → Blueprint cannot swap the subobject.
- Component-owned properties (`BoxExtent`, RelativeTransform) remain `EditAnywhere` on `UBoxComponent` itself.

Do **not** use `EditAnywhere` on the component pointer.

## NavigationObstacle
Same `bEditableWhenInherited = true` flag. Already documented as BP-authorable (RelativeLocation / Rotation / BoxExtent). Collision/nav/gameplay defaults unchanged.

## CDO persistence test
`gp.Building.RunBuildGridContractTest`:

- CDO `bEditableWhenInherited` + `IsEditableWhenInherited()` for PlacementFootprintBounds and NavigationObstacle
- Mutate stub CDO BoxExtent 200/200 + RelativeLocation (100, -30), resolve, restore CDO
- Resolver: 2×2 cells, LocalCenterOffsetCm preserved, authored bounds override DA 4×4
- Spawned instance also reports `IsEditableWhenInherited()`

Full Blueprint editor click-edit remains operator validation (contract runs `-game`).

## Tests (all Failures=0)
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
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

BuildGrid conversion, fallback, offset, snap, occupancy, reservation, ground-Z, READY/Purchase/Deploy, DropPod, preview, Hub +5 were not modified.

## Operator retest
1. Restart/recompile Editor.
2. Open authored Logistics Hub Blueprint.
3. Select `PlacementFootprintBounds`.
4. Box Extent fields editable.
5. Set X=200, Y=200; move Relative Location X e.g. +100; Save BP.
6. After that: PIE → Purchase Hub → Deploy; preview should be 2×2 with offset.

**NOT MERGED.**  
**NOT FINALIZED.**
