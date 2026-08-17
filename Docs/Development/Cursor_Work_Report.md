# Cursor Work Report

Status: **GP-S36G_PREVIEW_POLISH_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Feature head SHA
*(filled in SHA-record commit)*

## Operator feedback items
1. Footprint preview felt much larger than the visible building.
2. Partial overlap painted the entire footprint red.
3. Preview was contour-only; wanted filled cells, denser center, building ghost when valid.
4. Status text did not face the camera.
5. Hovering a placed building lifted preview onto its top surface.

Server/grid mechanics previously PASS and were not changed (CellSize, occupancy, reservation, READY, Purchase, Deploy, DropPod, Hub +5, match flow, server authority).

## Footprint data: A or B
**B — footprint data is visually correct-but-larger.**

Canonical SoT remains `UGP_BuildingDefinition.FootprintCells`. Native Logistics Hub catalog value is **4×4** (800×800 cm), matching TDD/GDD BuildGrid spec. Native Hub collision/presentation is much smaller (capsule radius 80 cm, nav obstacle ~140 cm). The occupied area was **not** shrunk to match the mesh. Preview now shows occupied cells as filled area **plus** a payload-shaped building ghost at real spawned scale.

## Exact preview architecture
Asset-independent (no `.uasset`, no `/Game/...` materials):

- Per-cell **filled quads** via `ULineBatchComponent::DrawSolidBox` in world space (Flush each update, lifetime 0). Inner cells taller/denser; edge cells thinner/lighter.
- Per-cell outlines + outer AABB border in matching green/red.
- `UTextRenderComponent` status label, billboarded toward the local player camera each preview update.
- Building ghost: copy `UStaticMeshComponent`s from payload CDO when present (authored BP mesh). If the CDO has no mesh (native Hub), Engine Cylinder scaled to the capsule is used. Shown only when placement is valid; hidden when invalid.
- Legacy Engine Cube slab remains a hidden unused component (`GhostMesh` never rendered).

## Per-cell classification model
Presentation-only `EGP_PlacementPreviewCellState`: `Free`, `Occupied`, `OutOfRange`, `NotNavigable`, `WorldBlocked`.

Priority: Occupied > OutOfRange > NotNavigable > WorldBlocked > Free.

Whole placement is invalid if any cell is invalid. Text uses the dominant reason (`BLOCKED: OCCUPIED` / `OUT OF RANGE` / `NOT NAVIGABLE` / `WORLD` / `VALID`). Occupancy uses server `IsCellOccupied` when present plus replicated building/DropPod footprints. Radius uses `UGP_OrbitalDeliverySettings::BuildingMaxDeployRadiusFromMainBaseCm`. Nav/world reuse existing 1×1 subsystem helpers. Server `ValidateBuildingPlacement` is unchanged.

Invalid local LMB still does not send deploy RPC.

## Ghost behavior
Valid: building ghost visible, centered on snapped footprint, footprint fill remains.  
Invalid: building ghost hidden; mixed cells keep free cells non-red.

## Ground-Z fix
Cursor placement trace ignores `AGP_BuildingBase`, `AGP_DropPod`, placement ghost, and pawns. After XY hit, `ResolvePreviewGroundZ` does a vertical WorldStatic/WorldDynamic/Visibility trace with the same ignore list so preview stays on deploy ground, not building tops.

## Camera-facing text
Each preview update: `GetPlayerViewPoint` → `StatusText` world rotation along (Camera − Text). Text stays at snapped footprint center, Z +160 cm.

## Tests (all Failures=0)
- `gp.Building.RunBuildGridContractTest` — mixed-validity cell states (not monolithic), building ghost show/hide, ground Z ignores elevated building over a WorldStatic slab, label mapping, cancel clears
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

## Next operator visual retest
1. Purchase Hub → Deploy.
2. Free area: filled footprint, green, building ghost visible, `VALID`.
3. Partial overlap: only blocked cells red, free cells non-red, reason text.
4. Over MainBase / placed Hub: preview on ground, blocked cells red, `BLOCKED: OCCUPIED`.
5. Outside radius: invalid feedback, `BLOCKED: OUT OF RANGE`.
6. Text faces camera.
7. RMB/Esc: preview disappears.
8. Stop PIE / close Editor: no crash.

## Exact files changed during this preview polish
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropAuthority.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingPlacementGhost.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingPlacementGhost.cpp`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

Not committed (operator-local): `DefaultEngine.ini`, `DefaultGame.ini`, `L_PrototypeArena.umap`, `BP_ResourceNode_AuthoredExample.uasset`, Blueprint/Materials/VFX packs, `Tools/`, AutoAcquire CRLF noise.

## Explicit
**NOT MERGED.**  
**NOT FINALIZED.**
