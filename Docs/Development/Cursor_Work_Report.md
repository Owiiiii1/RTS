# Cursor Work Report

Status: **GP-S36G_ATTACHMENT_LIFECYCLE_FIX_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
*(filled in SHA-record commit)*

## Operator ensure
PIE handled ensure:

`Ensure condition failed: !bRegistered`  
`SceneComponent.cpp` line 2246

`SetupAttachment should only be used to initialize AttachParent and AttachSocketName for a future AttachToComponent. Once a component is registered you must use AttachToComponent.`

Failing component: `BP_GP_MainBase_C_0.PlacementFootprintBounds`  
Failing call: `AGP_BuildingBase::AttachPlacementFootprintBoundsToRoot()` → `SetupAttachment(Root)` from `PostInitializeComponents()`.

## Root cause
`PlacementFootprintBounds` is a constructor default subobject. Attachment was deferred until derived classes set Capsule/root. `PostInitializeComponents()` runs **after** component registration, so `SetupAttachment()` is lifecycle-invalid.

`NavigationObstacle` used the same deferred `SetupAttachment` pattern. It usually skipped the ensure because MainBase/Hub constructors already attached it (parent == root). The latent bug remained.

## Old invalid lifecycle
```
CreateDefaultSubobject (ctor)
→ derived SetRootComponent
→ RegisterAllComponents
→ PostInitializeComponents
→ SetupAttachment(Root)   // ensure if already registered
```

## New attachment rule
Shared private helper `AttachDeferredComponentToRoot`:

- if component parent is already Root → do nothing
- else if component is registered → `AttachToComponent(Root, KeepRelativeTransform)`
- else → `SetupAttachment(Root)`

Applied to both `PlacementFootprintBounds` and `NavigationObstacle`. No detach/re-register. No component recreation.

## PlacementFootprintBounds preservation
Registered attach uses `FAttachmentTransformRules::KeepRelativeTransform`. RelativeLocation, RelativeRotation, and BoxExtent (including Blueprint overrides) are not reset in `PostInitializeComponents`.

## NavigationObstacle
Same helper only. QueryOnly, NavArea_Null, dynamic obstacle, BP-authored transform/extent unchanged. No gameplay change.

## Contract coverage
`gp.Building.RunBuildGridContractTest`:

- Deferred MainBase: constructor `SetupAttachment` already parents NavigationObstacle (pre-registration path)
- `FinishSpawning` / `SpawnActor`: PlacementFootprintBounds registered and parented to final root (registered `AttachToComponent` path)
- Authored/test RelativeLocation and non-default BoxExtent survive attachment
- Repeated `AttachDeferredSceneComponentsToRoot()` is idempotent
- Live contract MainBase spawn also asserts both boxes parented to root

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

BuildGrid conversion, precedence, offset, snap, ground-Z, occupancy, reservation, READY, Purchase, Deploy, DropPod, preview fills, Hub +5 were not modified.

## Operator retest
1. Launch Editor / PIE.
2. MainBase initializes with no `!bRegistered` / `SetupAttachment should only be used...` ensure.
3. Open authored Logistics Hub BP — `PlacementFootprintBounds` still present and editable.
4. Set Box Extent X/Y = 200, save BP.
5. PIE → Hub Deploy preview should become 2×2.

**NOT MERGED.**  
**NOT FINALIZED.**
