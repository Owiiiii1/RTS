# GP-S07 — AGP_GameMode (PostLogin, Match Countdown, EndMatch Hook)

## Slice Group
Slice 2 — Match Flow + Asset Loader

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S06 DONE — `AGP_GameState` authority API.
- Native tags `GP.Match.State.*` / `GP.Match.WinReason.*` (GP-S02).
- Module `GPRuntime`.

## Goal
Implement server-only `AGP_GameMode : AGameModeBase` that handles PostLogin start gating, owns the 1 Hz match countdown (`FTimerManager`), and provides a minimal FinishMatch / EvaluateAndFinishMatch hook writing results only through `AGP_GameState`.

No PlayerState, economy, units, AI Controller, LobbyState, or real TimerScore winner evaluation in this slice.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S08 until explicitly assigned (do not auto-materialize GP-S08 task file).

### Closed with
- `AGP_GameMode : AGameModeBase` — server-only match orchestration in `GPRuntime`.
- `GameStateClass = AGP_GameState`.
- BeginPlay → `WaitingForPlayers`; PostLogin → `TryStartMatch`; `ExpectedHumanPlayers` default **2**.
- Countdown via `FTimerManager` 1 Hz; **no Tick**; **no RPC**; **no replicated GameMode properties**.
- `FinishMatch` writes result through `AGP_GameState`; timeout → `EvaluateAndFinishMatch`.
- Timer-score evaluation intentionally deferred; timeout remains **Playing @ 0** until PlayerState/score integration.
- No assets / map / config wiring.
- Operator Editor/PIE validation **PASSED**.
- Listen-server replication proof **deferred** until temporary GameMode/map wiring.

---

## Tech-lead locks (OD-1…OD-7) — RESOLVED

### OD-1 — RESOLVED: Countdown ownership
- `AGP_GameMode` owns `FTimerHandle`.
- `FTimerManager` repeating timer, period **1.0** s.
- **No** actor Tick for countdown.
- `AGP_GameState` stores/replicates `MatchTimeRemaining` only.
- TDD/03 “GameMode::Tick low-frequency 1 Hz” = **stale implementation wording**; server authority remains canonical.

### OD-2 — RESOLVED: Start trigger
- BeginPlay does **NOT** automatically start Playing.
- BeginPlay initializes project match flow to `WaitingForPlayers`.
- PostLogin → Super, then `TryStartMatch()`.
- `TryStartMatch` is idempotent; no restart if already Playing or Finished.
- No LobbyState / ready system in GP-S07.

### OD-3 — RESOLVED: Expected player count
- `ExpectedHumanPlayers` — `EditDefaultsOnly`, `int32`.
- Canonical default = **2** (multiplayer MVP).
- Clamp `>= 1`.
- Count only valid human `APlayerController`s; **do not** count `AIController`.
- Singleplayer “1 human + AI” readiness deferred to AI/SP integration slice.
- Do **not** change production default to 1 for standalone PIE auto-start.
- Subclasses/config may override for controlled testing; GP-S07 default remains **2**.

### OD-4 — RESOLVED: Timeout before PlayerState/score (intentional integration gap)
- `HandleMatchTimeExpired` stops/clears countdown, then calls virtual `EvaluateAndFinishMatch()`.
- Base GP-S07 `EvaluateAndFinishMatch`:
  - does **NOT** choose a winner;
  - does **NOT** generate random/tie result;
  - does **NOT** call `FinishMatch(-1, TimerScore)`;
  - does **NOT** set `MatchStateTag` to Finished;
  - logs explicit **Error or Warning** that timer-score evaluation is unavailable until PlayerState/score integration.
- `MatchTimeRemaining` remains **0**.
- This is an **intentional integration gap**, not final production behavior.
- Later slice (with PlayerState / FerroniteScore) must implement timer score evaluation and call  
  `FinishMatch(WinnerTeamId, GP.Match.WinReason.TimerScore)`.
- **GP-S07 acceptance** validates the expiry **hook**, not complete timeout victory evaluation.

Rationale: GDD requires a deterministic timeout winner; inventing a fake result would violate GDD worse than leaving the flow explicitly incomplete.

