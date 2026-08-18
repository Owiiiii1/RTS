# GP-S40R — Timed Retaliation Pursuit

## Status
**GP-S40R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Slice Group
Post-GP-S39E (Economy / Logistics Data is on verified `main`)

## Branch
`feature/gp-s40r-timed-retaliation-pursuit`  
Base: `origin/main` @ `5ad69aa7abd39e181cd6ffafb11e4277adf3160a`  
Implementation tip: `fc57f0dd44dc7211c2e5e4905c04117207104d90` (LOS handoff correction)

## Goal
Server-authoritative timed retaliation for eligible **mobile combat** units, reused on the existing Attack FSM / LOS fire gate / movement serials. No parallel combat system.

## Ownership

| Concern | Owner |
| --- | --- |
| Duration | `UGP_UnitDefinition.RetaliationPursuitSeconds` via `AGP_UnitBase::GetRetaliationPursuitSeconds()` (fallback 5.0; 0 disables) |
| Notify seam | `AGP_UnitBase::ApplyDamageFromUnit` after **successful** apply, if target still alive |
| Runtime | `UGP_UnitCommandComponent` — weak attacker, timeout/evaluate timers, retaliation move serial, cancel |
| Fire / LOS | existing Attack Ready cadence + `GPCombatLOS` |
| Movement | existing `UGP_MovementComponent::RequestMove` / serial results |

Retaliation is **not** a player-visible `FGP_UnitCommand`. AttackMove ownership is unchanged.

## Attacker replacement policy

While retaliation **owns** behavior: latest valid hostile attacker replaces the previous target. Same-attacker hits refresh the timeout only (no Held-command thrash).

Never replace a manual Held `Move` / `Attack` / `AttackMove` / `Mine` / haul chain. Incoming HandleCommand cancels retaliation first.

## Eligibility

- Authority only
- Target alive; `GetRetaliationPursuitSeconds() > 0`
- Attacker valid, alive, hostile (`ValidateAttackTarget`)
- Owner is mobile combat-capable (current factual: Salvage Walker). Worker is not combat-capable. Buildings / Defensive Turret must not start movement retaliation
- Start blocked if Attack FSM already active or Held Move/Attack/AttackMove/Mine / mine-haul active

Handoff to the existing Attack FSM requires **both** effective auto-acquire / sight range **and** `GPCombatLOS::HasLineOfSight`. If the attacker is inside sight but LOS is blocked, retaliation stays owner and continues pursuit. Timeout with LOS still blocked returns Idle with no Held Attack. The Attack Ready fire gate is unchanged.

## Validation (candidate)

| Check | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** (LOS correction rebuild) |
| `gp.Combat.RunRetaliationPursuitContractTest` | `Complete Failures=0 Cancelled=false` (includes blocked-LOS A/B/C) |
| `gp.Combat.RunLOSFireGateContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunAutoAcquireContractTest` | `Complete Failures=0 Cancelled=false` |
| AttackMove / movement reconciliation | **NOT RERUN** this correction |
| `GP` Win64 Development / Shipping | **NOT RUN** (candidate only) |

## Out of scope

Economy/resource/building acquisition, FoW, Wall, AI opponent, rebalance, new Tick, second damage path.
