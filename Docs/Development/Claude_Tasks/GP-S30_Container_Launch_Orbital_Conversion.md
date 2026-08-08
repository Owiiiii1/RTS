# GP-S30 — Container Launch / Orbital Conversion

## Status
**SPEC_READY_FOR_APPROVAL**

## Slice Group
Slice 8 — Buildings + Orbital Drops (economy unlock vertical; first coding slice after GP-S29R)

## Code Allowed
**NO** — until explicit tech-lead / operator approval of this spec.

## Depends On
- `main` @ `3673a6891b3638592da115887d95e373d2475b1e` (GP-S29R merged)
- GP-S28 Storage + Worker haul (merged)
- ADR-0009 Orbital Delivery (Accepted)
- Audit: [`../Next_Slice_Audit_Post_S29R.md`](../Next_Slice_Audit_Post_S29R.md)

## Goal
Authority-side MainBase container launch that converts Ready planetary Ferronite into spendable `OrbitalFerronite` + cumulative `FerroniteScore`, and lowers `FerroniteThreatValue`.

## Why Now
Planetary mine→store is playable. Without launch, First_Playable cannot produce score/currency, Order Menu cannot spend, and win/SWARM relief fantasy cannot start. Navigation/AttackMove improve combat QoL but do not unlock the MVP economy gate.

## In Scope
- Production launch path for `UGP_StorageComponent` containers in `Ready` (not debug-only scaffold).
- Instant GAS applications on owning `AGP_PlayerState` ASC:
  - `GE_GP_AddOrbital` → `OrbitalFerronite`
  - `GE_GP_AddScore` → `FerroniteScore`
- Lower team `FerroniteThreatValue` on successful launch (per ADR-0009).
- Clear Ready → Launching → complete/empty lifecycle using existing Storage model.
- DA-driven conversion rates / telegraph duration (ResourceGameplaySettings and/or ResourceDefinition — no hardcoded balance in C++).
- Authority-only mutation; cosmetic launch FX stub allowed (Multicast cosmetic only).
- Non-shipping contract test for launch conversion invariants.
- Minimal docs sync (task + AI_Project_Log / index cursor after implementation — not this approval stage).

## Out of Scope
- `UGP_OrbitalDeliverySubsystem`, `AGP_DropPod`, Order Menu UI / `Server_RequestOrbitalDrop`
- Logistics Hub, walls, turrets, build grid
- Pathfinding / NavMesh pathfollowing / LOS repositioning
- TargetingComponent / AttackMove / CombatComponent
- SWARM wave spawning / AI opponent / FoW / Steam
- Match DeliveryQuota / timer win evaluation wiring (follow-on once score is live)
- Changing GP-S29R LOS hold-and-retry semantics
- Worker Repair ability
- Committing operator-local maps / Blueprints / Materials

## Existing Systems To Reuse
- `UGP_StorageComponent` (Ready / Launching scaffold)
- `UGP_PlayerAttributeSet` (`OrbitalFerronite`, `FerroniteScore`)
- `AGP_GameState` FerroniteThreatValue APIs
- PlayerState ASC / Instant GE patterns (mirror `UGP_GE_Damage_Basic` discipline)
- Worker → MainBase haul (unchanged)
- Existing resource settings / Ferronite definition soft refs

## Explicit Anti-Duplication Constraints
- Do **not** invent a second storage SoT or client-writable OrbitalFerronite.
- Do **not** grant Orbital/Score from mining or drop-off (only launch).
- Do **not** create CombatComponent / TargetingComponent / AttackMove in this slice.
- Do **not** create OrbitalDeliverySubsystem “while we are here”.
- Do **not** treat historical TDD/13 “GP-S30 = TargetingComponent” as this task’s meaning.

## Authority / Network Rules
- Launch validation + Storage mutation + GE apply + Threat mutate = **server/authority only**.
- Clients observe via replication (attrs / storage / threat) + optional cosmetic Multicast.
- No client-side conversion math.

## Data-Driven Constraints
- Conversion rates, telegraph duration, container capacity coupling — DataAsset / Project Settings.
- Soft refs for definitions; no hard content paths in Runtime hot paths.

## Automated Contract Plan
- `gp.Resource.RunContainerLaunchContractTest` (or equivalent non-shipping staged runner):
  - Ready container required
  - Authority launch succeeds once
  - Planetary amount decreases / container leaves Ready appropriately
  - OrbitalFerronite and FerroniteScore increase by expected DA math
  - FerroniteThreatValue decreases by launched volume rules
  - Reject: non-authority, empty/non-Ready, invalid owner
- Regression: `gp.Resource.RunS28RegressionSuite` must remain Failures=0

## Operator Validation Plan
- Existing Worker mine → haul → fill MainBase to Ready
- Trigger launch (console/debug UI acceptable for v1 if Order Menu absent)
- Confirm OrbitalFerronite + FerroniteScore rise; Threat falls; storage updates
- Confirm no combat/movement regressions in smoke PIE

## Build Policy (when Code Allowed = Yes)
1. GPEditor Win64 Development + UHT — PASS required  
2. GP Win64 Development — PASS required  
3. GP Win64 Shipping — PASS required  

## Pillar 8 — MVP Gate (pre-approval)
1. Fun now? **Yes** — first ship-to-orbit payoff  
2. Clear to new player? **Yes** — fill, launch, get currency/score  
3. New decision type? **Yes** — launch timing vs stockpile / Threat  
4. Cheap & fast? **Yes** — Storage Ready + existing attrs  
5. Scales via content? **Yes** — DA rates  

**Verdict: PASS**

## Stop Condition
Spec approved or rejected by tech-lead/operator.  
**No C++ / content implementation** until `Code Allowed` flipped to Yes in an explicit follow-up instruction.  
Do not start GP-S31+ DropPod / pathfinding / AttackMove without a separate approved task.
