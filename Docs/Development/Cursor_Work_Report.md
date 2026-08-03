# Cursor Work Report

## Task
GP-S25 Attack Damage Execution analysis

## Status
ANALYSIS_READY_IMPLEMENTATION_PENDING

## Branch
feature/gp-s25-attack-damage-analysis

## Base
main @ f0bfb3b0bfa3b96015011cc6c4bd0375d0b6ef69

## Current-State Inventory
- Units: no ASC, no `IAbilitySystemInterface`, no death API (UnitBase comment: ASC/death deferred)
- ASC host today: `AGP_PlayerState` only (Mixed + PlayerAttributeSet)
- `UGP_UnitAttributeSet`: combat attrs default 0; PreAttributeChange clamps; **no** PostGameplayEffectExecute
- `UGP_DamageCalculation` MMC: Source Damage − Armor × (1−Resistance) → −Health magnitude — **already coded**
- No GE_GP_Damage_Basic asset; no unit combat BP/DA
- GP-S24 Attack: Ready without hits; component AttackRange=250; no TargetDied reason
- Tags: `GP.Unit.State.Dead` exists; no Data.Damage / Event.UnitDied
- Module: GPRuntime already depends on GPGASRuntime

## Selected Architecture
UnitBase hosts ASC + UnitAttributeSet; CommandComponent owns hit cadence; GPGAS owns MMC + C++ Instant GE + AttributeSet death notify via thin GPGAS interface (no AttributeSet→UnitBase include cycle).

## Damage Mechanism
Instant C++ `UGP_GE_Damage_Basic` + existing `UGP_DamageCalculation` MMC. Reject SetByCaller dual-Damage and direct Health mutation.

## Attribute Semantics
- Damage = source base damage
- Armor = flat reduction
- DamageResistance = 0..1 fraction (MMC clamp)
- AttackCooldown = seconds between hits
- AttackSpeed = deferred
- No new IncomingDamage meta attribute

## Damage Formula
`Final = max(0, Damage - Armor) * (1 - clamp(Res,0,1))`; min damage 0; friendly/self rejected at command layer.

## Attack Cadence
Immediate-first-hit on Ready; then `NextHitTime = Now + AttackCooldown` (min 0.05s clamp); preserve NextHitTime across temporary OOR; AttackSpeed unused.

## Range Source
GAS AttackRange if finite and >0; else component AttackRange (250). Keep component property in S25.

## Attribute Initialization
EditDefaultsOnly combat defaults on UnitBase + authority `SetNumericAttributeBase` after ASC init. Debug `gp.Combat.SetStats`. No AttributeSet.cpp hardcode; DA deferred.

## Death Contract
PostGEExecute Health≤0 → UnitBase HandleDeath once → replicate bIsDead → Dead tag → stop move/clear commands → disable collision → OnUnitDied → delayed Destroy. GameMode notify deferred.

## Target Death Integration
Bind `OnUnitDied` at Attack start; FinishAttack Failed/TargetDied; unbind on replace/end. Reentrancy-safe with FinishAttack guards; no sync Destroy in GE stack.

## Attacker Death Integration
Owner HandleDeath → command `NotifyOwnerDied` (reset Attack, clear Held, stop move, disable tick).

## Replication
Authority-only Apply; unit ASC Mixed; replicate bIsDead; no damage RPC/prediction.

## Tags
Use existing `GP.Unit.State.Dead`. No new tags required for MMC path.

## Proposed Files
- S25A: UnitBase ASC/death; UnitAttributeSet PostGE; IGP death sink; UGP_GE_Damage_Basic; gp.Combat.*
- S25B: UnitCommandComponent cadence, Apply GE, TargetDied, EffectiveRange
- Build.cs: NO change expected

## Reentrancy Analysis
Death-during-hit FinishAttack idempotent; multi-attacker single death; LifeSpan not sync Destroy; exact serial on retarget.

## Debug/Logging Plan
`gp.Combat.Inspect|SetStats|ApplyDamage|KillTarget` (GE path). Logs: AttackHitAttempt/Applied/Rejected, UnitDied, AttackFinished TargetDied.

## Acceptance Plan
S25A: ApplyDamage/mitigation/single death/dead rejects commands. S25B: Ready cadence, OOR pause, TargetDied, two attackers, replace/retarget, attacker death, QueueDeferred, builds.

## Recommended Slice Split
**GP-S25A** Health & Damage Foundation → **GP-S25B** Attack Cadence Integration. Do not start B before A accepted.

## Risks
ASC init order; AttributeSet include cycle; zero defaults without init; FinishAttack reentrancy; Resistance=1 full block.

## Files Changed
Documentation only:
- `Docs/Development/Claude_Tasks/GP-S25_Attack_Damage_Execution.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Build Results
Not run — analysis-only.

## Git State
- Branch: `feature/gp-s25-attack-damage-analysis`
- Docs-only; working tree clean after push
- HEAD = origin
- no merge to main
