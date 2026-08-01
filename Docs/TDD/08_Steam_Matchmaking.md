# Steam Matchmaking

## Goal

2-player PvP match через Steam, listen server. Host створює session, client приєднується.

## Engine Stack

- `OnlineSubsystem` (engine plugin).
- `OnlineSubsystemSteam` (engine plugin).
- `OnlineSubsystemUtils`.

Enabled у `.uproject` `Plugins` array.

## Configuration

`Config/DefaultEngine.ini`:

```ini
[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480                        ; placeholder dev AppID (Spacewar); replace with real GP AppID
GameServerQueryPort=27015

[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="OnlineSubsystemSteam.SteamNetConnection"

[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
```

Real AppID — отримується після Steamworks registration. До того — Spacewar dev AppID для testing.

## Session Flow

```
Host:
   1. Main Menu -> "Host Game"
   2. Call IOnlineSession::CreateSession with FOnlineSessionSettings:
      - NumPublicConnections = 2
      - bIsLANMatch = false
      - bShouldAdvertise = true
      - bUsesPresence = true
      - bAllowJoinViaPresence = true
   3. On CreateSession success: ServerTravel("/Game/GrimProtocol/Maps/MAP_GP_MatchDefault?listen")
   4. AGP_GameMode initializes, AGP_GameState transitions to WaitingForPlayers

Client:
   1. Main Menu -> "Join Friend"
   2. Steam friend list overlay (or in-game Steam invite handler)
   3. On invite accept: IOnlineSession::JoinSession
   4. On JoinSession success: GetResolvedConnectString -> ClientTravel
   5. PlayerController spawns on host's server, GameMode handles join
```

### Why Listen Server (MVP)

- Дешево (no dedicated infra).
- Працює для 2-player target audience.
- Steam OSS handles NAT traversal / relay.
- Архітектура (server-authoritative) дозволяє future dedicated migration без gameplay code changes.

### Risks

- Host advantage (no latency to server) — acceptable у 2-player asymmetric multiplayer.
- Host disconnect terminates match — explicit UX expected behavior.

## Session Object Owner

Окремий subsystem або службовий контейнер:

`UGP_SessionSubsystem : UGameInstanceSubsystem` (у `GPRuntime/Public/Session/`):

- Bind delegates для CreateSession, JoinSession, FindSessions, DestroySession.
- Provide BP-friendly API для Main Menu UI.
- Centralized error handling.

```cpp
UCLASS()
class GPRUNTIME_API UGP_SessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Called from Main Menu
    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void HostMatch(FName MapName);

    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void JoinMatch(const FOnlineSessionSearchResult& Result);

    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void DestroyCurrentSession();

    // Delegates BP can listen to
    UPROPERTY(BlueprintAssignable)
    FOnSessionCreated OnSessionCreated;

    UPROPERTY(BlueprintAssignable)
    FOnSessionJoined OnSessionJoined;
    // ...
};
```

**Note:** SessionSubsystem — це **єдиний** виправданий subsystem у MVP (з ADR-обґрунтуванням), бо session lifecycle — game-instance scoped, ширше за будь-який single actor.

## Steam Invite Handling

Steam overlay → friend invite → game launch arg `+connect_lobby <id>`.

```cpp
// In UGameInstance::Init or UGP_SessionSubsystem::Initialize
const FString Cmd = FCommandLine::Get();
if (Cmd.Contains("+connect_lobby"))
{
    // Parse lobby ID, call FindSessionById -> JoinSession
}
```

Alternative: `OnSessionUserInviteAccepted` delegate на `IOnlineSessionPtr`.

## Map Travel

- ServerTravel: `/Game/GrimProtocol/Maps/MAP_GP_MatchDefault?listen` (host starts listen server).
- ClientTravel: standard через JoinSession's resolved connect string.
- Loading screen — TBD UI (поза critical MVP scope, але рекомендується для UX).

## Disconnect Handling

