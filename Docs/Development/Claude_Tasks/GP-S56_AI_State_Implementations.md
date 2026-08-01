# GP-S56 AI State Implementations

## Slice Group
Slice 10 — AI Opponent

## Code Allowed
Yes — after GP-S55 approval.

## Depends On
- GP-S54 (AGP_AIController scaffold).
- GP-S55 (UGP_AIBehaviorDefinition DA).
- Slices 5, 6, 7, 8 (gameplay APIs that AI invokes).

## Goal
Implement per-state action logic: `Explore`, `Mine`, `Ship`, `Order`, `Defend`. AI invokes same gameplay APIs as human player (per [ADR-0008](../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md)).

## Scope

### EvaluateNextState

Implement priority-ladder per GP-0306 design:

```cpp
EGP_AIState AGP_AIController::EvaluateNextState() const
{
    const auto& B = *CachedBehavior;
    const auto* PS = GetOwningPlayerState();

    // 1. Defense triggers
    if (HealthPctOf(MainBase) < B.BaseHealthDefenseThreshold) return EGP_AIState::Defend;
    if (GameState->FerroniteThreatValue >= B.DefenseThreshold) return EGP_AIState::Defend;
    if (IsEnemyVisibleNearBase(B.EnemyBaseRadiusForDefend)) return EGP_AIState::Defend;

    // 2. Order condition
    if (PS->OrbitalFerronite >= CheapestAffordableDropCost() && AnyTargetMissing(B))
        return EGP_AIState::Order;

    // 3. Mine condition
    if (HasIdleWorker() && HasAvailableDeposit()) return EGP_AIState::Mine;

    // 4. Explore default
    return EGP_AIState::Explore;
}
```

### Per-State Actions

#### Tick_Mine
```
For each idle Worker:
    Deposit = FindClosestNonDepletedDeposit(Worker, MaxRadius=10000cm)
    If found: IssueCommand(Worker, GP.Command.Mine, Deposit) via ServerExecuteCommand
    Else: Worker remains idle (will trigger Explore next tick)
```

#### Tick_Explore
```
For each idle Worker / SalvageWalker:
    UnexploredCells = LocalFoW.GetClosestUnexploredCells(Worker.Location, max=5)
    If found: target = first cell within current Visible reach
              IssueCommand(Worker, GP.Command.Move, cell.world)
    Else: idle (map fully explored)
```

#### Tick_Order
```
DropType = SelectPriorityDropType(B):
    If CurrentWorkers < B.TargetWorkerCount → PreferredWorkerDrop
    Else if CombatUnits < B.TargetCombatRoster → PreferredCombatDrop
    Else if Turrets < B.TargetTurretCount → PreferredTurretDrop
    Else if ContainersOverflowFrequent → PreferredLogisticsHubDrop
    Else: skip

If DropType set:
    DropPoint = FindDropPoint(DropType, B):
        Worker drop: random Visible cell within WorkerDropRadiusFromBase of MainBase
        Combat: same with CombatDropRadiusFromBase
        Turret: choose cell from TurretDropPreferredChokeZones OR FoW-edge analysis
        LogisticsHub: visible cell near MainBase, clearance OK
    OrbitalDeliverySubsystem.TryEnqueueOrder(AI_PlayerState, DropType, DropPoint, ...)
```

#### Tick_Defend
```
SalvageWalkers = OwnUnits.WhereTag(GP.Unit.Type.Combat)
For each Walker: IssueCommand(Walker, GP.Command.Move, MainBase.Location)
If FerroniteThreatValue very high AND OrbitalFerronite sufficient:
    Order Defensive Turret near MainBase choke
```

#### Tick_Ship
```
Passive. Containers auto-ship. AI suppresses Order during Ship state to let queue clear.
Optional: if all containers Ready AND none Launching → manual trigger (post-MVP feature, MVP auto).
```

### Min State Duration (Anti-Thrash)

```cpp
if (TimeSinceLastStateChange < CachedBehavior->MinStateDuration)
    return CurrentState;
```

### Server-Side Helpers

Refactor `UGP_CommandComponent::Server_RequestCommand_Implementation` → extract `ServerExecuteCommand` public static helper so AI може invoke without RPC. AI calls directly. Per ADR-0008.

