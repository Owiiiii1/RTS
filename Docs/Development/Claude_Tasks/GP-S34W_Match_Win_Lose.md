# GP-S34W — Match Win/Lose MVP

## Status
**GP-S34W_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

## Slice Group
Post-GP-S33C (Unit Cap + Logistics Hub + TEMP HUD layout are on verified `main` @ `7873d2826130bc711486b7a8c0a55322685e8d9d`)

## Branch
`feature/gp-s34w-match-win-lose`  
Base: `main` @ `7873d2826130bc711486b7a8c0a55322685e8d9d`  
Implementation: `3a1bc381309e77757aed519bae9fe66957d056fd`  
SHA-record: `35a501d1d8e733dd9e8e85b06f879566e6538a1f`

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
9. Development-only console seams: `gp.Match.DebugSetFerroniteScore`, `DebugSetMatchTimeRemaining`, `DebugKillMainBase`, `DebugSetMatchSeed`

## Out of scope (deferred)
- `GP.Match.WinReason.OpponentDisconnect` / Steam disconnect victory
- Spectating when `bAnnihilationCountsAsWin == false` (match simply continues)
- Polished end screen, 15s transition, return to lobby/menu
- SWARM
- Pause semantics
- Movement/combat freeze after Finished (not required; economic/orbital gates only)
- Session Data Asset for quota/duration (values live on GameMode until DA exists)

## Architecture
- `AGP_GameMode` remains the only match-flow authority. `FinishMatch` is exact-once (`MatchState == Finished` no-op).
- Quota: `AGP_PlayerState` binds FerroniteScore ASC change on authority and notifies GameMode. Evaluates **all** currently quota-valid playable teams (tie-break if several). No tick scan. Score stays on GAS.
- Timer: existing 1 Hz `FTimerManager` countdown. `EvaluateAndFinishMatch()` now resolves a winner. Zero playable participants: log invalid config, do **not** invent a winner.
- Annihilation: `AGP_MainBase::NotifyAuthorityDeath` → GameMode. Hub/unit death does not finish. World teardown / `bIsTearingDown` ignored. TeamId < 1 ignored.
- Tie-break seed is stored at match start. Finish uses `HashCombine(MatchSeed, TeamId)` then lowest TeamId on hash collision. No RNG at comparison time.
- Economic gate: `AGP_GameState::AreEconomicOrdersAllowed()` is false only when Finished. Waiting/Playing still accept orders (existing resource/orbital tests keep working).

## GDD notes (not rewritten)
- GDD/08 shows `FinalScores` as `TMap` — implementation uses replicated `TArray<FGP_MatchTeamScore>` (UE replication).
- GDD/08 mentions `GameMode::Tick` for the timer — implementation remains GP-S07 1 Hz `FTimerManager`.
- GDD allows WinnerTeamId `-1` for Draw — MVP has no Draw when two valid playable teams exist.

## TEMP HUD
Existing corner layout **unchanged**. Added top-center `MatchInfoPanel`:
- Playing: `MM:SS   SCORE <FerroniteScore> / <Quota>` from local PlayerState + replicated GameState
- Finished: `VICTORY` / `DEFEAT`, reason label, Winner TeamId
- Overlay is `HitTestInvisible` (does not block map)

## Post-finish gates vs still active
**Gated:** new orbital unit orders, building Purchase/Deploy, Launch Container.  
**Still active visually:** unit movement, combat, selection, camera, in-flight DropPods / launch telegraphs already accepted.

## Operator validation (do not self-approve)
Development-only, authority, non-shipping, logged, not RPCs:

1. PIE listen-server on `L_PrototypeArena` (one human stays `WaitingForPlayers` — this is canonical). Run `gp.Match.DebugStart`. Confirm top-center `10:00   SCORE 0 / 5000` and countdown. `ExpectedHumanPlayers` remains 2.
2. `gp.Match.DebugSetFerroniteScore 5000` → immediate VICTORY for local team, reason Delivery Quota. Second score write must not change winner.
3. Restart PIE. `gp.Match.DebugStart`. `gp.Match.DebugSetFerroniteScore 100` on one team, give the other more score, `gp.Match.DebugSetMatchTimeRemaining 0` → Timer Score, higher FerroniteScore wins.
4. Kill a playable MainBase (`gp.Match.DebugKillMainBase` or combat). Opponent wins Annihilation. Killing a Worker / Logistics Hub must not finish.
5. After Finished: Launch Container, Unit Drop Confirm, Purchase/Deploy Logistics Hub must reject. Map still viewable. `gp.Match.DebugStart` must not restart the finished match.

## Stop condition
Operator PIE FINAL PASS, then human merge. Agent must **not** merge.