Engine-level:
- `UEngine::HandleNetworkFailure`, `HandleTravelFailure`.
- `UGP_SessionSubsystem` listens, routes to UI з error message.
- On host disconnect → all clients return to main menu з reason.

Game-level:
- `AGP_GameMode::Logout` → mark player lost → check win condition.

## Testing Matrix

| Test | Setup | Pass criteria |
| --- | --- | --- |
| Host creates lobby | Solo dev | Session visible у Steam, no errors у logs |
| Client joins via Steam invite | 2 devs | Both controllers spawn, GameState says Playing |
| Mid-match client disconnect | 2 devs | Server transitions to Finished, host as winner |
| Mid-match host disconnect | 2 devs | Client returns to menu with disconnect error |
| Travel from menu → map | Single | Travel success, no fatal logs, AGP_GameMode loads |

Testing з `OnlineSubsystemSteam.SteamDevAppId=480` (Spacewar) — usable для dev iteration.

## Out of MVP

- Dedicated server.
- 3+ player support.
- Mid-match reconnect.
- Lobby chat.
- Custom session search filters.
- Friend list direct invite UI (relying on Steam overlay).
- EOS support.

## Detailed Steam Matchmaking Rules (GP-0501)

Stage — design only (per [`Claude_Tasks/GP-0501_Steam_Matchmaking_MVP`](../Development/Claude_Tasks/GP-0501_Steam_Matchmaking_MVP.md)). Formalize state machine, ready/start gating, failure handling, same-map invariant.

### MVP Constraints (Hard)

- **2 players max** (`NumPublicConnections = 2`). 3+ player joins rejected.
- **Listen-server only.** Host is one of the players; його client runs server logic locally.
- **Single map.** `MAP_GP_MatchDefault` для всіх matches. No map selection UI.
- **Same-map invariant.** Host travels to map; client travels to same map. Server enforces — no client-side travel override.
- **Server-authoritative gameplay** unchanged. Authority model з TDD/03 застосовується.

### Session State Machine

```
                           ┌──────────────┐
                           │ MainMenu     │
                           └──────┬───────┘
                                  │ (Host) HostMatch()
                                  │ (Client) JoinMatch(searchResult)
                                  ▼
                           ┌──────────────┐
              ┌────────────│ Connecting   │────────────┐
              │            └──────────────┘            │
              │                                        │
   CreateSession success                  JoinSession success
              │                                        │
              ▼                                        ▼
      ┌──────────────┐                        ┌──────────────┐
      │ HostingLobby │                        │ ClientLobby  │
      └──────┬───────┘                        └──────┬───────┘
             │                                       │
             │ both ready ──┐               ┌── client ack ready
             │              │               │
             │       ┌──────▼───────────────▼──────┐
             │       │ LoadingMatch (ServerTravel) │
             │       └──────────────┬──────────────┘
             │                      │
             │       AGP_GameMode initializes
             │       PlayerControllers possess CameraPawn
             │                      │
             │                      ▼
             │            ┌──────────────┐
             └───────────►│ Playing      │
                          └──────┬───────┘
                                 │
                                 │ Match end (timer / MainBase scenario)
                                 ▼
                          ┌──────────────┐
                          │ Finished     │
                          └──────┬───────┘
                                 │ Acknowledge / Return
                                 ▼
                       (Destroy session, return MainMenu)
```

State held у `UGP_SessionSubsystem` як `EGP_SessionState` enum (Replicated через separate AGP_LobbyState actor у Lobby state OR via existing AGP_GameState за map travel).

### Host Flow (Step-by-step)

1. Main Menu: button **Host Game** → `UGP_SessionSubsystem::HostMatch(MapName=MAP_GP_MatchDefault)`.
2. `CreateSession` з `FOnlineSessionSettings`:
   - `NumPublicConnections = 2`
   - `bIsLANMatch = false`
   - `bShouldAdvertise = true`
   - `bUsesPresence = true`
   - `bAllowJoinViaPresence = true`
   - `bAllowJoinInProgress = false` (no late join у MVP)
   - `bUseLobbiesIfAvailable = true` (Steam Lobby API)
