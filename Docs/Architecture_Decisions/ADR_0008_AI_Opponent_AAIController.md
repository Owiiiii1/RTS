# ADR-0008 — AI Opponent Implementation: AAIController

## Status
Accepted (2026-05-16)

## Context

AI opponent — mandatory MVP feature per owner directive 2026-05-16 ("AI Opponent - не Out Of Scope він повинен бути в ГРІ МВП хай не сильно розумний але повинен!"). Singleplayer mode requires opponent that:

- Runs same orbital-delivery flow as human player.
- Mines, ships containers, orders drops, defends.
- Operates server-side (singleplayer = listen-server with host as player).
- Uses primitive finite state machine (per `project_ai_opponent_in_mvp` memory). Not utility / goal-oriented / learning.
- Symmetric authority (no cheating — same `OrbitalFerronite`, same FoW, same rules).

Engine offers two paths для server-side AI:

1. **`APlayerController` subclass** — `AGP_AIController : AGP_PlayerController`. AI inherits all input pipeline (SelectionComponent, CommandComponent, etc.), вживає `Server_RequestCommand` як human player, simulates clicks.

2. **`AAIController` subclass** — `AGP_AIController : AAIController`. Native UE AI controller, designed для server-only NPC behavior. Standard for AI у UE.

## Decision

**Use `AAIController` subclass.**

`AGP_AIController : AAIController`.

## Rationale

