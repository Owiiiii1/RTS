# Multiplayer Architecture

## Topology

- **Listen server** (host hosts і грає одночасно). MVP-only model.
- 2 players (host + 1 client) через Steam OSS.
- Dedicated server — поза MVP. Architecture не блокує future dedicated migration: жодного "client is also authoritative" pattern.

## Authority Model

| Concern | Authority | Where |
| --- | --- | --- |
| Match state (`Loading → Playing → Finished`) | Server | `AGP_GameMode` writes, `AGP_GameState` replicates |
| `MatchTimeRemaining` countdown (10-min hard cap) | Server | `AGP_GameMode` 1 Hz `FTimerManager` (not Actor Tick), replicated via `AGP_GameState` |
| Player faction / team | Server (assigned on join) | `AGP_PlayerState`, replicated |
| Player OrbitalFerronite (spendable currency) | Server | `AGP_PlayerState.ASC` + `UGP_PlayerAttributeSet.OrbitalFerronite`, `COND_OwnerOnly` |
| Player FerroniteScore (cumulative) | Server | `AGP_PlayerState.ASC` + `UGP_PlayerAttributeSet.FerroniteScore`, monotonically increasing, RepNotify |
| `FerroniteThreatValue` (swarm pressure) | Server | `AGP_GameState.FerroniteThreatValue` — raw Ferronite stored at MainBase (up on container drop-off, down on launch), replicated. Drives wave intensity. Deprecates `SwarmAggressionLevel` / `AggressionPerUnit*` |
| SWARM unit spawn / AI tick | Server | `AGP_GameMode` тригерить waves; SWARM units — server-only AIControlled. Multicast тільки cosmetic (death VFX) |
| AI opponent decision tick (singleplayer) | Server (host) | `AGP_AIController : AAIController` low-frequency (2-5s), не client-side |
| Score tie-break execution | Server | `AGP_GameMode` evaluates while `Playing`, then `FinishMatch` writes `FGP_MatchResult` and sets `Finished`. Ladder: `FerroniteScore` → `OrbitalFerronite` → `CurrentUnits` → stored `MatchSeed` (no RNG at finish). |
| `MatchResult` struct write | Server | `AGP_GameState.MatchResult` (`TArray<FGP_MatchTeamScore>` snapshot, not `TMap`) plus compatibility `WinnerTeamId` / `WinReasonTag`. |
| Disconnect detection | Server | `AGP_PlayerState.bConnected = false`, replicated |
| Unit/building spawn | Server | `AGP_GameMode` / GAS abilities |
| Unit health | Server | `AGP_UnitBase.ASC` + `UGP_UnitAttributeSet` |
| Unit movement (location) | Server (authoritative position); client interpolates | `UCharacterMovementComponent` standard replication |
| Selection | Client-local | `UGP_SelectionComponent` |
| Camera | Client-local | `AGP_CameraPawn` |
| Command intent (human) | Client → Server | `Server_RequestCommand` RPC (human PC only). AI bypasses this — calls shared server-side command-execution helpers directly |
| Orbital drop intent (human) | Client → Server | `Server_RequestOrbitalDrop` RPC (replaces removed build/produce RPCs). AI calls the same server-side helper directly |
| Command result | Server → all (replicated state) | Unit attribute changes, tags, position |
| Cosmetic VFX/SFX | Server multicast (rare) або client predicted | Per-event design |

Жодного "client-side authoritative gameplay drift". Якщо client рахує gameplay — це bug.

### Score / Currency Replication Notes

- `OrbitalFerronite` — spendable currency attribute on `UGP_PlayerAttributeSet`. **`COND_OwnerOnly`** — private funds; opponent must not see your spendable pool (per [ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md) Hard Rule 6). Mutated only via `GE_GP_AddOrbital` (container launch) / `GE_GP_SpendOrbital` (drop order).
- `FerroniteScore` — cumulative shipped victory score on `UGP_PlayerAttributeSet`. **`COND_None`** (replicated to all clients) — both players see the score race per GDD/08. Monotonically increasing. Replication mode — `Mixed` (per ASC replication policy у GAS_Architecture).
- Score increment виключно через server-applied `UGameplayEffect` at container launch. Client ніколи не пише.
- RepNotify викликає HUD update + `+N` flash animation.

### SWARM Authority Notes

