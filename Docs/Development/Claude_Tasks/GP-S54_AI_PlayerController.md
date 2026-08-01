# GP-S54 AI PlayerController

Note: class renamed AGP_AIPlayerController → AGP_AIController (per ADR_0008; it derives from AAIController, not APlayerController). Filename retained for cursor stability.

## Slice Group
Slice 10 — AI Opponent

## Code Allowed
Yes — after approval of Slice 9 (UI Foundation + FoW) AND `GP-0306` design.

## Depends On
- GP-S53 (UI Foundation closed).
- GP-0306 design approved.

## Goal
Implement `AGP_AIController : AAIController` як core опонент for singleplayer mode. Per [ADR-0008](../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md).

## Scope

- Create `AGP_AIController` (server-only, inherits `AAIController`).
- Skeletal state machine: `EGP_AIState` enum + transition function (returns next state; не виконує actions yet).
- Per-state stubs: `Tick_Explore`, `Tick_Mine`, `Tick_Ship`, `Tick_Order`, `Tick_Defend` — log-only stubs (Verbose), no actual gameplay calls.
- `OnPossess` override — посаджує AI на dummy `AGP_AIPawn` (або no pawn — AI does не require viewport).
- Decision tick — gated by `BehaviorDef.DecisionInterval` (read from `CachedBehaviorDef` after async load).
- `BehaviorRef` як `TSoftObjectPtr<UGP_AIBehaviorDefinition>` — async load on `OnPossess`, set `CachedBehaviorDef`.
- AI ownership: receives own `AGP_PlayerState` (server-spawned by GameMode у singleplayer mode).
- TeamId = 2 (opponent у 1v1 vs human team 1).

## Out of Scope

- Actual state action implementations (GP-S56).
- DataAsset class definition (GP-S55).
- BehaviorTree migration (post-MVP).
- Multiple difficulty tiers.
- AI for PvP multiplayer ("AI fillers" — not in MVP).

## Required Skill Pass

- `ue5-architecture`
- `gp-mechanics-validator` (AI as gameplay mechanic).

## Files Touched

- `GP/Source/GPRuntime/Public/Player/GPAIPlayerController.h` — new
- `GP/Source/GPRuntime/Private/Player/GPAIPlayerController.cpp` — new
- `GP/Source/GPRuntime/Public/Player/GPAIState.h` — new (`EGP_AIState` enum)
- `GP/Source/GPRuntime/Private/Core/GPGameMode.cpp` — modify to spawn AI PC у singleplayer mode

## Acceptance Criteria

- [ ] Compiles clean.
- [ ] `AGP_AIController` selectable у GameMode `AIControllerClass`.
- [ ] Singleplayer launch — GameMode spawns 1 human PC + 1 AI PC. Both have valid `AGP_PlayerState` with ASC.
- [ ] AI Tick logs current state at Verbose; decision interval respects DA value.
- [ ] State transitions log when state changes.
- [ ] No replication overhead (AI PC `bReplicates=false` if engine allows; otherwise minimal).
- [ ] No `Server_RequestCommand` RPC у AI code path (AI invokes server-side helpers directly per ADR-0008).
- [ ] No widget creation у AI PC (no UI for AI).
- [ ] BehaviorRef async loading respects soft-ref rule (ADR-0002).
- [ ] AIModule dependency declared у `GPRuntime.Build.cs`.

## Playtest / Validation Note

Launch singleplayer match. Verify Output Log:
- "AI possessed pawn"
- "AI state: Explore" (initial)
- After 3 s — "AI decision tick"
- After N decisions — state may change based on stub logic (random/fixed для тести).

Server CPU usage from AI tick — flat (no spike per decision).

## Risks / Edge Cases

- AI state machine stuck on one state (bug у evaluator) — mitigated by per-state minimum-duration timer + tests.
- AI PC unintentionally replicated to human client (creates duplicate-PC bug). Test з `ShowDebug` chains.
- AI possession of pawn but no pawn class assigned — null deref guard у OnPossess.
- Map travel from singleplayer to multiplayer — AI PC must be destroyed properly у `EndPlay`.

## Linked

- [ADR-0008 AI Opponent AAIController](../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md)
- [GP-0306 AI Opponent design](GP-0306_AI_Opponent.md)
- `TDD/13_Architecture_Proposal` §Slice 10.
- `TDD/03_Multiplayer_Architecture` §AI Opponent Authority Notes.

## Stop Condition
STOP. Await approval before GP-S55.
