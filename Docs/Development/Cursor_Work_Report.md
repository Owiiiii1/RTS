# Cursor Work Report — GP-S40R Timed Retaliation Pursuit

## Status
**GP-S40R_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head
- Branch: `feature/gp-s40r-timed-retaliation-pursuit`
- Base: `origin/main` @ `5ad69aa7abd39e181cd6ffafb11e4277adf3160a`
- Head: recorded after commit (see git tip of this branch)

## Implementation summary
After **successful** hostile `AGP_UnitBase::ApplyDamageFromUnit`, the living target notifies `UGP_UnitCommandComponent::NotifyHostileDamageReceived`. Eligible idle mobile combat units (factual: Salvage Walker) either enter the existing Attack FSM if the attacker is already engageable, or pursue that attacker for `GetRetaliationPursuitSeconds()` (definition value; fallback 5.0; 0 disables). Fire/LOS remain the existing Ready cadence + `GPCombatLOS`. No second combat system, no new permanent Tick, no player-visible retaliation command.

## Retaliation state ownership
`UGP_UnitCommandComponent` owns runtime only: `bRetaliationActive`, weak attacker, timeout + 0.20s evaluate timers, attacker-death bind, retaliation move serial / last serial. Duration source is `AGP_UnitBase::GetRetaliationPursuitSeconds()` only.

## Attacker replacement policy
While retaliation owns behavior: latest valid hostile attacker replaces the previous target. Same-attacker hits refresh the timeout only (no Held-command thrash). Never replaces a manual player command.

## Manual-command precedence
Start is blocked if Attack FSM, Mine/haul, or Held `Move` / `Attack` / `AttackMove` / `Mine` is active. Any non-queue `HandleCommand` cancels retaliation first (except the internal engage-to-Attack handoff). Stale retaliation movement results are consumed by serial and cannot clear a newer Held Move.

## Tests actually run
Headless `UnrealEditor-Cmd` `-game -NullRHI` on `/Game/GrimProtocol/Maps/L_PrototypeArena`:

| Command | Result |
| --- | --- |
| `gp.Combat.RunRetaliationPursuitContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunAutoAcquireContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunAttackMoveContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Combat.RunLOSFireGateContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | `Complete Failures=0 Cancelled=false` |

Economy / resource / building / full suite: **NOT RUN** (blast radius stayed combat/movement).

## Candidate build
`GPEditor Win64 Development` + UHT **PASS**.  
`GP` Win64 Development / Shipping: **NOT RUN**.

## Changed files
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinition.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Combat/GPCombatRetaliationPursuitContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPCombatRetaliationPursuitContractTest.cpp`
- `Docs/Development/Claude_Tasks/GP-S40R_Timed_Retaliation_Pursuit.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/TDD/05_Unit_Architecture.md`
- `Docs/TDD/06_Building_Architecture.md`

## Protected assets untouched
No operator DataAssets, Blueprints, maps, or `GP/Config` committed.

## Merge
**NOT MERGED. NOT FINALIZED.** Await operator PIE.
