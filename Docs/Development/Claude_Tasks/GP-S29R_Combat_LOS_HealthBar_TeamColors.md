# GP-S29R — Combat LOS Fire Gate + Minimal Health Bar + Team Color Presentation

## Status
**GP-S29R_SALVAGE_WALKER_READY_FOR_OPERATOR_VALIDATION**

## Slice Group
Slice 7 — Combat (reconciliation path; preserves Attack FSM, not CombatComponent)

## Code Allowed
**YES**

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

### Health bar
- `UGP_HealthBarComponent` (WidgetComponent) + `UGP_HealthBarWidget` (NativePaint green fill / dark frame).
- Owned by `AGP_UnitBase` (covers Workers + MainBase/buildings).
- GAS Health/MaxHealth SoT via ASC attribute change delegates; initial sync on BeginPlay; no health polling Tick.
- Fill stays green for all teams.

### Team colors
- `UGP_GameplayPresentationSettings` (`Config=Game`) — Project Settings → Game → GP Gameplay Presentation.
- `FGP_TeamPresentationStyle` array + `GetTeamColor(TeamId)`; defaults Team1 BLUE / Team2 RED / unknown WHITE.
- `UGP_TeamPresentationComponent` on `AGP_UnitBase`; refresh on BeginPlay, `NotifyTeamIdChanged`, `OnRep_TeamId`.
- Native prototype tint via MID vector params (`TeamColor` preferred) + `UGP_UnitVisualComponent::RefreshTeamColorFromPresentation`.

### Salvage Walker (post health-bar / Details cleanup)
- Native `AGP_SalvageWalker : AGP_Unit` for player-facing combat validation.
- Operator creates `BP_SalvageWalker` manually (not in this stage).
- Defaults: Health/MaxHealth 200, Damage 20, AttackCooldown 1.0, AttackRange 600, MoveSpeed 250, VisualSourceMode=AuthoredComponents.

## Automated contracts
| Command | Coverage |
| --- | --- |
| `gp.Combat.RunLOSFireGateContractTest` | A–H LOS / resume / cooldown / approach / FF / death |
| `gp.Combat.RunHealthBarContractTest` | ratio / frame / zero / initial / no Tick |
| `gp.Combat.RunTeamColorContractTest` | settings resolve / apply / change / OnRep / no mutate TeamId |
| `gp.Combat.RunSalvageWalkerContractTest` | native class hierarchy / composition / GDD defaults |

Expected: Failures=0 (operator runs in PIE / existing prototype map).

## Build
GPEditor Win64 Development + UHT — **PASS**

## Operator validation (pending)
See Cursor Work Report §Operator Local Assets / manual checklist (team tint, green health bars, LOS block/resume).

## Scope exclusions (confirmed)
No TargetingComponent, AttackMove, cooldown GE, projectiles, damage numbers, shields/armor bars, team-colored health fill, selection/minimap/FoW redesign, resource/construction changes, duplicate CombatComponent.

## Stop Condition
Code ready; operator validation pending; do **not** start GP-S30; do **not** merge without approval.