### OD-5 — RESOLVED: Project flow only
- Do not use engine MatchState as a second project SoT.
- Do not call `AGameModeBase::StartMatch` / `EndMatch` as project state orchestration in this slice.
- Project state lives in `AGP_GameState::MatchStateTag`.

### OD-6 — RESOLVED: Initial state
- `AGP_GameState` constructor may begin at Loading.
- `AGP_GameMode::BeginPlay`, after validating `AGP_GameState`, sets `GP.Match.State.WaitingForPlayers`.
- BeginPlay does not set Playing.

### OD-7 — RESOLVED: Logout
- Override `Logout`, call Super.
- Log/update connected human count.
- Do **not** determine a winner; do **not** call `FinishMatch`.
- If match has not started, re-evaluate `TryStartMatch` only as appropriate (logout can never satisfy the threshold alone).
- Mid-match disconnect result deferred; **no** `OpponentDisconnect` in GP-S07.

---

## Exact lifecycle

### Constructor
- `GameStateClass = AGP_GameState::StaticClass()`
- Defaults: `MatchDurationSeconds = 600.0f`, `ExpectedHumanPlayers = 2`
- Tick disabled (`PrimaryActorTick.bCanEverTick = false` / equivalent)

### BeginPlay
1. `Super::BeginPlay`
2. Authority validation
3. Resolve `AGP_GameState*`
4. If unavailable / wrong class → log **Error**, stop match-flow init
5. `SetMatchStateTag(WaitingForPlayers)`
6. `SetMatchTimeRemaining(0)`
7. `ClearMatchResult()`
8. **No** automatic `StartMatchFlow`

### PostLogin
1. `Super::PostLogin(NewPlayer)`
2. Authority validation
3. `TryStartMatch()`

### TryStartMatch (idempotent)
1. Resolve GameState
2. No-op if MatchStateTag is Playing or Finished
3. Count valid human PlayerControllers (`>= ExpectedHumanPlayers`, clamp Expected `>= 1`)
4. If threshold met → `StartMatchFlow()`

### StartMatchFlow (authority only, idempotent)
1. `StopMatchCountdown()` first
2. `ClearMatchResult()`
3. `SetMatchStateTag(Playing)`
4. `SetMatchTimeRemaining(max(0, MatchDurationSeconds))`
5. If duration `> 0` → start 1 Hz timer → `HandleMatchCountdownTick`
6. If duration `== 0` → `HandleMatchTimeExpired()` without starting a timer
7. Call virtual `OnMatchFlowStarted()` if retained

### HandleMatchCountdownTick
1. Authority only; require Playing
2. `NewRemaining = max(0, current - 1.0f)`
3. `GameState->SetMatchTimeRemaining(NewRemaining)`
4. If `<= 0` → `HandleMatchTimeExpired()`

### HandleMatchTimeExpired (idempotent)
1. `StopMatchCountdown()`
2. `EvaluateAndFinishMatch()`
3. No direct fake result

### EvaluateAndFinishMatch (virtual/protected)
- Base: log explicit unavailable-score Warning/Error
- Leaves MatchState **Playing**, time **0** (intentional gap)
- Later score integration overrides / replaces

### FinishMatch(int32 WinnerTeamId, FGameplayTag WinReasonTag)
1. Authority only
2. Idempotent if already Finished
3. **Orchestration validation (preferred, no GameState API change):**  
   GameMode validates `WinnerTeamId >= -1` and WinReason is valid under `GP.Match.WinReason` (same branch rules as GameState) **before** calling GameState
4. `StopMatchCountdown()`
5. `GameState->SetMatchResult(...)`
6. Only after local validation passed: `GameState->SetMatchStateTag(Finished)`  
   (duplicated boundary validation = orchestration protection; GameState setters remain `void`)
7. No travel / UI / cleanup

---

## Start condition
`NumValidHumanPlayerControllers >= ExpectedHumanPlayers` (default **2**).  
No AI, no LobbyState, no ready flags.

## Countdown semantics
| Item | Value |
| --- | --- |
| Owner | `AGP_GameMode` |
| Mechanism | `FTimerHandle` + `FTimerManager`, 1.0 s |
| Duration | `MatchDurationSeconds` default **600.f**, clamp `>= 0` |
| Storage | GameState `SetMatchTimeRemaining` each second |
| Floor | never below `0` |
| Tick | forbidden for countdown |

