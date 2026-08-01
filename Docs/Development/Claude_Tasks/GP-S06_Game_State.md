# GP-S06 — AGP_GameState (Match State and Timer)

## Slice Group
Slice 2 — Match Flow + Asset Loader

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S05 DONE (Slice 1 Foundation complete).
- Native tags `GP.Match.State.*` and `GP.Match.WinReason.*` registered in `FGPGameplayTags` (GP-S02).
- Module `GPRuntime` scaffold exists (depends on `GPGASRuntime` for GameplayTags).

## Goal
Implement minimal server-authoritative `AGP_GameState : AGameStateBase` in `GPRuntime` that stores and replicates match flow state for HUD / future GameMode — **without** owning the match timer, win-condition logic, UI ViewModels, or map/config wiring.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S07 until explicitly assigned (do not auto-materialize GP-S07 task file).

### Closed with
- Replicated match state implemented (five properties, COND_None, REPNOTIFY_Always).
- Authority-only setters implemented (no Server/NetMulticast RPCs).
- Delegates / OnRep implemented (authority via setters; clients via OnRep; field-level result refresh).
- No Tick / timer / GameMode / RPC / assets / map-config GameStateClass changes.
- Multiplayer replication proof **deferred** to GameMode/map integration.

---

## Tech-lead locks (OD-1…OD-4) — RESOLVED

### OD-1 — RESOLVED: Timer orchestration belongs to `AGP_GameMode`
- GameState has **no** `FTimerHandle`, no `StartMatchTimer` / `StopMatchTimer`.
- Storage mutation only: `SetMatchTimeRemaining(float)`.

### OD-2 — RESOLVED: Expiry orchestration is GameMode-only
- GameState does not auto-Finished / pick winner / EndMatch / `OnMatchTimerExpired`.

### OD-3 — RESOLVED: `MatchTimeRemaining` is `float`
- Clamp `>= 0.0f`; no GameState Tick.

### OD-4 — RESOLVED: single global `FerroniteThreatValue`
- One `float`; per-player rejected; TDD/07 per-player wording stale.

### OD-5 — Deferred
Flat `WinnerTeamId` + `WinReasonTag` only (no `MatchResult` struct).

---

## Implementation status (2026-08-01)

### Files
- `GP/Source/GPRuntime/Public/Game/GPGameState.h`
- `GP/Source/GPRuntime/Private/Game/GPGameState.cpp`

### MatchStateTag default (chosen)
**Initialize to `FGPGameplayTags::Get().Match_State_Loading` when valid.**  
Rationale: `FGPGASRuntimeModule::StartupModule` calls `InitializeNativeTags()` before any world/GameState spawn; GPRuntime depends on GPGASRuntime. If Loading tag is somehow invalid, leave unset and log a Warning once — do not call `InitializeNativeTags` from GameState.

### Replicated properties
| Property | Type | Default | Rep / OnRep |
| --- | --- | --- | --- |
| `MatchStateTag` | `FGameplayTag` | Loading (if safe) | COND_None, REPNOTIFY_Always → `OnRep_MatchStateTag` |
| `MatchTimeRemaining` | `float` | `0.0f` | → `OnRep_MatchTimeRemaining` |
| `FerroniteThreatValue` | `float` | `0.0f` | → `OnRep_FerroniteThreatValue` |
| `WinnerTeamId` | `int32` | `-1` | → `OnRep_WinnerTeamId` |
| `WinReasonTag` | `FGameplayTag` | invalid | → `OnRep_WinReasonTag` |

### Authority API
`SetMatchStateTag` · `SetMatchTimeRemaining` · `SetFerroniteThreatValue` · `SetMatchResult` · `ClearMatchResult`  
+ BlueprintPure getters for all five fields.

### Validation
- Branch checks via `RequestDirectParent()` on native leaf tags + `MatchesTag` (no magic-string `RequestGameplayTag`).
- Authority-only; invalid → Warning + no mutate.
- Time/threat clamp `>= 0`; `WinnerTeamId < -1` rejected; WinReason under `GP.Match.WinReason`.
- `SetMatchResult` / `ClearMatchResult` = one logical result broadcast on authority.

### Delegate / OnRep
- Native `DECLARE_MULTICAST_DELEGATE_*` (C++ only; no BlueprintAssignable — no project precedent).
- Authority: broadcast from setters only (no manual OnRep).
- Clients: broadcast from OnRep.
- Result: separate OnReps; **field-level refresh** (unchanged field passes current/current); UI should read both getters. Documented on `OnMatchResultChanged`.

### Explicitly absent
No Tick, timer, GameMode, RPC, MatchResult struct, enum SoT, UI VM, map/config GameStateClass, assets, winner/score logic, storage aggregation.

### Builds
- GPEditor Win64 Development — **PASSED**
- GP Win64 Development — **PASSED**
- GP Win64 Shipping — **PASSED**

---

## Out of Scope
- GameMode timer / EndMatch / win evaluation.
- GameStateClass ini/map assignment.
- Blueprint / maps / ViewModels.
- Multiplayer listen-server replication proof (deferred to GameMode integration).
- GP-S07+.

---

## Acceptance Criteria
- [x] Compiles clean (Editor / Dev / Shipping).
- [x] Five properties replicated (`COND_None`, `REPNOTIFY_Always`, `ReplicatedUsing`).
- [x] Exact authority API; no Start/StopMatchTimer; no FTimerHandle.
- [x] Validation rules enforced.
- [x] Delegates: authority via setters, clients via OnRep; no double-broadcast on authority.
- [x] Defaults per spec; MatchStateTag default documented.
- [x] No GameMode / ini assignment / Blueprint / RPCs / GP-S07.
- [x] Operator: Editor/module load PASSED; AGP_GameState in Class Viewer; PIE PASSED; GP-S06 errors ABSENT.
- [x] Multiplayer replication — **deferred** to GameMode/map integration (accepted for GP-S06 close).
- [x] Tech lead accepted.
- [x] Operator accepted.

## Manual Editor / PIE validation
1. Open `GP/GP.uproject`.
2. Confirm no module/load errors for `GPRuntime`.
3. Confirm `AGP_GameState` visible as C++ parent / Class Viewer.
4. PIE on default map (do **not** change GameStateClass).
5. Multiplayer property replication — deferred until GameMode wires `GameStateClass`.

## Multiplayer validation plan (deferred)
Listen server + client after GameMode assigns `AGP_GameState`; authority setters must update both peers.

---

## Risks / edge cases
- Client may see staggered Winner/WinReason OnRep → field-level delegate; UI must use getters.
- Without GameStateClass assignment, PIE uses engine default GameState — class existence check only until GameMode slice.
- Stale TDD/07 per-player threat wording superseded by OD-4.

## Linked canonical docs
- TDD/13, TDD/03, GDD/07, GDD/08, First_Playable, TDD/07 (threat meaning; per-player stale), TDD/09 + `GPGameplayTags.*`.

## Stop Condition
STOP. GP-S06 closed as DONE. Do **not** start GP-S07; do not auto-materialize its task file.
