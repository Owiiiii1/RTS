# GP-S33M — RTS Movement Reconciliation

## Status
**GP-S33M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED. Await operator PIE validation.

## Slice Group
Post-GP-S32A combat QoL / movement production layer

## Branch
`feature/gp-s33m-rts-movement-reconciliation`  
Base: `main` @ `0df4468445e939aaca33ed73548a78c2caabb86d`  
Candidate: `322385465539436dd426c24ade2912c2b9dbfecd`

## Goal
Replace straight-line-only movement with a minimal production RTS movement layer while keeping a single backend (`UGP_MovementComponent`) and existing Move / Attack / AttackMove / Mine / Haul contracts.

## In scope (done)
1. NavMesh pathfinding around static obstacles (`UNavigationSystemV1` / Recast)
2. Soft unit presence + local separation (no physics sim, no hard Pawn block deadlocks)
3. Group destination spreading for Move / AttackMove at dispatch
4. `EGP_MovementResult::Failed` + reject reasons for nav failures
5. Contract `gp.Movement.RunRTSMovementReconciliationContractTest`

## Out of scope
MassAI · AIController-per-unit · formation persistence · BuildingBase redesign · dynamic orbital building nav carve · map/config commits · GP Dev/Shipping builds (candidate gate = GPEditor + UHT only)

## Architecture
- Single backend: `RequestMove` / `StopMove` / `OnMovementResult`
- Server-authoritative; no client gameplay prediction
- Capsules: Pawn=Overlap (separation queries); WorldStatic=Ignore (static avoidance via NavMesh)
- Off-nav / missing NavData → straight-line fallback; on-nav unreachable → reject/fail

## Operator setup (NavMesh)
Do **not** commit `L_PrototypeArena.umap`.

If PIE has no usable NavMesh:
1. Open `L_PrototypeArena`
2. Place `NavMeshBoundsVolume` covering the playable floor
3. Build Paths (or PIE with runtime generation if project already enables it)
4. Confirm green Recast nav overlay in editor

Local map edits stay operator-local.

## PIE acceptance sketch
**A.** Obstacle between unit and RMB Move destination → unit goes **around**, not through.  
**B.** Select 3–4 units → RMB same point → loose group, no permanent stack, visibly separated slots.  
**C.** AttackMove through same area → nav + combat interrupt + resume **per-unit** assigned slot.

## Contract / regressions (candidate)
- `gp.Movement.RunRTSMovementReconciliationContractTest` Failures=0
- AttackMove / AutoAcquire / SalvageWalker / LOS / S28 / DropOff / ContainerLaunch / HUD / Orbital Unit+Building Drop → Failures=0
- GPEditor Win64 Development + UHT **PASS**
- GP Dev / Shipping **not run** (post-operator finalization)

## Stop Condition
Operator validation. Do **not** merge. Do **not** auto-start next slice.