- SWARM units спавняться через `AGP_GameMode::SpawnSwarmWave` server-only.
- Кожен SWARM unit — `AGP_UnitBase` child з `GP.Faction.Swarm` tag.
- SWARM AI — server-only `AAIController` subclass з простим targeting (closest player asset у aggro radius).
- SWARM **не приймає player commands**. Будь-яка спроба `Server_RequestCommand` з SWARM target як attacker — Validation reject.
- Multicast тільки на death VFX / attack montage (cosmetic).

### AI Opponent Authority Notes (Singleplayer)

- `AGP_AIController : AAIController` — server-only (per [`../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController`](../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md)). **Not** a `AGP_PlayerController` subclass — the earlier "or `AAIController` — TDD decision" ambiguity is resolved in favour of `AAIController`.
- AI owns a normal `AGP_PlayerState` with ASC + `UGP_PlayerAttributeSet` — symmetric to a human player (same `OrbitalFerronite`, same `FerroniteScore`, same FoW rules). No special "AI player state" class.
- Decision tick — low-frequency timer (2-5s), не frame tick.
- **AI does NOT use the client→server `Server_RequestCommand` RPC** (it is already on the server — an RPC round-trip would be wasteful). Instead AI invokes the **same server-side command-execution helpers** that `AGP_PlayerController::Server_RequestCommand_Implementation` calls *after* validation — i.e. the shared server-authoritative command layer (`UGP_CommandComponent::ExecuteServerCommand` / the orbital-order helper behind `Server_RequestOrbitalDrop`). Same validation and execution path, no simulated clicks.
- AI uses the **same orbital-delivery model** as players (orders drops via `UGP_OrbitalDeliverySubsystem`); no special AI path.
- `DA_GP_AIBehavior_Default` — Data Asset з thresholds (worker count target, roster size target, attack trigger threshold). Tunable без recompile.

## RPC Discipline

### Server RPCs (Client → Server)

Усі server RPCs:
- Called from `AGP_PlayerController` (owns connection).
- Format: `void Server_RequestX(...) WithValidation`.
- `Validate` перевіряє sanity (non-null pointers, parameter ranges).
- `Implementation` додатково перевіряє ownership і authority constraints.

Примір:

```cpp
UFUNCTION(Server, Reliable, WithValidation)
void Server_RequestCommand(FGP_CommandRequest Request);

bool AGP_PlayerController::Server_RequestCommand_Validate(FGP_CommandRequest Request)
{
    // Range / null sanity
    return Request.CommandTag.IsValid()
        && Request.Targets.Num() > 0
        && Request.Targets.Num() < 256;
}

void AGP_PlayerController::Server_RequestCommand_Implementation(FGP_CommandRequest Request)
{
    // Ownership check
    for (TWeakObjectPtr<AGP_UnitBase> Target : Request.Targets)
    {
        if (!Target.IsValid() || Target->GetTeamId() != GetPlayerState<AGP_PlayerState>()->GetTeamId())
        {
            UE_LOG(LogGPNet, Warning, TEXT("Rejected command: ownership mismatch"));
            return;
        }
    }
    // Dispatch into the shared server-authoritative command layer.
    UGP_CommandComponent* Cmd = FindComponentByClass<UGP_CommandComponent>();
    Cmd->ExecuteServerCommand(Request);
}
```

**Shared server-side command layer.** `UGP_CommandComponent::ExecuteServerCommand` (and the orbital-order helper behind `Server_RequestOrbitalDrop`) is the single execution entry point on the server. The human path reaches it via the `Server_RequestCommand` / `Server_RequestOrbitalDrop` RPC after validation; `AGP_AIController` (per ADR-0008) calls these **same helpers directly** without any client RPC. There is exactly one authoritative execution path regardless of caller.

**Orbital model RPCs (post-pivot).** Per [ADR-0009 Orbital Delivery](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md), local-production RPCs `Server_BuildAt` / `Server_QueueProduction` / `Server_CancelProduction` / `Server_SetRallyPoint` are **removed**. The single replacement is `Server_RequestOrbitalDrop` (human PC), which validates ownership + `OrbitalFerronite` affordability + Active-Visibility FoW, then routes through `UGP_OrbitalDeliverySubsystem`.

### Client RPCs (Server → specific client)

- Use sparingly. Більшість targeted feedback — through replicated state (RepNotify on PlayerState attribute, etc.).
- Valid cases: end-of-match dialog open, command-rejected notification.

```cpp
UFUNCTION(Client, Reliable)
void Client_NotifyCommandRejected(FGameplayTag Reason);
```

