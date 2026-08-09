# GP-S30R — Combat Auto-Acquire

## Status
**GP-S30R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Slice Group
Combat QoL (post GP-S32R roadmap reconciliation)

## Branch
`feature/gp-s30r-combat-auto-acquire`  
Base: `main` @ `ba98383ffca90dafc4645b8761bfaeb93fa5cdc2`

## Goal
Salvage Walker server-authoritatively auto-engages the nearest valid enemy unit in AttackRange while Idle, using the existing Attack FSM / LOS / damage / presentation pipeline.

## Architecture
- **No** new `UGP_TargetingComponent` / `UGP_CombatComponent`
- Integrated into `UGP_UnitCommandComponent` (rate-limited World timer)
- Combat-capable gate: `GP.Unit.Type.SalvageWalker` capability tag
- Issues synthetic `Command_Attack` via `HandleCommand` (same path as player Attack)
- Buildings excluded from auto-acquire MVP
- Minimal `Command_Stop` support (PC validate + UnitCommand clear Held / stop move)

## Semantics
| State | Behavior |
|---|---|
| Idle | Scan every `AutoAcquireScanIntervalSeconds` (default 0.35s) |
| Explicit Attack | Auto-acquire suppressed (Held Attack / IsAttackActive) |
| Move | Auto-acquire suppressed while Held Move or `IsMoving` |
| Stop | Clears Held + attack → Idle → scan may resume |
| Target death/loss | Existing Attack FSM clears; later scan may reacquire |

## Contract
`gp.Combat.RunAutoAcquireContractTest` — Failures=0

## Builds
| Target | Policy |
|---|---|
| GPEditor Win64 Development + UHT | **Required PASS** |
| GP Dev / Shipping | **NOT RUN** (candidate) |

## Out of scope
Attack-Move · SWARM priority · Turret AI · BuildingDefinition · BuildGrid · Hub bonuses · FoW · Order Menu · CombatComponent resurrection

## Operator PIE sketch
1. Spawn/drop SW + enemy in AttackRange, give no command → SW auto-attacks  
2. Issue Move → disengages and moves  
3. Stop / finish Move → may auto-engage again when Idle

## Stop Condition
Operator validation. Do **not** auto-assign Attack-Move.
