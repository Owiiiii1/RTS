# GP-S32A — Attack-Move Reconciliation

## Status
**GP-S32A_FINALIZATION_READY_FOR_MERGE**

Operator FULL PASS. NOT MERGED — await human merge/check.

## Slice Group
Combat QoL (post GP-S30R)

## Branch
`feature/gp-s32a-attack-move-reconciliation`  
Base: `main` @ `989ca3fe6eae31b177ba2fade2ca1f02300d3326`  
Operator-validated candidate: `5e6b3f192821a92737cd16e98e2aaa99ef73f7b6`

## Goal
Production MVP Attack-Move on existing Move + GP-S30R auto-acquire + Attack FSM + Sight/AttackRange + LOS + Damage + Facing. No parallel combat/movement architecture.

## Operator FULL PASS (confirmed)
1. A → LMB ground beyond enemy → travel → detect → fight → resume **original** AttackMove destination after death  
2. During engagement, RMB Move elsewhere → abandon AttackMove/combat immediately → obey Move → **no** stale AttackMove resume  

## Preserved semantics
- A modal → LMB confirm; Esc/RMB cancel; no confirm click-through  
- SalvageWalker only; Worker ineligible  
- Pure Move suppresses auto-acquire; AttackMove opts into Sight acquire while travelling  
- Existing Attack FSM / LOS / Damage / Cooldown / Facing  
- Destination persists during temporary engagement; arrival → Idle  
- Move / explicit Attack / Stop replace permanently; new AttackMove replaces prior  
- Buildings excluded this slice  

## Contract / builds (finalization)
- Final regressions Failures=0 (combat + resource + building list)  
- GPEditor Win64 Development + UHT **PASS**  
- GP Win64 Development **PASS**  
- GP Win64 Shipping **PASS**  

## Out of scope
NavMesh/pathfinding · collision · local avoidance · formations · group spreading · SWARM · building targeting · Unit Cap · Hub bonuses · BuildingDefinition/BuildGrid · FoW · production UI  

## Approved planning order (after human merge — do not auto-start)
1. RTS Movement Reconciliation  
2. Unit Cap + LogisticsHub gameplay  
3. Match win flow  
4. BuildingDefinition / BuildGrid  

## Stop Condition
Human merge/check. Do **not** auto-merge. Do **not** auto-assign the next code slice.