3. On `OnCreateSessionComplete` success:
   - SessionState → `HostingLobby`.
   - Broadcast `OnSessionCreated` (BP-bindable, UI swap to Lobby screen).
4. Lobby UI (`WBP_GP_Lobby`, Activatable per Common UI + MVVM):
   - VM: `UGP_LobbyVM.{PlayerList[], LocalReady, OpponentReady, MapName, CanStart}`.
   - Adapter listens to `UGP_SessionSubsystem` events.
   - Host sees self у player list immediately; client appears on `OnPlayerJoinedLobby`.
5. Both players toggle **Ready** (button → server RPC `Server_SetReady(bool)`).
6. When `LocalReady && OpponentReady` → host gets **Start Match** button enabled.
7. Host clicks Start → `Server_StartMatch()` RPC → server validates both ready:
   - SessionState → `LoadingMatch`.
   - Disable `bShouldAdvertise` (session closes to new joiners).
   - `ServerTravel("/Game/GrimProtocol/Maps/MAP_GP_MatchDefault?listen")`.
8. Map loads → `AGP_GameMode::PostLogin` ensures both players present.
9. `AGP_GameState::MatchState` → `GP.Match.State.Playing`.
10. Gameplay starts per existing TDD.

### Client Flow (Step-by-step)

1. Main Menu: 3 entry points:
   - **A. Find Match:** search-driven (`FindSessions` з presence filter).
   - **B. Join Friend:** Steam friend overlay → invite click.
   - **C. Command-line:** `+connect_lobby <id>` (Steam launch arg).
2. All routes resolve до `UGP_SessionSubsystem::JoinMatch(FOnlineSessionSearchResult)`.
3. `JoinSession(NAME_GameSession, Result)` → `OnJoinSessionComplete`:
   - On success → `GetResolvedConnectString` → `ClientTravel(ConnectString, TRAVEL_Absolute)`.
   - On fail → error path (див. Failure Matrix).
4. Client connects → `AGP_GameMode::PreLogin` / `PostLogin` on host approves.
5. PlayerController spawns on host server; client UI receives state via existing replication.
6. SessionState на client → `ClientLobby` (читає AGP_LobbyState replicated).
7. Client toggles Ready → `Server_SetReady(true)`.
8. Wait for host Start click.
9. On server travel → client travels automatically via engine handshake.
10. Same state transitions as host для решти gameplay.

### Ready / Start Conditions

| Condition | Required | Source |
| --- | --- | --- |
| `PlayerCount == 2` | True | Session player count |
| `LocalReady == true` (host) | True | `Server_SetReady` |
| `OpponentReady == true` (client) | True | Same RPC, replicated |
| `MatchState == LobbyOpen` | True | AGP_LobbyState |
| Host clicks Start | True | Only host has Start button enabled |

Server validates всі p`re-`travel; будь-який fail → log warning, ignore RPC.

Client cannot trigger Start. Only host. UX: client's Start button disabled / hidden.

### Same-Map Rule

- Map name hardcoded у `UGP_SessionSubsystem::HostMatch` defaults (`MAP_GP_MatchDefault`).
- Client cannot override — engine travel handshake уforce-handle map name.
- No "map vote" UI у MVP. Future expansion → `FOnlineSessionSettings::Settings.Add("MAP", FOnlineSessionSetting(...))`.
- Validation: AGP_GameMode у `InitGame` asserts loaded map name matches expected.

### Failure Matrix

