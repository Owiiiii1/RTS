# Cursor Work Report

Status: **GP-S36G_VISUAL_FEEDBACK_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.** This is **not** finalization.

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Prior remote feature head (before this visual patch)
`5c05cd68034eb44ef9d38ce5708f73449693e854`

## Feature head SHA
`702676743c4f3ba29a9f94f680bbee6861fd77a9`

## Prior logic operator PASS (do not change mechanics)

Operator manually confirmed:

- A. Logistics Hub ghost snaps discretely on the 200 cm grid — **PASS**
- B. First Hub deploys on free cells — **PASS**
- C. Second Hub cannot be deployed overlapping an already-landed Hub — **PASS**
- D. Rejected overlap does not consume READY — **PASS**
- E. Second Hub can be deployed on adjacent free cells — **PASS**
- F. In-flight reservation: while first Hub DropPod is still descending and payload has not spawned, second Hub cannot be deployed onto the same footprint — **PASS**
- G. MainBase occupancy: Hub cannot be deployed through/over MainBase footprint — **PASS**
- H. Occupancy release: after placed Hub is destroyed/removed, previous cells become available and another Hub can be deployed there — **PASS**

Occupancy rules, reservation lifecycle, WorldToCell, footprint anchor math, READY, Purchase, DropPod timing, Hub +5, MainBase fallback, NavigationObstacle, Walls, and FoW were not modified.

## Operator UX issue
Placement validity was not visually obvious. `AGP_BuildingPlacementGhost` used Engine BasicShapes Cube, scaled to footprint, created an MID, and called `SetVectorParameterValue("Color")`. Engine Cube's material is **not assumed** to expose `Color`. Feedback must remain readable even if that tint does nothing.

## Exact visual implementation
Asset-independent runtime path (no authored materials, no fabricated `.uasset`, no hardcoded `/Game/...` material paths):

- Unscaled `USceneComponent` root on the ghost.
- Optional translucent Engine Cube fill still scaled to footprint.
- Primary: local `ULineBatchComponent` lines from `UpdateGridPreview(Grid, OriginCell, FootprintSize, GroundZ, bValid, RejectReason)` using `UGP_BuildGridSubsystem::GetFootprintWorldAABB` + `GetCellSize`. Outer border + internal cell divisions. Direct `FColor` green/red. Lines at local Z 24 cm to avoid z-fighting. Current footprint only — no world-wide overlay.
- Primary: `UTextRenderComponent` above the ghost with the same green/red and compact status text.
- 4×4 Logistics Hub: outer 800×800 cm, 16 cells, 10 lines (4 border + 3+3 internals).

## Material Color parameter
Optional MID `"Color"` tint is **kept** as an extra on the cube fill. It is **not** the primary acceptance path. This patch does not claim Engine Cube actually exposes `Color`. Validity is accepted from lines + text even if the cube color never changes.

## Validity / reject text mapping
Uses existing `EGP_BuildingDropRejectReason` (no new overlapping enum).

- valid → `VALID`
- `GridOccupied` → `BLOCKED: OCCUPIED`
- `OutOfDeployRadius` → `BLOCKED: OUT OF RANGE`
- `NotNavigable` → `BLOCKED: NOT NAVIGABLE`
- `PlacementOverlap` → `BLOCKED: WORLD`
- other → `BLOCKED`

Local preview struct: `GPBuildingDropAuthority::FPlacementPreview` (`bValid`, `RejectReason`, snapped Origin/Size/ground). `ValidateBuildingPlacement` now writes snap outputs after a successful snap even when a later check rejects, so the ghost follows the snapped footprint (not raw cursor) while invalid. Gameplay accept/reject is unchanged.

Out-of-range uses `UGP_OrbitalDeliverySettings::BuildingMaxDeployRadiusFromMainBaseCm` (same as server).

## Local vs server authority
Client preview predicts for feedback only. Occupancy maps stay server-only.

On LMB confirm: if local preview is invalid, **no deploy RPC** is sent; placement mode remains active; READY unchanged. If local preview is valid, client still sends a snapped transform intent; server re-snaps and re-validates. Race: local VALID + server reject → server wins. Contracts continue to call `AuthorityDeployBuilding` directly, so server rejection tests remain intact.

## In-flight reservation preview
Full BuildGrid replication was **not** added.

Tiny existing seam: `AGP_DropPod` replicates `BuildingGridOriginCell` + `BuildingGridFootprintSize` for building payloads. Local preview also scans replicated `AGP_BuildingBase` OriginCell/FootprintSize (landed Hub / MainBase). That is enough for immediate RED/`BLOCKED: OCCUPIED` on landed structures and, when the pod facts have replicated, on an in-flight Hub DropPod.

If those replicated facts have not arrived yet, in-flight overlap can still be a **server-only** reject. Server reservation remains authoritative. This is not a grid-replication architecture change.

## Presentation lifecycle
Ghost + lines + text appear only while building Deploy mode is active, update with cursor snap, and are destroyed/cleared on successful confirm, Esc cancel (`OnSelectionCanceled`), RMB cancel, leaving placement mode, and PlayerController EndPlay / PIE teardown. `ClearGridPreview` flushes the line batch.

## Tests (all Failures=0)
- `gp.Building.RunBuildGridContractTest` (extended: 4×4 outer 800, cell/line counts, label mapping, local occupied/out-of-range, cancel clears preview state — no pixel tests)
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

## Next operator visual test
1. Purchase Hub → Deploy.
2. Free area: footprint cells visible, GREEN, `VALID`.
3. Move over MainBase / existing Hub: RED, `BLOCKED: OCCUPIED`.
4. Move outside deploy radius: RED, `BLOCKED: OUT OF RANGE`.
5. Move back to valid: GREEN again.
6. Cancel Esc/RMB: all preview visuals disappear.
7. Close Editor: no crash.

## Exact files changed during this visual patch
- `GP/Source/GPRuntime/Public/Buildings/Grid/GPBuildGridSubsystem.h`
- `GP/Source/GPRuntime/Private/Buildings/Grid/GPBuildGridSubsystem.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropAuthority.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingPlacementGhost.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingPlacementGhost.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPDropPod.h`
- `GP/Source/GPRuntime/Private/Orbital/GPDropPod.cpp`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Claude_Tasks/GP-S36G_BuildGrid_MVP.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

Not committed (operator-local): `DefaultEngine.ini`, `DefaultGame.ini`, `L_PrototypeArena.umap`, `BP_ResourceNode_AuthoredExample.uasset`, Blueprint/Materials/VFX packs, `Tools/`, AutoAcquire CRLF noise.

## Explicit
**NOT MERGED.**