| Aspect | APlayerController subclass | **AAIController subclass (chosen)** |
| --- | --- | --- |
| Engine native fit | Misuse — PC is for human input, not autonomous decisions | Designed exactly для server-side autonomous control |
| Input pipeline | Inherits all input infra (waste — AI doesn't use Enhanced Input / SelectionComponent / mouse) | Clean — no input infra |
| Memory footprint per AI | Heavy — PC has UI subobjects, MVVM adapters, HUD class, IMC subscriptions | Light — no UI / no input baggage |
| Replication | PC replicates to owning client (none for AI — wasted band) | AAIController not replicated to clients by default |
| BehaviorTree / Blackboard integration (future) | Awkward — not a natural fit | First-class — standard UE pattern |
| Possession semantics | "PlayerController possesses Pawn" implies player; confusing for AI | "AIController possesses Pawn" — standard semantic |
| Existing UE samples / community patterns | Rare for AI | Common, well-documented |
| AI tick scheduling | Tick on PC tick group (same as input) | Default — UE AI tick groups |

`AAIController` — engine-canonical choice. APlayerController subclass буде hacky workaround саме для перевикористання input pipeline, що для AI непотрібно.

## How AI Calls Same Actions as Human

AI does NOT use `Server_RequestCommand` RPC (it's already on server — no need for RPC round-trip). Замість цього:

- AI invokes the **same server-side command-execution helpers** that `AGP_PlayerController::Server_RequestCommand_Implementation` calls *after* validation (shared server-authoritative command layer) — no client RPC round-trip:
  - `UGP_CommandComponent::ExecuteServerCommand(FGP_CommandRequest&)` — refactored to allow non-PC callers (same entry point used by the human `Server_RequestCommand` RPC).
  - The orbital-order helper behind `Server_RequestOrbitalDrop` (routes through `UGP_OrbitalDeliverySubsystem`) — works with the AI's regular `AGP_PlayerState`.
- AI owns a regular `AGP_PlayerState` (with ASC + AttributeSet) — symmetric to human player. No special "AI player state" class.
- AI's `TeamId` set normally (TeamId=2 for opponent у 1v1 singleplayer).

This means AI shares **gameplay state model** with human player. Differs only at input layer (no Selection / Camera / UI).

## Class Outline

```cpp
UCLASS()
class GPRUNTIME_API AGP_AIController : public AAIController
{
    GENERATED_BODY()
public:
    AGP_AIController();

    virtual void OnPossess(APawn* InPawn) override;     // possess a dummy "AI camera" pawn for orientation, або no pawn
    virtual void Tick(float DeltaTime) override;        // gated by DecisionInterval

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GP|AI")
    TSoftObjectPtr<UGP_AIBehaviorDefinition> BehaviorRef;

    UPROPERTY(Transient)
    TObjectPtr<UGP_AIBehaviorDefinition> CachedBehavior;     // resolved after async load

    UPROPERTY()
    EGP_AIState CurrentState = EGP_AIState::Explore;

    float TimeSinceLastDecision = 0.f;

    void RunDecisionTick();                              // 2-5 s rate per BehaviorDef.DecisionInterval
    EGP_AIState EvaluateNextState() const;
    void ExecuteStateActions(EGP_AIState State);

    // Per-state action dispatchers — invoke same gameplay APIs as human
    void Tick_Explore();
    void Tick_Mine();
    void Tick_Ship();
    void Tick_Order();
    void Tick_Defend();
};

UENUM()
enum class EGP_AIState : uint8
{
    Explore,
    Mine,
    Ship,
    Order,
    Defend
};
```

DataAsset `UGP_AIBehaviorDefinition` (DA-driven thresholds, per project Data-Driven First):

```cpp
UCLASS()
class GPRUNTIME_API UGP_AIBehaviorDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)  float DecisionInterval = 3.0f;          // seconds
    UPROPERTY(EditAnywhere)  int32 TargetWorkerCount = 6;
    UPROPERTY(EditAnywhere)  int32 ShipUrgencyThreshold = 80;        // container fill % to prioritize ship
    UPROPERTY(EditAnywhere)  int32 DefenseThreshold = 50;            // SwarmAggression triggering Defend state
    UPROPERTY(EditAnywhere)  TSoftObjectPtr<UGP_OrbitalDropDefinition> PreferredCombatDrop;
    UPROPERTY(EditAnywhere)  TSoftObjectPtr<UGP_OrbitalDropDefinition> PreferredWorkerDrop;
    UPROPERTY(EditAnywhere)  TSoftObjectPtr<UGP_OrbitalDropDefinition> PreferredDefenseDrop;
    // ... more thresholds DA-driven, per balance pass
};
```

All asset refs soft per ADR-0002. Loaded via `UGP_MatchAssetLoader::PreloadForMatch`.

## Consequences

### Positive

- Engine-idiomatic AI integration.
- Light memory footprint per AI instance.
- Clean separation from human input infrastructure.
- BehaviorTree / Blackboard migration path open (post-MVP) without refactor.
- Symmetric gameplay model (AI owns standard `AGP_PlayerState` з ASC).
- No `bReplicates=true` overhead on AI controller (server-only).

### Negative

- Need to refactor `UGP_CommandComponent` to support non-PC callers (extract `ServerExecuteCommand` from RPC wrapper). One-time refactor cost; benefits server-side AI clarity.
- AI doesn't reuse Selection / Marquee / Camera logic — but those are human-input only by design, so no real reuse loss.
- Some duplication у "decide what to do" + "execute action" чи через PC pipeline reuse. Minor.

## Alternatives Considered

### `APlayerController` subclass

Reuses input pipeline. Sounds elegant ("AI plays like human"), але:

- Simulating clicks через `Server_RequestCommand` RPC round-trip on same machine — wasteful.
- Drags UI / VM / IMC infra что AI never uses.
- Confusing class hierarchy (`AGP_PlayerController` родовий для both human and AI).
- Performance penalty per AI instance.

Rejected.

### `AGameMode` тригерить AI logic directly (no controller)

GameMode acts як monolithic AI brain. Rejected:

- GameMode already responsible для match flow; mixing AI logic — single-responsibility violation.
- Doesn't scale to multiple AI opponents (future).
- Hard to test in isolation.

### Behavior Tree з blackboard у MVP

Over-engineered для primitive AI. `AAIController` з direct state-machine logic — simpler і matches Pillar 8 (Simple Core). BehaviorTree available as post-MVP upgrade path.

## Implementation Order

Per [`../TDD/13_Architecture_Proposal`](../TDD/13_Architecture_Proposal.md) Implementation Order — AI slice додається як **Slice 11A — AI Opponent** (or fold into Slice 11 Steam MVP). Triggered when foundational gameplay slices complete і MVP loop testable.

## References

- `project_ai_opponent_in_mvp` memory rule.
- `/CONTRIBUTING.md` — hard bans (single-responsibility, no manager hell).
- `Docs/GDD/03_Factions.md` — AI opponent gameplay spec.
- `Docs/GDD/First_Playable_Match.md` — singleplayer flow з AI.
- `Docs/TDD/03_Multiplayer_Architecture.md` §AI Opponent Authority Notes.
- `Docs/TDD/13_Architecture_Proposal.md` §GPRuntime — Match Flow class list.
- UE docs — `AAIController` reference.
