# GP-S29R — Combat LOS Fire Gate + Minimal Health Bar + Team Color Presentation

## Status
**GP-S29R_DONE_MERGED**

Operator validation **PASS**. Final builds **PASS**. Merged on `main` @ `3673a6891b3638592da115887d95e373d2475b1e`.

See post-merge next-slice audit: [`../Next_Slice_Audit_Post_S29R.md`](../Next_Slice_Audit_Post_S29R.md). NEXT coding stage is **not** automatically started.

## Slice Group
Slice 7 — Combat (reconciliation path; preserves Attack FSM, not CombatComponent)

## Code Allowed
**YES** (implementation complete; finalization docs/builds only)

## Depends On
- Main @ `d75fb426b043c80005c8363bef0f61ac37408fc5` (Merge GP-S28P4 planetary Ferronite HUD)
- Reference audit: `audit/gp-slice7-combat-reconciliation-refresh` (docs only; **not** merged into this branch)
- Canonical LOS: `Docs/TDD/04_RTS_Selection_And_Commands.md` multi-point Visibility policy

## Branch
`feature/gp-s29r-combat-los-healthbar-teamcolors`

## Implementation summary

### LOS fire gate
- `GPCombatLOS` helpers: 3-pair `ECC_Visibility` traces (Eye→Head, Chest→Chest, Feet→Feet); ANY clear pair succeeds; ignore Source; Target hit = clear; fail-closed.
- Wired in `UGP_UnitCommandComponent::AttemptAttackHit` after range/hysteresis, before GAS damage.
- Blocked LOS: no damage, no successful-hit presentation, no successful-hit cooldown spend, Attack intent retained (Ready retries via existing cadence Tick).
- Transition diagnostics: `AttackLOSBlocked` / `AttackLOSRestored` (no per-retry spam).

### Accepted temporary LOS-blocked semantics (operator-validated)
When target is in range and LOS is blocked:
- do not fire / do not apply damage;
- do not spend successful attack cooldown;
- do not cancel Attack;
- stay in place;
- periodically re-check LOS;
- automatically resume fire when LOS clears (no new Attack command).

**Future work (explicitly not S29R):** navigation / pathfinding / obstacle avoidance / firing-position search / repositioning around LOS blockers / TargetingComponent / AttackMove / auto-acquire.

### Health bar
- `UGP_HealthBarComponent` (WidgetComponent) + `UGP_HealthBarWidget` (NativePaint green fill / dark frame).
- Owned by `AGP_UnitBase` (covers Workers + MainBase/buildings + Salvage Walker).
- GAS Health/MaxHealth SoT via ASC attribute change delegates; initial sync on BeginPlay; no health polling Tick.
- Fill stays green for all teams.

### Team colors
- `UGP_GameplayPresentationSettings` (`Config=Game`) — Project Settings → Game → GP Gameplay Presentation.
- `FGP_TeamPresentationStyle` array + `GetTeamColor(TeamId)`; defaults Team1 BLUE / Team2 RED / unknown WHITE.
- `UGP_TeamPresentationComponent` on `AGP_UnitBase`; refresh on BeginPlay, `NotifyTeamIdChanged`, `OnRep_TeamId`.
- Native prototype tint via MID vector params (`TeamColor` preferred) + `UGP_UnitVisualComponent::RefreshTeamColorFromPresentation`.

### Salvage Walker
- Native `AGP_SalvageWalker : AGP_Unit`.
- Operator-created `BP_SalvageWalker` (operator-local; not committed by agent).
- Defaults: Health/MaxHealth 200, Damage 20, AttackCooldown 1.0, AttackRange 600, MoveSpeed 250, VisualSourceMode=AuthoredComponents.
- No Cargo/Mining; single Movement; single UnitVisual.

### Unit Details cleanup
- Actor pointers: `GP|Components|Movement`, `GP|Components|Visual` (tunables remain `GP|Movement` / `GP|Visual`).

## Automated contracts
| Command | Coverage |
| --- | --- |
| `gp.Combat.RunLOSFireGateContractTest` | LOS / resume / cooldown / approach / FF / death / LOS latch reset |
| `gp.Combat.RunHealthBarContractTest` | ratio / frame / zero / initial / no Tick |
| `gp.Combat.RunTeamColorContractTest` | settings resolve / apply / change / OnRep / no mutate TeamId |
| `gp.Combat.RunSalvageWalkerContractTest` | native class hierarchy / composition / GDD defaults |
| `gp.Resource.RunS28RegressionSuite` | resource regression |

Expected: Failures=0.

## Operator validation
**PASS** (PIE): Team Colors; Health Bar (incl. Salvage Walker); Salvage Walker authored visual/combat; Combat Attack/damage/death; LOS block/restore; LOS log spam fix.

## Build
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **PASS** (finalization)
- GP Win64 Shipping — **PASS** (finalization)

## Scope exclusions (confirmed)
No TargetingComponent, AttackMove, cooldown GE, projectiles, damage numbers, shields/armor bars, team-colored health fill, selection/minimap/FoW redesign, resource/construction changes, duplicate CombatComponent, pathfinding/repositioning, AI/SWARM, new combat unit types.

## Stop Condition
GP-S29R **DONE / MERGED** on `main` @ `3673a689…`. Do **not** reopen S29R scope. Next stage requires approval of GP-S30 spec (container launch) — see post-S29R audit.