## Timeout integration gap
At expiry, countdown cleared → `EvaluateAndFinishMatch` logs gap → state stays **Playing**, time **0**.  
Acceptance: hook fires correctly; **not** full TimerScore victory.  
Later: real evaluation → `FinishMatch(..., TimerScore)`.

## FinishMatch semantics
Authority, idempotent Finished, StopCountdown, validated SetMatchResult, then SetMatchStateTag(Finished).  
GameMode pre-validates result fields because GameState setters return `void` — do **not** change GameState API in this slice.

## Exact planned API

```cpp
virtual void BeginPlay() override;
virtual void PostLogin(APlayerController* NewPlayer) override;
virtual void Logout(AController* Exiting) override;

void TryStartMatch();
void StartMatchFlow();
void StopMatchCountdown();
void HandleMatchCountdownTick();
void HandleMatchTimeExpired();

void FinishMatch(int32 WinnerTeamId, FGameplayTag WinReasonTag);
virtual void EvaluateAndFinishMatch();
virtual void OnMatchFlowStarted();
```

## Configurable properties
| Property | Type | Default |
| --- | --- | --- |
| `ExpectedHumanPlayers` | `int32` | **2** |
| `MatchDurationSeconds` | `float` | **600.f** |

## Out of Scope
- PlayerState / score / TimerScore winner computation
- AI Controller / SP “1+AI” start
- LobbyState / ready / ServerTravel
- Engine MatchState as project SoT
- Unit/economy/SWARM spawn
- UI / ViewModels
- Blueprint GameMode; map/DefaultEngine GameModeClass on this pass
- Changing `AGP_GameState` API
- GP-S08+

## Files delivered
- `GP/Source/GPRuntime/Public/Game/GPGameMode.h`
- `GP/Source/GPRuntime/Private/Game/GPGameMode.cpp`

## Acceptance Criteria (implementation)
- [x] Compiles; `GameStateClass = AGP_GameState`.
- [x] BeginPlay → WaitingForPlayers; no auto Playing.
- [x] PostLogin → TryStartMatch; starts at ExpectedHumanPlayers (default 2).
- [x] 1 Hz timer manager countdown; no Tick.
- [x] Timeout → EvaluateAndFinishMatch logs gap; stays Playing @ time 0.
- [x] FinishMatch validates then writes result + Finished.
- [x] No Lobby/AI/PlayerState gameplay; no client match RPCs.
- [x] No GP-S08 bundled.
- [x] Operator Editor validation (Class Viewer / PIE without map wiring) PASSED.
- [x] Listen-server proof — **deferred** (temporary wiring not committed; accepted for close).
- [x] Tech lead accepted / operator accepted → DONE.

## Manual Editor validation (operator — no assets committed by agent)
1. Open project; confirm no module/load errors.
2. Class Viewer: `AGP_GameMode` visible.
3. PIE on current map (without assigning GP GameMode): must not break.
4. **Deferred listen-server proof** (temporary, do not commit):
   - Assign `AGP_GameMode` for PIE / map.
   - 2-player listen-server PIE.
   - Confirm WaitingForPlayers → Playing; countdown replicates.
   - At 0: Warning for deferred evaluation; state stays Playing.

## Remaining non-blocking decisions
- Exact log verbosity (Warning vs Error) for EvaluateAndFinishMatch — prefer Warning unless tech lead wants Error.
- Whether `OnMatchFlowStarted` is required empty virtual or omitted if unused — retain empty virtual hook.
- Temporary PIE config override of ExpectedHumanPlayers for 1-player tests — via subclass/config only, not production default.

## Risks / edge cases
- Production default 2 means single-player PIE will not auto-start until second PC or config override — intentional.
- Playing @ time 0 until score slice — HUD/timer UX must tolerate gap.
- FinishMatch pre-validation duplicates GameState rules — keep in sync when tags change.

## Linked canonical docs
- TDD/13, TDD/03 (Tick wording stale), GDD/07, GDD/08, First_Playable, TDD/00
- [`GP-S06_Game_State.md`](GP-S06_Game_State.md) + `AGP_GameState`

## Stop Condition
**STOP.** DONE.  
Tech lead accepted. Operator accepted.  
Do **not** start GP-S08. Do **not** auto-materialize the next task file.
