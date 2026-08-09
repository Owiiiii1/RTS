# GP-S33M — RTS Movement Reconciliation

## Status
**GP-S33M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED. Await operator PIE retest (Worker natural reassignment haul; Building already PASS).

## Slice Group
Post-GP-S32A combat QoL / movement production layer

## Branch
`feature/gp-s33m-rts-movement-reconciliation`  
Base: `main` @ `0df4468445e939aaca33ed73548a78c2caabb86d`  
Prior candidate: `f72794da169dd8efd3b881443453655547b63264`  
Operator defect revision: 1e34ae348fe99e1cac74b36e26254407128486cf  
Second operator FAIL revision: see HEAD after natural-chain haul fix

## Goal
Replace straight-line-only movement with a minimal production RTS movement layer while keeping a single backend (`UGP_MovementComponent`) and existing Move / Attack / AttackMove / Mine / Haul contracts.

## In scope (done)
1. NavMesh pathfinding around static obstacles (`UNavigationSystemV1` / Recast)
2. Soft unit presence + local separation (no physics sim, no hard Pawn block deadlocks)
3. Group destination spreading for Move / AttackMove at dispatch
4. `EGP_MovementResult::Failed` + reject reasons for nav failures
5. Contract `gp.Movement.RunRTSMovementReconciliationContractTest`
6. **Revision:** `AGP_BuildingBase::NavigationObstacle` authored dynamic NavArea_Null box
7. **Revision:** Worker SlotFull reassignment → CargoFull → automatic haul fix
8. Contract `gp.Resource.RunMineReassignmentHaulContractTest` (rewritten: natural chain only; no teleport/repair Mine)
9. **Revision 2:** Mine/Haul SelfSupersede keep-chain; post-BeginMining ownership reaffirm; orphan CargoFull notify; LastMineDepositForHaul

## Out of scope
MassAI · AIController-per-unit · formation persistence · BuildingBase redesign beyond nav footprint · map/config commits · GP Dev/Shipping builds (candidate gate = GPEditor + UHT only)

## Architecture
- Single backend: `RequestMove` / `StopMove` / `OnMovementResult`
- Server-authoritative; no client gameplay prediction
- Capsules: Pawn=Overlap (separation queries); WorldStatic=Ignore (static avoidance via NavMesh)
- Buildings: independent `NavigationObstacle` box (not capsule); dynamic obstacle / NavArea_Null
- Off-nav / missing NavData → straight-line fallback; on-nav unreachable → reject/fail
- Reassignment updates `HeldCommand.TargetActor` + `MineTarget` to active deposit; CargoFull must haul

## Operator passes
- NavMesh / unit nav / unit avoidance: **PASS**
- Building nav obstacle: **PASS** (second pass)
- Reassigned Worker CargoFull→Haul: **FAIL** (second pass; prior contract false-positive) → **fixed** (natural chain)
- Manual Mine+CargoFull reject: **intentional, preserved**

## Operator setup
Do **not** commit `L_PrototypeArena.umap` / DefaultEngine.ini / DefaultGame.ini.

NavMeshBoundsVolume: as before if missing.

**Runtime orbital buildings:** Project Settings → Navigation Mesh → **Runtime Generation = Dynamic** (local) so spawned `NavigationObstacle` updates Recast.

## Operator retest (Worker first)
Two workers, A full/unavailable, one Mine(A): reassign → mine B → CargoFull → automatic MainBase haul → unload → continue. No manual Mine after cargo full.

## Contract / regressions (revision)
- `gp.Movement.RunRTSMovementReconciliationContractTest` Failures=0
- `gp.Resource.RunMineReassignmentHaulContractTest` Failures=0
- S28 / DropOff / ContainerLaunch / HUD / AttackMove / AutoAcquire / SalvageWalker / LOS / Orbital Unit+Building Drop → Failures=0
- GPEditor Win64 Development + UHT **PASS**
- GP Dev / Shipping **not run**

## Stop Condition
Operator retest A/B. Do **not** merge. Do **not** auto-start next slice.
