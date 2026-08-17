# Cursor Work Report — GP-S33C Unit Cap + Logistics Hub Capacity

## Status
**GP-S33C_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Branch
`feature/gp-s33c-unit-cap-logistics-hub`  
Base `main` SHA: `e40d545b89c27e2d9738082009fb691a5c8e5a2a`  
Feature head SHA: recorded after commit on this branch.

## Factual pre-slice debt
- `UGP_PlayerAttributeSet` already had `MaxUnits` / `CurrentUnits` (OwnerOnly) but the constructor did not initialize them.
- `GPUnitDropAuthority` enforced cap only when `MaxUnits > 0`, so `MaxUnits == 0` behaved as unlimited.
- `AGP_DropPod::AuthoritySpawnUnitPayload` bulk-incremented `CurrentUnits` by manifest Total when MaxUnits was active (including failed spawns).
- `AGP_LogisticsHub` was identity/navigation only — no MaxUnits bonus.

## MaxUnits initialization
- Native infinite GE `UGP_GE_UnitCap_Base5`: Additive +5 MaxUnits.
- Applied once from `AGP_PlayerState` after ASC `InitAbilityActorInfo` on authority (`bBaseUnitCapApplied`).
- Safe across BeginPlay / ClientInitialize / listen-server (no stack on repeated Init).

## CurrentUnits ownership lifecycle
- Counts only `AGP_Worker` / `AGP_SalvageWalker` (`CountsTowardPlayerUnitCap`).
- Register once when live/owned (BeginPlay, NotifyTeamIdChanged, PlayerState team catch-up).
- Unregister once on death (`HandleDeathInternal` / `NotifyAuthorityDeath`) or EndPlay — not waiting for LifeSpan.
- Flags prevent double increment/decrement; CurrentUnits clamped `>= 0`.
- Owner: DropPod `RequestingPlayerState`, else GameState PlayerArray team lookup. Unresolved: warning, no silent wrong-player increment.

## Pending reservation model
- `AGP_PlayerState::PendingOrbitalUnitCount` (server-authoritative, entity count).
- Committed = CurrentUnits + Pending.
- Validate `Committed + ManifestCount <= MaxUnits` (always; Max==0 is not unlimited).
- On accepted unit order: reserve once, then spend, then spawn pod.
- Live payload converts reservation (Pending-- as Current++).
- Failed/incomplete payload / `DebugForceSkipPayloadSpawn` / pod EndPlay leftover: release reservation.
- Spend-fail and pod-spawn-fail also release.

## Manifest cap validation
- Full manifest reject `EGP_UnitDropRejectReason::UnitCapReached`.
- Transport-slot validation unchanged and independent.
- Salvage Walker costs 1 CurrentUnit (not TransportSlotCost 2).

## Hub +5 GAS effect
- Native infinite `UGP_GE_UnitCap_Plus5` Additive +5 MaxUnits.
- Applied to owning PlayerState ASC; handle stored on the Hub; removed exactly once.

## Hub activation / removal
- Starts when the Hub actor is live/operational (orbital payload spawn or editor-placed owned live Hub).
- Does **not** start at Purchase READY, ghost, placement confirm, or descending DropPod.
- Destruction / EndPlay removes +5 once.
- Over-cap: Current stays; units are not killed; new orders reject until Current <= Max.
- Two players/teams isolated.

## Container cap bonus
**Deferred.** GDD +N is TBD. This slice implements only +5 MaxUnits.

## HUD
TEMP `UGP_TEMP_S28P_PlanetaryFerroniteHUD`: `UNITS Current / Max` (example `UNITS 4 / 5`).
Local confirm disabled when manifest would exceed cap; reject RPC shows `Unit Cap reached`.
MaxUnits/CurrentUnits bound like Orbital Ferronite (OwnerOnly attributes).

## Focused contract
`gp.Resource.RunUnitCapLogisticsHubContractTest` — **Failures=0** (A–L).

## Regressions (Failures=0)
| Test | Result |
| --- | --- |
| gp.Resource.RunUnitCapLogisticsHubContractTest | PASS |
| gp.Resource.RunOrbitalUnitDropContractTest | PASS |
| gp.Building.RunOrbitalBuildingDropContractTest | PASS |
| gp.Movement.RunRTSMovementReconciliationContractTest | PASS |
| gp.Resource.RunHaulNavApproachContractTest | PASS |
| gp.Resource.RunMineReassignmentHaulContractTest | PASS |
| gp.Resource.RunS28RegressionSuite | PASS |
| gp.Resource.RunDropOffResilienceContractTest | PASS |
| gp.Resource.RunContainerLaunchContractTest | PASS |
| gp.Resource.RunContainerLaunchHUDContractTest | PASS |
| gp.Combat.RunAttackMoveContractTest | PASS |
| gp.Combat.RunAutoAcquireContractTest | PASS |
| gp.Combat.RunSalvageWalkerContractTest | PASS |
| gp.Combat.RunLOSFireGateContractTest | PASS |
| gp.Combat.RunHealthBarContractTest | PASS |
| gp.Combat.RunTeamColorContractTest | PASS |

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASS** |
| GP Win64 Development | **not run** (finalization after operator PASS) |
| GP Win64 Shipping | **not run** (finalization after operator PASS) |

## Changed files (implementation)
C++ / docs only. Operator-local configs, map, Blueprint, Materials, VFX, Tools were not committed.

## Operator test steps
1. PIE with 2 starting Workers → HUD `UNITS 2 / 5`.
2. Give Orbital Ferronite; order units until Current=5; next order rejected **Unit Cap reached** (HUD feedback + log `GP UnitDrop HUD: Unit Cap reached` / Reason=UnitCapReached).
3. Kill one own unit → Current 5→4; one new unit order possible.
4. Purchase Logistics Hub → READY only: Max remains 5.
5. Deploy Hub: while pod descending, Max remains 5; Hub live → Max 5→10 (HUD UNITS line).
6. Fill above 5 if practical.
7. Destroy Hub → Max returns toward 5; existing units remain alive; new orders blocked if Current > Max.

Inspect: TEMP HUD UNITS line, Output Log `GP UnitCap` / `GP UnitDrop Result`.

## Explicit
**NOT MERGED.**
