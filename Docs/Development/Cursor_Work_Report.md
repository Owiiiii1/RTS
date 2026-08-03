# Cursor Work Report

## Task
GP-S24 Attack Execution Foundation analysis

## Status
ANALYSIS_READY_IMPLEMENTATION_PENDING

## Branch
feature/gp-s24-attack-execution-analysis

## Base
main @ 60169c1cfb6fd7bd1e701e634a14c7a49395327a

## Current Code Findings
- Attack already delivered into Held via GP-S17/S18; GP-S23 stops prior movement then idles.
- `HandleMovementResult` clears only exact Held Move; Attack Held yields `HeldTagNotMove` today.
- Team rules live on `AGP_UnitBase::GetTeamId`; server rejects same-team Attack.
- ASC / `UGP_UnitAttributeSet::AttackRange` exist but are not wired to units.
- No damage/death/Attack executor exists.

## Selected Architecture
Attack executor state machine private to `UGP_UnitCommandComponent` (Option A). Rejected separate AttackComponent and generic executor framework for GP-S24 scope.

## Attack State Model
`Idle → Approaching → Ready` (Ready non-terminal). Terminal `Cancelled|Failed` + reason. Accept-time invalid clears Held (no phantom).

## Serial Model
Attack Held serial == approach movement serial. Self-supersede Cancelled/Superseded ignored while still moving on same serial. Allocator never rewound.

## Target Validation
Authority + valid `AGP_UnitBase` target ≠ self + finite location + same world + owner TeamId≥1 + not same team; neutrals allowed (matches current server validate). No health interface.

## Movement Integration
Out of range → `RequestMove(target XY, owner Z, AttackSerial)`. In-range entry may `StopMove(Manual)` with `bExpectRangeEntryStop`. Result routing: Attack Approaching consume-first inside `HandleMovementResult`, then Held Move path.

## Held Policy
Retain Attack through Ready. Clear on terminal / accept reject / EndPlay. Replace overwrites Held then resets executor; old callbacks must not mutate new Attack.

## Replacement Matrix
Attack↔Move / Attack↔Attack / Attack→Mine(Held only) / QueueDeferred unchanged — see GP-S24 doc matrix. Reentrancy-safe: reset executor → sync movement → start new Attack if applicable.

## Reentrancy/Lifecycle
Capture → mutate → log → RequestMove/StopMove. Tick only while Attack active. EndPlay silent (align Movement EndPlay). No Attack multicast delegate in S24.

## Expected Implementation Files
- `GPUnitCommandComponent.h/.cpp` (+ optional `GPAttackTypes.h`)
- GP-S24 doc / AI log / Cursor report

NO: MovementComponent (preferred), MobileUnit, tags, Build.cs, GAS wiring, assets.

## Operator Validation Plan
A in-range Ready; B approach→Ready; C moving target reissue; D Ready→Approaching; E Move replace; F retarget; G invalid/same-team/self; H/I target destroyed; J QueueDeferred; K remote; L multi-unit; M EndPlay.

## Scope Verification
- C++ changed: **NO**
- Build.cs changed: **NO**
- assets/maps/config changed: **NO**
- damage/health added: **NO**
- Attack implemented: **NO**
- Nav/pathfinding added: **NO**
- queue added: **NO**

## Git State
- Branch: `feature/gp-s24-attack-execution-analysis`
- Docs-only commit; working tree clean after push
- HEAD = origin
- no merge to main

## Implementation Pending
Explicit implementation task required. Target stage close: `DONE_WITH_DAMAGE_DEFERRED`.
