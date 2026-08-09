# GP-S30R — Combat Auto-Acquire

## Status
**GP-S30R_FINALIZATION_READY_FOR_MERGE**

Operator FULL PASS. NOT MERGED — await human merge/check.

## Slice Group
Combat QoL (post GP-S32R roadmap reconciliation)

## Branch
`feature/gp-s30r-combat-auto-acquire`  
Base: `main` @ `ba98383ffca90dafc4645b8761bfaeb93fa5cdc2`  
Operator-validated candidate: `a7338e19eaba39f062b2c075413baf4d671ad1e6`

## Goal
Salvage Walker server-authoritatively auto-engages the nearest valid enemy unit within **Sight / AutoAcquire range**, then drives the existing Attack FSM (Approaching → Ready → fire under AttackRange + LOS). Ready-state yaw faces the Attack target.

## Architecture
- **No** new `UGP_TargetingComponent` / `UGP_CombatComponent`
- Integrated into `UGP_UnitCommandComponent` (rate-limited World timer + Ready facing tick)
- Combat-capable gate: `GP.Unit.Type.SalvageWalker`
- Issues synthetic `Command_Attack` via `HandleCommand` (same path as player Attack)
- Buildings excluded from auto-acquire MVP
- Minimal `Command_Stop` support (PC validate + UnitCommand clear Held / stop move)

## Operator FULL PASS (confirmed)
- Base Idle auto-acquire → Attack/LOS/Damage pipeline
- Sight > AttackRange: discover → approach → no OOR damage → fire in range
- Yaw facing toward current Attack target while Ready/firing
- Overall flow correct

## Preserved semantics
| Item | Value |
|---|---|
| AttackRange runtime | GAS (`UGP_UnitAttributeSet::AttackRange`) |
| SW default AttackRange | 600 |
| AutoAcquireSightRangeCm | 900 |
| Effective sight | `max(configured Sight, AttackRange)` |
| AutoAcquireScanIntervalSeconds | 0.35 |
| AttackFacingRotationSpeedDegreesPerSecond | 360 |
| Damage / AttackCooldown | GAS |
| Explicit Attack | priority over auto-acquire |
| Pure Move | suppresses auto-acquire |
| Stop → Idle | later scan may resume |
| Buildings | excluded this slice |
| Facing | Ready/firing only; yaw only; Approaching = movement-driven |

## Contract / builds (finalization)
- Final regressions Failures=0 (combat + resource + building list including Launch HUD)
- GPEditor Win64 Development + UHT **PASS**
- GP Win64 Development **PASS**
- GP Win64 Shipping **PASS**

## Out of scope (do not expand here)
Attack-Move · pathfinding/collision/avoidance/group spread · SWARM · building/turret targeting · Unit Cap · Hub bonuses · BuildingDefinition/BuildGrid · FoW · production UI

## Approved planning order (after human merge — do not auto-start)
1. Attack-Move reconciliation  
2. RTS Movement Reconciliation (pathfinding / collision / local avoidance / group destination spreading)  
3. Unit Cap + LogisticsHub gameplay  
4. Match win flow  
5. BuildingDefinition / BuildGrid  

## Stop Condition
Human merge/check. Do **not** auto-merge. Do **not** auto-assign the next code slice.
