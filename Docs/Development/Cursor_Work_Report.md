# Cursor Work Report — GP-S34W Match Win/Lose MVP

## Status
**GP-S34W_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

## Branch
`feature/gp-s34w-match-win-lose`  
Base `main` SHA: `7873d2826130bc711486b7a8c0a55322685e8d9d`  
Implementation SHA: `3a1bc381309e77757aed519bae9fe66957d056fd`  
Feature head SHA: recorded by the SHA-record commit immediately after this documentation update.

## Pre-slice gaps found
- `AGP_GameMode::EvaluateAndFinishMatch()` was an intentional GP-S07 gap: timer expiry logged and left `Playing` at time 0 with no winner.
- `AGP_GameState` replicated `WinnerTeamId` / `WinReasonTag` but not quota, annihilation toggle, MatchSeed, or a finish-time score snapshot.
- FerroniteScore lived on GAS with no GameMode notify (GPGASRuntime cannot call GameMode).
- `AGP_MainBase` had no `NotifyAuthorityDeath` override for match authority.
- TEMP HUD had no match timer/score/result line. Corner layout from the HUD-layout slice was already correct and was preserved.
- Economic commands had no Finished gate.

## Exact architecture implemented
`AGP_GameMode` remains the only match-flow owner. Client never computes a winner. `FinishMatch` is exact-once (`MatchState == Finished` returns immediately). After Finished the countdown is stopped, later score writes / death events cannot replace the result, and `FGP_MatchResult` stays stable.

Defaults live on GameMode (`EditDefaultsOnly`), published to GameState at BeginPlay / Playing start: duration 600, quota 5000, annihilation true. Client-visible timer remains GameState-owned.

## Quota path
Canonical score write remains GAS (`UGP_GE_AddScore` from container launch). Authority `AGP_PlayerState` binds the FerroniteScore ASC change delegate and calls `AGP_GameMode::NotifyFerroniteScoreChanged`. GameMode evaluates **all** playable teams currently `>= quota` (same-tick both-valid uses the canonical tie-break, still DeliveryQuota). No actor scan. FerroniteScore is not moved off GAS. OrbitalFerronite is never the victory score.

## Timer path
Existing 1 Hz `FTimerManager` countdown is unchanged. At 0, exactly one `EvaluateAndFinishMatch()` gathers playable `AGP_PlayerState` from GameState `PlayerArray` (ignore TeamId 0 / `< 1` / invalid). Ladder: FerroniteScore → OrbitalFerronite → CurrentUnits → MatchSeed. Integers via `FMath::RoundToInt`. One valid participant wins that team. Zero valid: log invalid match configuration, do not invent a winner.

## Annihilation path
`AGP_MainBase::NotifyAuthorityDeath` → GameMode while Playing, TeamId `>= 1`, `bAnnihilationCountsAsWin`, world not tearing down. Opponent is the other playable team. Hub / unit death does not finish. `Destroy()` / EndPlay without GAS death does not finish. Toggle false: log, continue match; Spectating deferred.

## Deterministic tie-break
MatchSeed is created and stored on GameState at Playing start. Finish uses a pure function `HashCombine(MatchSeed, TeamId)` and lowest TeamId on hash collision. No RNG at comparison time. Iteration order is not used as a tie-break.

## MatchResult replication
`FGP_MatchResult` (WinnerTeamId, WinnerReason, MatchDuration, `TArray<FGP_MatchTeamScore>` FinalScores) is replicated. Compatibility getters `GetWinnerTeamId()` / `GetWinReasonTag()` remain. TMap is not used. Final scores are captured at finish and do not keep mutating inside the result.

## Post-finish command gates
`AGP_GameState::AreEconomicOrdersAllowed()` is false only when Finished. Waiting/Playing still accept orders.

Gated: orbital unit drop, building Purchase/Deploy, Launch Container (`MatchFinished` reject reasons appended at enum end).

