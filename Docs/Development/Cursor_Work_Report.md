# Cursor Work Report — GP-S40R LOS Handoff Correction

## Status
**GP-S40R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head
- Branch: `feature/gp-s40r-timed-retaliation-pursuit`
- Base: `origin/main` @ `5ad69aa7abd39e181cd6ffafb11e4277adf3160a`
- Head: recorded after commit

## Defect (factual review)
`CanEngageRetaliationTarget()` handed off on `Distance <= GetEffectiveAutoAcquireRange()` only. An in-sight attacker behind an obstacle became a Held Attack; if already inside AttackRange the existing fire gate then left the unit Ready with blocked fire instead of continuing pursuit.

## Correction
Handoff now requires **both** existing sight/engagement range **and** `GPCombatLOS::HasLineOfSight(World, Owner, Target)`. No new trace. Attack Ready fire gate unchanged.

## Blocked-LOS pursuit semantics
- In sight + LOS blocked: retaliation stays active; no Held Attack / Attack FSM.
- LOS later clear: one handoff to existing Attack; retaliation timers/movement cancelled.
- Timeout while still blocked: stop retaliation-owned movement, return Idle, no Attack command left behind.

## Tests actually rerun
Headless `UnrealEditor-Cmd` `-game -NullRHI` on `/Game/GrimProtocol/Maps/L_PrototypeArena`:

| Command | Result |
| --- | --- |
| `gp.Combat.RunRetaliationPursuitContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunLOSFireGateContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunAutoAcquireContractTest` | `Complete Failures=0 Cancelled=false` |

AttackMove / movement reconciliation: **NOT RERUN** (shared movement/AttackMove code unchanged).

## Candidate build
`GPEditor Win64 Development` + UHT **PASS**.  
`GP` Win64 Development / Shipping: **NOT RUN**.

## Protected assets
Untouched / not committed.

## Merge
**NOT MERGED. NOT FINALIZED.** Await operator PIE.
