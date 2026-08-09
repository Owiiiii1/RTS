# GP-S33M — RTS Movement Reconciliation

## Status
**GP-S33M_FINALIZATION_READY_FOR_MERGE**

Operator validation **FINAL PASS**. Final regressions Failures=0. GPEditor+UHT / GP Development / GP Shipping **PASS**.  
**NOT MERGED** — await human merge/check onto `main`. Do **not** auto-start the next code slice.

## Slice Group
Post-GP-S32A combat QoL / movement production layer

## Branch
`feature/gp-s33m-rts-movement-reconciliation`  
Base: `main` @ `0df4468445e939aaca33ed73548a78c2caabb86d`  
Operator-validated / finalization head: see `Docs/Development/Cursor_Work_Report.md` (feature tip after finalization commit)

## Goal
Replace straight-line-only movement with a minimal production RTS movement layer while keeping a single backend (`UGP_MovementComponent`) and existing Move / Attack / AttackMove / Mine / Haul contracts.

## In scope (delivered)
1. NavMesh pathfinding around static obstacles (`UNavigationSystemV1` / Recast)
2. Soft unit presence + local separation (no physics sim, no hard Pawn block deadlocks)
3. Deterministic group destination spreading for Move / AttackMove at dispatch
4. `EGP_MovementResult::Failed` + reject reasons for nav failures (no straight-line through known unreachable obstacles)
5. Contract `gp.Movement.RunRTSMovementReconciliationContractTest`
6. `AGP_BuildingBase::NavigationObstacle` authored dynamic NavArea_Null box (Blueprint-authorable)
7. Worker SlotFull reassignment → CargoFull → automatic haul (natural chain ownership fixes)
8. Nav-aware reachable MainBase haul approach (multi-candidate, complete FindPathSync)
9. MainBase drop-off **GroundPlane2D** Dist2D semantics; ResourceNode mining keeps **ThreeDimensional**
10. Contracts: `gp.Resource.RunMineReassignmentHaulContractTest`, `gp.Resource.RunHaulNavApproachContractTest`

## Out of scope (deferred)
MassAI · AIController-per-unit · formation persistence · docking slots · BuildingBase redesign beyond nav footprint · local Build/Produce architecture · map/config/content commits

## Architecture (final)
- Single backend: `RequestMove` / `StopMove` / `OnMovementResult`
- Server-authoritative; replicated actor movement; no client gameplay prediction
- On NavMesh: pathfind; missing/unavailable nav → straight-line fallback only then
- On-nav unreachable → PathNotFound / reject (no bypass through NavArea_Null footprints)
- Capsules: Pawn=Overlap (separation); WorldStatic=Ignore (static avoidance via NavMesh)
- Buildings: independent `NavigationObstacle` box; dynamic obstacle / NavArea_Null
- MainBase unload: `Dist2D(worker, MainBase) <= DropOffRangeCm` (actor-origin Z ignored)
- Move / Attack / AttackMove preserved; AttackMove resumes assigned destination after combat
- Manual Mine + full cargo remains rejected

## Operator validation FINAL PASS
- NavMesh exists; units pathfind and route around obstacles
- Unit↔unit avoidance/separation; group move does not collapse to one point
- Building NavigationObstacle Blueprint-authorable; units route around it
- Worker automatic reassignment; CargoFull → haul; reachable MainBase approach
- Workers unload and continue the resource chain

## Final builds / regressions
- Listed final regression suite: **Failures=0**
- GPEditor Win64 Development + UHT: **PASS**
- GP Win64 Development: **PASS**
- GP Win64 Shipping: **PASS**
- Finalization C++ changes: **none** (docs only)

## Stop Condition
Finalization complete. **NOT MERGED.** Do **not** auto-start next slice. Await human merge/check.
Approved planning order after merge: (1) Unit Cap + LogisticsHub gameplay (2) Match win flow (3) BuildingDefinition / BuildGrid