Not frozen: movement, combat, selection, camera, already-accepted in-flight DropPods / launch telegraphs.

## TEMP HUD additions
Corner layout unchanged (top-right Orbital+UNITS, bottom-right procurement, bottom-left base/container, bottom-center Launch). Added top-center `MatchInfoPanel`: `MM:SS   SCORE n / quota` while Playing; `VICTORY`/`DEFEAT` + reason + Winner TeamId when Finished. HitTestInvisible. No menu/lobby/15s transition.

## Debug / operator seams (non-shipping, authority-only, not RPCs)
- `gp.Match.DebugSetFerroniteScore <Amount> [TeamId]`
- `gp.Match.DebugSetMatchTimeRemaining <Seconds>` (0 triggers timer evaluation)
- `gp.Match.DebugKillMainBase [TeamId]`
- `gp.Match.DebugSetMatchSeed <Seed>`

Logged as `GP Match Debug`. Inert in Shipping.

## Deferred
OpponentDisconnect; Spectating; polished end screen; return to lobby/menu; SWARM; pause semantics.

## Exact tests (Failures=0)
| Test | Result |
| --- | --- |
| gp.Match.RunWinLoseContractTest | Failures=0 |
| gp.Resource.RunContainerLaunchContractTest | Failures=0 |
| gp.Resource.RunContainerLaunchHUDContractTest | Failures=0 |
| gp.Resource.RunUnitCapLogisticsHubContractTest | Failures=0 |
| gp.Resource.RunOrbitalUnitDropContractTest | Failures=0 |
| gp.Building.RunOrbitalBuildingDropContractTest | Failures=0 |
| gp.Combat.RunSalvageWalkerContractTest | Failures=0 |
| gp.Combat.RunLOSFireGateContractTest | Failures=0 |
| gp.Combat.RunHealthBarContractTest | Failures=0 |
| gp.Resource.RunS28RegressionSuite | Failures=0 |

## Candidate build
GPEditor Win64 Development + UHT **PASS**. GP Development / Shipping **not run** (candidate stop).

## Exact changed files
- `GP/Source/GPRuntime/Public/Game/GPMatchResult.h` (new)
- `GP/Source/GPRuntime/Public/Game/GPMatchWinLoseContractTest.h` (new)
- `GP/Source/GPRuntime/Private/Debug/GPMatchWinLoseContractTest.cpp` (new)
- `GP/Source/GPRuntime/Public/Game/GPGameMode.h`
- `GP/Source/GPRuntime/Private/Game/GPGameMode.cpp`
- `GP/Source/GPRuntime/Public/Game/GPGameState.h`
- `GP/Source/GPRuntime/Private/Game/GPGameState.cpp`
- `GP/Source/GPRuntime/Public/Player/GPPlayerState.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerState.cpp`
- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPMainBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPMainBase.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPUnitDropManifest.h`
- `GP/Source/GPRuntime/Private/Orbital/GPUnitDropAuthority.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropAuthority.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Public/Resources/GPStorageComponent.h`
- `GP/Source/GPRuntime/Private/Resources/GPStorageComponent.cpp`
- `GP/Source/GPRuntime/Public/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h`
- `GP/Source/GPRuntime/Private/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.cpp`
- `Docs/Development/Claude_Tasks/GP-S34W_Match_Win_Lose.md` (new)
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/TDD/03_Multiplayer_Architecture.md`
- `Docs/TDD/10_Data_Assets.md`
- `Docs/TDD/13_Architecture_Proposal.md`

Not committed (operator-local): `GP/Config/DefaultEngine.ini`, `DefaultGame.ini`, `L_PrototypeArena.umap`, `BP_ResourceNode_AuthoredExample.uasset`, `GP/Content/GrimProtocol/Blueprint/`, `Materials/`, VFX packs, `Tools/`, AutoAcquire contract CRLF noise.

## Explicit
**NOT MERGED.**