| Failure | Source | UX |
| --- | --- | --- |
| `CreateSession` fails (Steam offline, no Internet) | `OnCreateSessionComplete(bSuccess=false)` | Toast "Failed to host session", return to MainMenu. |
| Host AppID mismatch | Steam handshake reject | Error dialog "Steam version mismatch", MainMenu. |
| `JoinSession` fails (session full / closed) | `OnJoinSessionComplete(EOnJoinSessionCompleteResult)` | Toast specific to result (Full / SessionDoesNotExist / AlreadyInSession / UnknownError). |
| Client connection timeout | `UEngine::HandleNetworkFailure(ConnectionLost)` | Toast "Connection lost", destroy local session, MainMenu. |
| Host disconnects mid-lobby | `UEngine::HandleNetworkFailure` on client | Toast "Host left", client destroys session, MainMenu. |
| Client disconnects mid-lobby | `AGP_LobbyState::OnPlayerLeft` | Host sees "Opponent left lobby" toast; lobby remains open (waiting for new client). |
| Host disconnects mid-match | `AGP_GameMode::Logout` (host instance ends) | All clients return to MainMenu з error "Host left match". Session destroyed. |
| Client disconnects mid-match | `AGP_GameMode::Logout(client)` | Server: log opponent left; can either:<br>**(a)** auto-win for host (per validation criterion) — recommended<br>**(b)** continue solo (score race vs SWARM) — out of scope. Recommend (a). |
| Server travel fails (map load error) | `HandleTravelFailure` | Both players return to MainMenu з error; session destroyed. |
| Steam overlay invite while in-match | `OnSessionUserInviteAccepted` | Ignored — log info "invite during match, deferred". |
| `DestroyCurrentSession` fails | `OnDestroySessionComplete(false)` | Log warning; force-clear local cache; MainMenu navigation proceeds. |
| Player joins while lobby `bAllowJoinInProgress=false` and match started | Engine reject | Steam shows "session full". |
| AppID = Spacewar dev ID у shipping build | Dev check | Build-time `#if !UE_BUILD_SHIPPING` assert; CI gate. |

### Connection Quality (MVP)

- Listen-server: host has 0 ms; client has natural Steam relay/direct latency.
- No latency compensation у MVP. RTS commands tolerate 50-150 ms RTT.
- Net update frequency для replicated unit movement: ~30 Hz (engine default per `AActor::NetUpdateFrequency`).
- High-frequency cosmetic events (multicast unreliable) drop under packet loss — acceptable.

### Subsystem API Surface (Confirms TDD/08)

```cpp
UCLASS()
class GPRUNTIME_API UGP_SessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void HostMatch();   // hardcoded map per MVP

    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void StartMatchAsHost();

    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void FindMatches(FGP_SessionSearchParams Params, FOnSessionSearchComplete Callback);

    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void JoinMatch(const FOnlineSessionSearchResult& Result);

    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void SetLocalReady(bool bReady);

    UFUNCTION(BlueprintCallable, Category = "GP|Session")
    void DestroyCurrentSession();

    UPROPERTY(BlueprintAssignable)  FGP_OnSessionCreated   OnSessionCreated;
    UPROPERTY(BlueprintAssignable)  FGP_OnSessionJoined    OnSessionJoined;
    UPROPERTY(BlueprintAssignable)  FGP_OnSessionFailed    OnSessionFailed;  // FString reason
    UPROPERTY(BlueprintAssignable)  FGP_OnLobbyUpdated     OnLobbyUpdated;
    UPROPERTY(BlueprintAssignable)  FGP_OnMatchStarting    OnMatchStarting;
};
```

Single subsystem per ADR-0006 (no extra subsystem proliferation).

### Lobby Replication

`AGP_LobbyState : AInfo` (spawned by GameMode before ServerTravel):

```cpp
USTRUCT()
struct FGP_LobbyPlayer
{
    GENERATED_BODY()
    UPROPERTY()  FString  SteamDisplayName;
    UPROPERTY()  uint64   SteamId = 0;
    UPROPERTY()  int32    TeamId  = 0;
    UPROPERTY()  bool     bReady  = false;
};

UCLASS()
class GPRUNTIME_API AGP_LobbyState : public AInfo
{
    GENERATED_BODY()
public:
    UPROPERTY(ReplicatedUsing=OnRep_Players)  TArray<FGP_LobbyPlayer> Players;

    UPROPERTY(Replicated)  bool bAllReady = false;

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetReady(bool bReady);
};
```