### Multicast RPCs (Server → all)

- **Cosmetic only.** Death VFX, attack montage trigger, building explosion.
- Gameplay state — replicated property, не multicast.
- Multicast — `Unreliable` за замовчуванням (cosmetic не critical).

```cpp
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayDeathVFX();
```

## Replication Strategy

### Properties

- `UPROPERTY(Replicated)` для primitive values.
- `UPROPERTY(ReplicatedUsing=OnRep_X)` для state, що тригерить client side reactions (UI updates, particle spawn).
- `DOREPLIFETIME_*` блок у `GetLifetimeReplicatedProps`. `DOREPLIFETIME_CONDITION(Class, Field, COND_*)` для conditional (e.g., `COND_OwnerOnly`).

### GAS

- AttributeSet replication через `UAbilitySystemComponent::SetReplicationMode`.
- Active gameplay effects — auto-replicated by GAS у `Mixed`/`Full` modes.
- Loose gameplay tags — server-set, propagated to clients via ASC replication.

### Net Relevancy

- `bAlwaysRelevant = true` для PlayerState, GameState, GameMode (GameMode — server-only anyway).
- Unit і building actors — default relevancy (within frustum + dormancy).
- Map size MVP — small (single screen / two-screen camera span). Relevancy не bottleneck у MVP.

### Dormancy

- Buildings — `DORM_DormantAll` після drop-pod landing (asset operational immediately, no construction phase per orbital model), wake on damage / state change.
- Workers і Troopers — non-dormant (constantly moving / acting).

## Per-Player Loot / Per-Player State

Не релевантно у MVP RTS (не ARPG inventory). Усі resources — shared player attribute, не per-unit pickup.

## Disconnect Handling

Per [`../GDD/08_Win_Lose_Conditions`](../GDD/08_Win_Lose_Conditions.md) і [`../GDD/First_Playable_Match`](../GDD/First_Playable_Match.md) — match-end model є score-based з 10-min hard cap, **не** annihilation-based. Disconnect handling узгоджений з цим.

- **Host disconnect:** session terminates → client receives disconnect → return to main menu з error. Match aborts, no winner.
- **Client disconnect:**
  - Server detects via `ENetworkFailure::ConnectionLost` / `PostLogin/Logout`.
  - Server marks player as lost: `AGP_PlayerState.bConnected = false`, replicated.
  - **Match продовжується до 10-min timer expiry** з remaining player. Frozen score disconnected player залишається у `FinalScores` для final comparison.
  - На timer expiry — remaining player виграє по score (зазвичай вищий, бо тільки він mineить), `WinnerReason = GP.Match.WinReason.OpponentDisconnect`.
  - Reconnect mid-match — **поза MVP**. Disconnected client returns to main menu з error.

Це **не** "immediate Finished з remaining as winner" — match все одно triggers Finished по timer, що зберігає consistent server-authoritative flow незалежно від disconnect timing.

## Network Profiles (for testing)

- LAN testing — useful for fast iteration, але не sufficient.
- Steam relay testing — final correctness check before release candidate.
- Simulated latency profiles у editor — `net pktlag` console commands для local testing (target 100-200ms RTT).

## Validation Discipline

**Every server-state change validates ownership і authority.** Common checks:

```cpp
ensure(GetWorld()->IsServer() || HasAuthority());
```

— at start of every server-side function that mutates state. Якщо `ensure` fails — це bug, не expected runtime path.

## Out of MVP

- Dedicated server.
- Mid-match join.
- Spectator slots.
- Rollback / netcode prediction (RTS не потребує fighting-game-style rollback).
- Anti-cheat integration (Steam EAC / VAC) — deferred decision.

## References

- Steam matchmaking flow — [`08_Steam_Matchmaking`](08_Steam_Matchmaking.md).
- Command pipeline — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md).
- GAS replication — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- ADR — [`../Architecture_Decisions/ADR_0004_Multiplayer_First`](../Architecture_Decisions/ADR_0004_Multiplayer_First.md).
- Score model і tie-break — [`../GDD/08_Win_Lose_Conditions`](../GDD/08_Win_Lose_Conditions.md).
- Match flow і timer — [`../GDD/07_Match_Flow`](../GDD/07_Match_Flow.md).
- SWARM faction і AI opponent scope — [`../GDD/03_Factions`](../GDD/03_Factions.md).
- End-to-end player story — [`../GDD/First_Playable_Match`](../GDD/First_Playable_Match.md).
