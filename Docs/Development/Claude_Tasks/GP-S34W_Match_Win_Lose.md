# GP-S34W — Match Win/Lose MVP

## Status
**GP-S34W_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.**

## Slice Group
Post-GP-S33C (Unit Cap + Logistics Hub + TEMP HUD layout are on verified `main` @ `7873d2826130bc711486b7a8c0a55322685e8d9d`)

## Branch
`feature/gp-s34w-match-win-lose`  
Base: `main` @ `7873d2826130bc711486b7a8c0a55322685e8d9d`  
Prior remote feature head: `1828d5722da611cc940370a81f603388ac6f3bcb`  
Finalization: `b0a94f113e27fd9a6ed798b84bbbcc59f32954fc`

## Goal
Authoritative match finish for Delivery Quota, timer score ladder, and MainBase annihilation. Exactly one winner when two valid playable teams exist. No Draw. No client-side victory logic.

## In scope (delivered)
1. `DeliveryQuotaFerroniteScore = 5000` — first playable team with `FerroniteScore >= quota` wins immediately (`GP.Match.WinReason.DeliveryQuota`)
2. 600s hard cap — ladder FerroniteScore → OrbitalFerronite → CurrentUnits → stored MatchSeed (`GP.Match.WinReason.TimerScore`)
3. `bAnnihilationCountsAsWin = true` — playable MainBase death loses; opponent wins (`GP.Match.WinReason.Annihilation`)
4. Event-driven quota on FerroniteScore GAS write (PlayerState ASC delegate → GameMode)
5. Replicated `FGP_MatchResult` (array snapshot, not TMap) + compatibility `GetWinnerTeamId()` / `GetWinReasonTag()`
6. Finished gates: orbital unit drop, building Purchase/Deploy, Launch Container
7. TEMP HUD top-center timer/score + VICTORY/DEFEAT overlay
8. Contract `gp.Match.RunWinLoseContractTest`
9. Development-only console seams: `gp.Match.DebugStart`, `DebugSetFerroniteScore`, `DebugSetMatchTimeRemaining`, `DebugKillMainBase`, `DebugSetMatchSeed`

## Out of scope (deferred)
- `GP.Match.WinReason.OpponentDisconnect` / Steam disconnect victory
- Spectating when `bAnnihilationCountsAsWin == false` (match simply continues)
- Polished end screen, 15s transition, return to lobby/menu
- SWARM
- Pause semantics
- Movement/combat freeze after Finished (not required; economic/orbital gates only)
- Session Data Asset for quota/duration (values live on GameMode until DA exists)
- AI opponent
- Production CommonUI redesign

## Architecture
- `AGP_GameMode` remains the only match-flow authority. `FinishMatch` is exact-once (`MatchState == Finished` no-op).
- Quota: `AGP_PlayerState` binds FerroniteScore ASC change on authority and notifies GameMode. Evaluates **all** currently quota-valid playable teams (tie-break if several). No tick scan. Score stays on GAS.
- Timer: existing 1 Hz `FTimerManager` countdown. `EvaluateAndFinishMatch()` now resolves a winner. Zero playable participants: log invalid config, do **not** invent a winner.
- Annihilation: `AGP_MainBase::NotifyAuthorityDeath` → GameMode. Hub/unit death does not finish. World teardown / `bIsTearingDown` ignored. TeamId < 1 ignored. `bAnnihilationCountsAsWin=false` does not finish.
- Tie-break seed is stored at match start. Finish uses `HashCombine(MatchSeed, TeamId)` then lowest TeamId on hash collision. No RNG at comparison time.
- Economic gate: `AGP_GameState::AreEconomicOrdersAllowed()` is false only when Finished. Waiting/Playing still accept orders.
- Canonical `ExpectedHumanPlayers = 2`. One-human PIE stays `WaitingForPlayers` until `gp.Match.DebugStart` (dev-only; Shipping inert). Two-human PIE auto-starts. Production single-player auto-start is not implemented.

## GDD notes (not rewritten)
- GDD/08 shows `FinalScores` as `TMap` — implementation uses replicated `TArray<FGP_MatchTeamScore>` (UE replication).
- GDD/08 mentions `GameMode::Tick` for the timer — implementation remains GP-S07 1 Hz `FTimerManager`.
- GDD allows WinnerTeamId `-1` for Draw — MVP has no Draw when two valid playable teams exist.

## TEMP HUD
Existing corner layout **unchanged**. Top-center `MatchInfoPanel`:
- Playing: `MM:SS   SCORE <FerroniteScore> / <Quota>` from local PlayerState + replicated GameState
- Finished: `VICTORY` / `DEFEAT`, reason label, Winner TeamId
- Overlay is `HitTestInvisible` (does not block map)

## Post-finish gates vs still active
**Gated:** new orbital unit orders, building Purchase/Deploy, Launch Container.  
**Still active visually:** unit movement, combat, selection, camera, in-flight DropPods / launch telegraphs already accepted.

## Operator validation — FINAL PASS (2026-08-17)

### Match start / timer
PASS. Single-player PIE remains `WaitingForPlayers` until `gp.Match.DebugStart`. Timer initializes around 10:00 and counts down. Standard 2-player PIE starts automatically. Timer works for both players. `ExpectedHumanPlayers` remains 2.

### Delivery quota
PASS via production score-award path (not only debug setter): score set to 4999; Worker mined Ferronite; real container launch pushed FerroniteScore above 5000; immediate VICTORY; timer stopped.

### Annihilation
PASS in 2-player PIE: one MainBase killed; owner DEFEAT; opponent VICTORY; result replicated to both windows; immediate finish.

### Timer-score fallback
PASS in 2-player PIE: higher FerroniteScore + time forced to 0 → that player VICTORY, other DEFEAT, reason Timer Score.

## Finalization note
Docs-only. No C++ changes during finalization. GPEditor+UHT / GP Win64 Development / GP Win64 Shipping **PASS**. Listed regressions **Failures=0**. Shipping binary contains none of the five `gp.Match.Debug*` command strings.

## Stop condition
Operator PIE FINAL PASS complete. **NOT MERGED.** Human merge only. Agent must **not** merge.
