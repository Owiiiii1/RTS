# GP-S32A — Attack-Move Reconciliation

## Status
**GP-S32A_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Slice Group
Combat QoL (post GP-S30R)

## Branch
`feature/gp-s32a-attack-move-reconciliation`  
Base: `main` @ `989ca3fe6eae31b177ba2fade2ca1f02300d3326`

## Goal
Production MVP Attack-Move on existing Move + GP-S30R auto-acquire + Attack FSM + Sight/AttackRange + LOS + Damage + Facing. No parallel combat/movement architecture.

## Semantics
A → local Attack-Move modal → LMB ground → `GP.Command.AttackMove`  
Travel → acquire in effective Sight → Attack FSM → on target loss resume original destination → arrive → Idle.

## Architecture
- Tag: existing `GP.Command.AttackMove`
- Validate: SalvageWalker-only issuers (Worker → UnsupportedUnit)
- Held AttackMove owns destination; engagement runs Attack FSM under same serial without replacing Held
- Idle auto-acquire unchanged (pure Move still suppresses)
- AttackMove opt-in acquire via `IsEligibleForAttackMoveAcquire`

## Contract
`gp.Combat.RunAttackMoveContractTest` — Failures=0

## Builds
| Target | Policy |
|---|---|
| GPEditor Win64 Development + UHT | **Required PASS** |
| GP Dev / Shipping | **NOT RUN** (finalization) |

## Out of scope
NavMesh/pathfinding · collision · local avoidance · formations · group spreading · Attack-Move Worker combat · SWARM · building targeting · FoW

## NEXT
Await operator validation. Do **not** auto-start RTS Movement Reconciliation.
