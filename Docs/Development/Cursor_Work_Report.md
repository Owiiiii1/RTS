# Cursor Work Report

Status: **GP-S36G_VISUAL_FEEDBACK_FIX_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**  
**NOT FINALIZED.**

## Branch
`feature/gp-s36g-buildgrid-mvp`

## Base main SHA
`6f258a1069fd92a45f99faf7c877c941528beb2a`

## Prior remote feature head (before this visual fix)
`1df431c` / previous visual-feedback SHA-record on the same branch.

## Feature head SHA
*(filled in SHA-record commit)*

## Screenshot-observed failure
Operator screenshot:

- large opaque/gray 800×800 rectangle followed the cursor;
- `VALID` / blocked text appeared on/near that rectangle;
- green/red 4×4 grid lines appeared around world origin / map center, not under the cursor;
- grid color changed correctly but was spatially detached from the placement ghost.

Server/grid mechanics previously PASS and were not changed.

## Factual root cause
`ULineBatchComponent::DrawLine` stores `Start`/`End` and the scene proxy draws them through `PDI->DrawTranslucentLine` with **no LocalToWorld transform**. `CalcBounds` documents: "Points are in world space, so no need to transform." `ApplyWorldOffset` adds a world offset to those same points.

`UpdateGridPreview` converted `GetFootprintWorldAABB` into actor-local coordinates by subtracting `GetActorLocation()`. Those local deltas were then passed to `DrawLine` as if they were world positions, so a ghost at e.g. `(2100, 1300)` drew an 800×800 grid around world origin.

No arbitrary offset compensation was applied. The cube fill was a separate UX failure: Engine BasicShapes Cube default material is opaque gray, so the scaled fill slab dominated the preview.

## Exact world/local coordinate correction
`UpdateGridPreview` now draws `GetFootprintWorldAABB` directly:

- `(Min.X, Min.Y, GroundZ + 24)`
- `(Max.X, Max.Y, GroundZ + 24)`
- internal cell divisions at `Min + n * CellSize`

No `ActorLocation` subtraction. `UTextRenderComponent` stays attached to the ghost root (snapped footprint center, Z +90) so status text travels with the snapped placement, not the world origin.

Line lifetime is `0` (engine meaning: persist until `Flush()`, not a 3600s fake TTL). Each preview update flushes then redraws one footprint. Cancel / confirm / leave mode / EndPlay flush and hide text. Line batch tick is disabled; no extra actor Tick.

## GhostMesh gray-fill handling
`GhostMesh` is retained as a component for compatibility but is never assigned a mesh and is forced `HiddenInGame` / not visible during placement. Primary visual is grid lines + status text only. No authored material, no new `.uasset`, no `/Game/...` path.

## Tests (all Failures=0)
- `gp.Building.RunBuildGridContractTest` — added world-space endpoint checks: 4×4 snapped near `(2000, 1200)` matches AABB, no line endpoints near `(0,0)`, cancel still clears stored segments; ghost fill remains hidden
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
Purchase Logistics Hub → Deploy.

- no large gray rectangle;
- 4×4 grid under mouse / snapped placement;
- free = green `VALID`;
- MainBase/Hub overlap = red `BLOCKED: OCCUPIED`;
- outside radius = red `BLOCKED: OUT OF RANGE`;
- mouse move → grid steps 200 cm with snap;
- no leftover grid at map center;
- Esc/RMB clears preview.

## Exact files changed during this visual fix
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingPlacementGhost.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingPlacementGhost.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

Not committed (operator-local): `DefaultEngine.ini`, `DefaultGame.ini`, `L_PrototypeArena.umap`, `BP_ResourceNode_AuthoredExample.uasset`, Blueprint/Materials/VFX packs, `Tools/`, AutoAcquire CRLF noise.

## Explicit
**NOT MERGED.**  
**NOT FINALIZED.**
