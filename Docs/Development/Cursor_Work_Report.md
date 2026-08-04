# Cursor Work Report

## Task
GP-S26 Combat Presentation — analysis / design

## Status
GP-S26_ANALYSIS_READY_FOR_REVIEW

## Branch
feature/gp-s26-combat-presentation-analysis

## Base
main @ b511cf5008546cc421971cd4612cbd92c1a8b945 (Merge GP-S25B attack cadence integration)

## Studied Files
- `GP/Source/GPRuntime/Public|Private/Units/GPUnitCommandComponent.*`
- `GP/Source/GPRuntime/Public|Private/Units/GPUnitBase.*`
- `GP/Source/GPRuntime/Public|Private/Units/GPUnit.*`
- `GP/Source/GPRuntime/Public|Private/Units/GPMovementComponent.*` (via inventory)
- `GP/Source/GPGASRuntime` combat/GAS/tags/effects/MMC/damage application
- `GP/Source/GPUIRuntime` (empty shell) + Build.cs module edges
- `GP/Source/GPRuntime/Public|Private/Player/GPPlayerController.*` (RPC surface)
- `Docs/Development/Claude_Tasks/GP-S24_Attack_Execution_Foundation.md`
- `Docs/Development/Claude_Tasks/GP-S25_Attack_Damage_Execution.md`
- `GP/Content` (Enhanced Input only; no combat assets)

## Findings
- Gameplay Attack/damage/death is complete and server-authoritative; presentation layer is effectively absent.
- Command component: no replication/RPC; AttackState/Serial/Target/cadence are process-local authority fields.
- Clients today: replicated transform, TeamId, bIsDead, GAS attributes — not hit identity or Attack intent.
- Only client combat hook: `ApplyClientDeadPresentation` (collision off).
- No SkeletalMesh/AnimInstance/Niagara/Sound/GameplayCue/multicast cosmetic infrastructure.
- Listen-server double-play risk appears only when future visuals bind both gameplay hit path and replicated receive path.
- Safe emit point: after `AttackHitApplied` (includes blocked); never gate cadence on animation.

## Recommended Architecture
**Hybrid: Option A primary** — authoritative cosmetic presentation event after Apply, with PresentationSequence dedupe and dedicated-server early-out. Option B (replicated Attack state observation) deferred for UI chrome only.

## Recommended GP-S26A Scope
- Emit cosmetic event post-AttackHitApplied
- Payload: PresentationSequence, AttackSerial, Source, Target, EventType, world time, Applied/Blocked/death metadata
- NetMulticast reliable (+ optional LastEvent replicate)
- One generic presentation component; debug draw/logs; no animation/Niagara assets
- Operator validation: listen server + remote client

## Risks
- Listen-server double play if Play is also called from AttemptAttackHit
- Accidental gameplay wait on presentation/AnimNotify
- Ambiguous Health-only inference if event channel skipped
- Late join miss if multicast-only (acceptable for S26A)

## Files Changed
Documentation only:
- `Docs/Development/Claude_Tasks/GP-S26_Combat_Presentation.md` (new)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Build Results
- Not required (docs-only analysis)
- C++ diff: none

## Commit SHA
COMMIT_SHA_PLACEHOLDER

## Git State
- Push to `feature/gp-s26-combat-presentation-analysis`
- No merge to main; no PR; no S26A implementation
