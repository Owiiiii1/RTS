# Cursor Work Report - GP Slice 7 Combat Reconciliation Refresh

## Status
GP_SLICE7_AUDIT_REFRESH_READY_FOR_REVIEW

## Branch
audit/gp-slice7-combat-reconciliation-refresh

## Base
d75fb426b043c80005c8363bef0f61ac37408fc5

## Audit Source
- Prior branch: `audit/gp-slice7-combat-reconciliation`
- Prior commit: `2120b7d893428a4ad76cf440fa2b12a8e004afaf`
- Prior base: `035c486758059032bb2551520834dd73f8667ef5`
- Old branch not merged; content re-verified on current main

## Current Main Verification
Inspected: `GPUnitCommandComponent`, `GPCommandComponent`, `GPUnitBase`, `GPMobileUnit`, `GPWorker`, `GPCombatPresentationComponent`, `GPDamageApplication`, `GPGE_DamageBasic`, `GPDamageCalculation`, `GPUnitAttributeSet`, `GPGameplayTags`.
P1-P4 resource pass did not change Attack fire/damage/presentation/LOS/FF semantics (UnitCommand gained haul paths only).

## Matrix Summary
COMPLETE 19 / PARTIAL 9 / MISSING 12 / CONFLICTING 1 / OUTDATED 2

## Confirmed Preserved Systems
- Attack Idle->Approaching->Ready + hysteresis + TargetDied
- Authority Instant GE damage + MMC
- Death sink / OnUnitDied
- Unreliable multicast combat presentation
- Hostile-only Attack validation
- No projectile actors

## Confirmed Gaps
- Canonical 3-trace LOS fire gate -> **GP-S29R**
- TargetingComponent / auto-acquire -> GP-S30
- Cooldown GE + AttackCooldown tag gate -> GP-S31R
- AttackMove executor/state -> GP-S32
- Production combat art / Niagara (non-blocking)

## Policy Decisions
- Friendly fire remains **disabled** (intended MVP); TDD ally-Attack wording = future docs correction
- Cooldown GE deferred to **GP-S31R**; keep `NextAttackHitTime` + `AttackCooldown` attribute

## Recommended NEXT
GP-S29R Combat LOS Fire Gate

## GP-S29R Proposed Scope
Preserve Attack FSM; add TDD/04 ECC_Visibility Eye/Chest/Feet 3-pair LOS before applied hit; fail-closed (no damage/no cooldown spend) while blocked; resume on LOS restore; no CombatComponent/Targeting/AttackMove/cooldown GE/projectile/art; contract + operator PIE.

## Scope Audit
docs-only (Combat_Reconciliation_Audit + index/README/log/report). No production C++ / content / config.

## Operator Local Assets
untouched

## Commit
`deacfffc5c7bedf48c41bd0be042c56cfbe19bcd`