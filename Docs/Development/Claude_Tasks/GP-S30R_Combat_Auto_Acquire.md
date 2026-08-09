# GP-S30R — Combat Auto-Acquire

## Status
**GP-S30R_FACING_AND_SIGHT_READY_FOR_OPERATOR_RETEST**

NOT MERGED. Do **not** finalize until operator retest PASS.

## Slice Group
Combat QoL (post GP-S32R roadmap reconciliation)

## Branch
`feature/gp-s30r-combat-auto-acquire`  
Base: `main` @ `ba98383ffca90dafc4645b8761bfaeb93fa5cdc2`

## Goal
Salvage Walker server-authoritatively auto-engages the nearest valid enemy unit within **Sight / AutoAcquire range**, then drives the existing Attack FSM (Approaching → Ready → fire under AttackRange + LOS). Ready-state yaw faces the Attack target.

## Architecture
- **No** new `UGP_TargetingComponent` / `UGP_CombatComponent`
- Integrated into `UGP_UnitCommandComponent` (rate-limited World timer + Ready facing tick)
- Combat-capable gate: `GP.Unit.Type.SalvageWalker`
- Issues synthetic `Command_Attack` via `HandleCommand` (same path as player Attack)
- Buildings excluded from auto-acquire MVP
- Minimal `Command_Stop` support (PC validate + UnitCommand clear Held / stop move)

## Range / facing semantics
| Parameter | Role |
|---|---|
| `AttackRange` (GAS) | Fire / engage range only |
| `AutoAcquireSightRangeCm` | Sight / scan range (not fire range) |
| Effective acquire | `max(AutoAcquireSightRangeCm, AttackRange)` |
| Ready facing | Authority yaw-only `RInterpConstantTo` on owning actor |

## Tuning seams (no second SoT for Damage/Cooldown/AttackRange)
| Field | Source / edit |
|---|---|
| AttackRange | GAS `UGP_UnitAttributeSet::AttackRange` (SW CDO `DefaultAttackRange=600`) |
| Damage | GAS `Damage` (SW CDO `DefaultDamage=20`) |
| AttackCooldown | GAS `AttackCooldown` (SW CDO `DefaultAttackCooldown=1`) |
| AutoAcquireSightRangeCm | `UGP_UnitCommandComponent` EditAnywhere (default 900; SW CDO sets 900) |
| AutoAcquireScanIntervalSeconds | same component (default 0.35) |
| AttackFacingRotationSpeedDegreesPerSecond | same component (default 360) |

## Contract
`gp.Combat.RunAutoAcquireContractTest` — base auto-acquire + sight-outside-AttackRange approach/no OOR damage + facing A/B/C — Failures=0

## Builds
| Target | Policy |
|---|---|
| GPEditor Win64 Development + UHT | **Required PASS** |
| GP Dev / Shipping | **NOT RUN** |

## Out of scope
Attack-Move · SWARM priority · Turret AI · BuildingDefinition · BuildGrid · Hub bonuses · FoW · Order Menu · CombatComponent resurrection · separate aim animation system

## Operator retest sketch
1. Enemy inside Sight but outside AttackRange → auto-acquire + approach; no damage until in AttackRange  
2. Ready attack with target beside Walker → Walker yaws toward target  
3. Moving target while Ready → facing tracks  
4. Idle / no attack → no arbitrary combat spin  

## Stop Condition
Operator retest. Do **not** auto-finalize / merge. Do **not** auto-assign Attack-Move.