`UGP_OrbitalDeliverySubsystem::TryEnqueueOrder` already takes `AGP_PlayerController*` — accept `AGP_AIController*` parameter polymorphically (use `AController*` or interface).

## Out of Scope

- BehaviorTree migration.
- Difficulty curves (multiple DA tiers).
- Wall drag-build AI (deferred — `bUsesWalls` flag remains false у MVP).
- AI vs AI matches.
- Replay determinism.

## Required Skill Pass

- `gp-mechanics-validator` (verify symmetric authority + Pillar 8 fit).
- `game-design-framework` (5-component check для each state action — Clarity / Motivation / Response / Satisfaction / Fit).

## Files Touched

- `GP/Source/GPRuntime/Private/Player/GPAIPlayerController.cpp` — implement EvaluateNextState + Tick_* methods.
- `GP/Source/GPRuntime/Public/Components/GPCommandComponent.h` — expose `ServerExecuteCommand` як static helper.
- `GP/Source/GPRuntime/Private/Components/GPCommandComponent.cpp` — refactor RPC → calls helper.
- `GP/Source/GPRuntime/Private/Subsystems/GPOrbitalDeliverySubsystem.cpp` — accept `AController*` (or PlayerState directly).
- (optional) `GP/Source/GPRuntime/Public/AI/GPAIHelpers.h` — utility namespace для FindDropPoint, IsEnemyVisibleNearBase, etc.

## Acceptance Criteria

- [ ] Compiles clean.
- [ ] AI plays through full 10-min singleplayer match without crashes.
- [ ] AI ships ≥ 1 container by 2:00 mark.
- [ ] AI orders ≥ 1 unit/structure by 3:00 mark.
- [ ] AI does not target hidden enemies (FoW filter respected).
- [ ] AI does not exceed `OrbitalFerronite` budget (no negative pool).
- [ ] AI cannot deploy outside Visible cells.
- [ ] AI Defends — combat units rally to base on SWARM trigger (visible у PIE).
- [ ] State machine transitions logged at Verbose.
- [ ] No spam — Verbose log < 5 lines/sec average.
- [ ] No "magic" — AI invokes same APIs as human (CommandComponent / OrbitalDeliverySubsystem).
- [ ] Min state duration prevents thrash (state doesn't flip > 2 times/sec).
- [ ] Pillar 8 5-question gate documented passed у PR description.

## Playtest / Validation Note

Full singleplayer match. Stand back, observe AI for 10 min. Subjective:
- Чи AI behaves like a "competitor" (visibly does economy, ships, attacks)?
- Чи player can win / lose against AI (perfect play loses не against MVP AI)?
- Чи AI feels organic vs scripted? (state changes timed to events, not random)

Server CPU profile: AI tick < 1 ms.

## Risks / Edge Cases

- AI tries to drop on un-FoW cell у race condition (FoW just shifted) — Validation rejects, AI re-evaluates next tick. No crash.
- AI runs out of map — Explore can't find unexplored cells → state stays Explore but Tick_Explore no-ops. Mitigated by fallback to Mine/Defend if no exploration possible.
- AI overspends на Defense → no economy → loses anyway. By design (player should exploit aggressive economy AI).
- AI fails to ship before Containers max — drop-off blocked, AI tries to spend, fails → idle. Mitigated by Ship state pause Orders.
- AI commits to combat unit deploy у moment economy collapses → AI continues mining attempts. State machine resilient via priority ladder evaluation per tick.

## Linked

- [ADR-0008 AI Opponent AAIController](../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md).
- [GP-0306 AI Opponent design](GP-0306_AI_Opponent.md).
- [GP-S54 AI PlayerController scaffold](GP-S54_AI_PlayerController.md).
- [GP-S55 AI Behavior DataAsset](GP-S55_AI_Behavior_Definition.md).
- [`../../TDD/13_Architecture_Proposal.md`](../../TDD/13_Architecture_Proposal.md) §Slice 10.
- [`../../TDD/14_Orbital_Delivery.md`](../../TDD/14_Orbital_Delivery.md) §UGP_OrbitalDeliverySubsystem (consumer API).

## Stop Condition
STOP. Slice 10 complete after merge. Await approval before Slice 11 (Feedback Pass).