Lobby UI binds через `UGP_LobbyVM` (per Common UI + MVVM rule).

### Anti-Patterns (Review-Blocking)

- ❌ Client modifying `bShouldAdvertise` або session settings.
- ❌ Client calling `ServerTravel` (only host has authority).
- ❌ Map name read from client-supplied URL.
- ❌ Skipping `OnDestroySessionComplete` callback (orphans session).
- ❌ Using `FOnlineSessionSearchResult.Session.OwningUserId` для auth — use Steam ticket validation post-MVP.
- ❌ Multicast lobby state mutation — Players list updates через replication only.

### Validation Checklist (Stop Condition)

- [x] 2 players only — `NumPublicConnections=2`, engine reject 3-rd.
- [x] Host/client listen server — `?listen` URL parameter, no dedicated path.
- [x] Server-authoritative unchanged — gameplay TDDs intact, listen-server is host's local server instance.
- [x] Host/client failure paths documented — failure matrix above.
- [x] Same-map rule — host hardcodes, client cannot override.

### Open Questions

1. **Client mid-match leave behavior:** auto-win host (recommended) vs continue solo. Recommend auto-win + EndMatch with `Reason=OpponentLeft`.
2. **Lobby chat:** out of MVP confirmed; defer to post-MVP UX pass.
3. **Steam invite during match:** ignore vs queue. Recommend ignore + log.
4. **Loading screen UI:** TBD у visual pass; functional spec — show "Loading map…" splash during ServerTravel.
5. **Region routing:** Steam handles automatically? Or set `RegionFilter` для presence? Defer to playtest з real Steam AppID.
6. **Match history persistence:** out of MVP; no backend.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Host create lobby | Session visible у Steam friends list, lobby UI shows host. |
| 2 | Client invite-accept | Joins lobby, both player entries visible. |
| 3 | Both ready | Both Ready buttons toggled true → host Start button enables. |
| 4 | Host starts | ServerTravel completes, both reach Playing state. |
| 5 | Match plays out | 10-min timer, score race, end-of-match screen. |
| 6 | Host disconnect mid-lobby | Client returns to MainMenu з "Host left". |
| 7 | Host disconnect mid-match | Client returns to MainMenu з error. |
| 8 | Client disconnect mid-match | Host wins immediately, EndOfMatch reason="OpponentLeft". |
| 9 | Connection failure on join | Toast specific to result; client returns to MainMenu cleanly. |
| 10 | Try join full session | "Session full" error. |
| 11 | Same-map invariant | Editor / config has only one map; impossible to load other. |
| 12 | Travel failure | Mock travel fail → both clients return MainMenu з message. |
| 13 | Session destroy on quit | Quit from MainMenu → session destroyed; no orphan у Steam. |
| 14 | Mid-lobby new client join | While lobby open, 3-rd attempt → reject ("Session full"). |
| 15 | Steam offline | Host attempt → "Failed to host" toast. |

### Out of MVP

- Dedicated server.
- Custom search filters (skill, region, mode).
- Map selection / map pool.
- Faction selection (single faction MVP).
- 3+ player support.
- Mid-match reconnect.
- Lobby chat / voice.
- Spectator slots.
- Match replay export.
- Steam achievements integration.
- Steam Workshop integration.
- EOS, PlayStation Network, Xbox Live integration.

## References

- Multiplayer authority — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).
- Match state flow — [`../GDD/07_Match_Flow`](../GDD/07_Match_Flow.md).
- Subsystem rationale — [`../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering`](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md).
- Lobby UI VM binding — [`12_UI_Architecture`](12_UI_Architecture.md).
- Steam Matchmaking task — [`../Development/Claude_Tasks/GP-0501_Steam_Matchmaking_MVP`](../Development/Claude_Tasks/GP-0501_Steam_Matchmaking_MVP.md).
