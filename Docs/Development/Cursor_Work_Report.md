# Cursor Work Report

## Task
GP-S25B Ready/Approaching boundary hysteresis (anti-thrashing)

## Status
GP-S25B_CODE_READY_OPERATOR_VALIDATION_PENDING

## Branch
feature/gp-s25b-attack-cadence-integration

## Base
main @ 7864b2bc45060f48021f46a1711d71fd62b0f3da

## Cause
With AttackRange=250 and a moving target oscillating Dist ≈ 248–256, Ready used a hard `Distance > EffectiveRange` exit. Each exit started movement; RangeEntryStop re-entered Ready → MoveStarted/MoveStopped/AttackReady spam. Cadence (`NextHitTime` / `FirstHitAttempted`) was already preserved across OOR; the defect was boundary thrashing, not cooldown loss.

## Fix
`AttackReadyExitTolerance = 20.0f` (private `static constexpr` on `UGP_UnitCommandComponent`):

| Transition | Condition |
| --- | --- |
| → Ready (entry) | `Distance <= EffectiveRange` (unchanged) |
| Ready → Approaching | `Distance > EffectiveRange + ExitTolerance` (AttackExitRange) |
| Hit while Ready | Damage only if `Distance <= EffectiveRange`; hysteresis band stays Ready without Approaching and without clearing NextHitTime |

Approaching transition log includes `AttackExitRange` and `ExitTolerance` (not per-frame).

## Files Changed
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h` — `AttackReadyExitTolerance`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp` — EvaluateAttack / AttemptAttackHit / Approaching transition log
- `Docs/Development/Cursor_Work_Report.md` — this report

## Preserved
- Immediate first hit; NextHitTime; FirstHitAttempted; cooldown cadence
- GAS AttackRange → component fallback
- Moving-target refresh; SelfSupersede; RangeUnreachable
- Damage path; death lifecycle; command replacement
- Movement component core
- Idle/Approaching → Ready entry threshold (no early hits on new Attack)

## Static Scenario Checks
- Dist 248–256 @ range 250 → remain Ready; no thrash movement
- Dist > 270 → Ready → Approaching; log AttackExitRange / ExitTolerance
- Re-entry only at Dist <= 250; hit after cooldown when back inside entry range
- AttackRange=1 unreachable → still RangeUnreachable
- Far moving-target approach refresh unchanged
- Automated Attack tests — none present in repo

## Build Results
- GPEditor Win64 Development — Succeeded (18.14s); compiled GPUnitCommandComponent.cpp / GPUnitBase.cpp / Module.GPRuntime.cpp; linked UnrealEditor-GPRuntime
- UHT — processed GPEditor successfully (header touched; 0 generated files written)
- Automated Attack tests — N/A (none in repo)

## Commit SHA
e5333fa7ef853bab2018648d273ffb6ecee7b695

## Git State
- Push to `feature/gp-s25b-attack-cadence-integration`
- No merge to main; no PR; no finalization
