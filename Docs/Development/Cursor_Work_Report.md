# Cursor Work Report

Status: **GP-S36G_LIVE_FOOTPRINT_SOURCE_RECONCILIATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
`47dcb071931ea442afb92febe41041a23996f944`

## Operator evidence (not rounding)
Operator enlarged MainBase `PlacementFootprintBounds` massively — several times the visible building — and the forbidden construction area appeared in a completely different place. That is a source/transform divergence, not 200 cm snap quantization.

## Prior CDO-vs-visible-instance design mistake
`ResolveActorFootprint` returned the Blueprint **class CDO** for `IsNetStartupActor()`. The editor/PIE drew the **live level-instance** box. Those could differ. A designer must never see one footprint while runtime registers another.

## Stale inherited snapshot root cause
Level actors serialize inherited BoxExtent / Scale / Offset. Example previously traced: `BP_GP_MainBase_C_2` kept 4×2 + offset 133.5 after the BP class changed. That is a synchronization problem. Solving it by hiding occupancy on the CDO created the visible mismatch.

## Final authoring policy (GP-S36G)
`PlacementFootprintBounds` is authored on the **Blueprint class**, not per-level-instance.

Reliable UE “explicit instance override vs stale inherit” distinction is not used as the primary design for this native inherited component.

On Construction / PostLoad / PostInitializeComponents / BeginPlay, **net-startup** buildings copy from class CDO onto the live component:

- BoxExtent
- RelativeLocation
- RelativeRotation
- RelativeScale3D

Runtime-spawned / deferred actors are not synced, so DropPod and contract instance authoring stay intact.

## Synchronization lifecycle
- `OnConstruction` — editor reconstruction / BP compile
- `PostLoad` — level load (in-memory; `.umap` is not written by this task)
- `PostInitializeComponents` / `BeginPlay` — before BuildGrid registration

No Tick. No programmatic map edit.

## Live component as single gameplay source
After sync, `ResolveActorFootprint` always uses the **live** component when usable. The hidden `IsNetStartupActor() → FromClass` path and the native-default CDO bypass are **removed**. Class CDO is design data for synchronization only.

## Live component world center rule
Pre-placed / unconfigured registration snaps `Bounds->GetComponentLocation()` XY — the visible box center. Size remains `UnscaledBoxExtent × RelativeScale3D` (no actor/world scale). Cells = ceil(total / 200). Preview / DropPod still use shared offset helpers (yaw 0 in GP-S36G).

## Tests
`gp.Building.RunBuildGridContractTest`: stale live vs CDO; sync copies design; resolve is FromInstance; CDO change after sync does not bypass live; 10×8 occupancy around live box; +600 offset; yaw 90 live center coincides; Hub path unchanged.

All listed regressions Failures=0.

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

## Operator retest
1. MainBase BP: huge, strongly offset `PlacementFootprintBounds`. Save/compile.
2. Open level: pre-placed inherited box must show that BP design.
3. PIE → Hub Deploy: red/forbidden area is the same region as that visible box (quantized to 200 cm).

**NOT MERGED.**  
**NOT FINALIZED.**
