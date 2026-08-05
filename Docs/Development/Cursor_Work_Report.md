# Cursor Work Report — GP-S28 Diagnostic Scenario Correction

## Task
GP-S28 — Diagnostic Scenario Correction (operator-blocking TeamId/registry/setup)

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28-storage-threat`

## Base
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e`

## Prior candidate
`cd83858390db086c6913669f348a7402ae0a5ad3`

## Operator failure log
- `gp.Storage.SpawnDiagnostic 1` → MainBase spawned, Cap=500, Containers=5
- BeginPlay `RegisterMainBase` warned `TeamId=-1 is not a playable team`
- `gp.Worker.SpawnDiagnostic` → Worker TeamId=-1, Node=None
- `gp.Worker.List` → Workers=1, ResourceNodes=0, MainBases=1 — hauling loop unverifiable

## Root cause
1. MainBase `BeginPlay` registered before authority TeamId assignment (SpawnActor then SetTeamId).
2. `RegisterMainBase` still added actors with TeamId=-1 (warning only).
3. Worker diagnostic spawn did not assign TeamId and did not create a ResourceNode.
4. No single coherent scenario command — operator had to chain incomplete commands.

## TeamId assignment lifecycle
- `AGP_UnitBase::SetTeamId` now calls virtual `NotifyTeamIdChanged(Old, New)`.
- `AGP_MainBase` overrides: unregister on change; register only when TeamId ≥ 1.
- Diagnostic deferred spawn: `SpawnActorDeferred` → SetTeamId → `FinishSpawning` → refresh registration.
- Production-safe for post-BeginPlay TeamId assignment (same Notify path).

## MainBase registration fix
- No register when TeamId < 1 (silent wait; no warning spam).
- `RegisterMainBase` refuses TeamId < 1 (does not add).
- Team change clears stale registry entry and re-registers under new TeamId.
- Duplicate TeamId registration still errors.

## Diagnostic scenario API
Primary: `gp.Resource.SpawnDiagnosticScenario [TeamId=1]`  
Alias: `gp.Storage.SpawnDiagnosticScenario [TeamId=1]`  
Also: `gp.Storage.SpawnDiagnostic` now spawns **full** scenario (not MainBase-only).

Creates transient coherent set:
- MainBase TeamId=N + Storage
- ResourceNode (Ferronite, amount>0, MaxConcurrentMiners=4)
- Worker TeamId=N near deposit
- GameState registry resolves MainBase for team

## Worker team assignment
Diagnostic Worker always receives the scenario TeamId.  
`gp.Worker.SpawnDiagnostic [TeamId=1]` creates full scenario if MainBase missing; otherwise spawns Worker+Node with TeamId.

## ResourceNode creation
Always deterministic transient node at diagnostic layout (does not depend on authored map nodes).

## Spawn geometry
PrototypeArena west strip (existing diagnostic convention):
- MainBase: (-45000, 0, 100)
- ResourceNode: +2000 cm X
- Worker: +150 cm from Node

Nav projection + `TestPathSync` Worker↔Node and Node↔Base when NavigationSystem present.

## Navigation validation
`gp.Worker.List` ScenarioValidation includes NavReachable flags.  
If NavSys missing: warning path; Ready may still be true for functional forced-cycle tests.  
If NavSys present but path fails: ReadyForHaulingTest=false.

## gp.Worker.List readiness output
Emits ScenarioValidation:
PlayableTeamValid, WorkerHasMainBase, WorkerHasResourceNode, MainBaseRegisteredForTeam, WorkerAndBaseSameTeam, NodeMineable, NavReachable Worker→Node / Node→Base, **ReadyForHaulingTest**, Errors, Warnings.

## Diagnostic contract test
`gp.Resource.RunDiagnosticScenarioContractTest` (DiagnosticScenarioSpawn):
- one MainBase Team1, Worker Team1, valid Node
- registry resolve / no TeamId=-1
- team reassign clears stale keys
- cleanup empties registry
- transient actors

## Existing contract regressions
Entry points unchanged: Storage / Hauling / Worker / Mining / Cargo RunContractTest (operator re-run in PIE).

## Files changed
- `GPUnitBase` NotifyTeamIdChanged
- `GPMainBase` registration refresh
- `GPGameState` RegisterMainBase refuse TeamId<1
- `GPResourceLoopDiagnostics.*` (new)
- `GPWorker.*` spawn/list/scenario/contract
- `GPStorageComponent.cpp` full-scenario SpawnDiagnostic
- `GPRuntime.Build.cs` NavigationSystem private dep
- Docs: GP-S28 task, AI log, this report

## GPEditor / UHT result
**PASSED** — GPEditor Win64 Development + UHT

## GP Development / Shipping
**Not run**

## Map / content / LFS
Unchanged

## Operator flow after fix
1. `gp.Resource.SpawnDiagnosticScenario 1`
2. `gp.Worker.List` → ReadyForHaulingTest=true (with nav)
3. Mine via `gp.Worker.CommandMine` using listed names

## Correction commit SHA
`61f69dff98bb2b79f795a74d93e0b2c8a2b12b76`

## Git state
Branch `feature/gp-s28-storage-threat` pushed; main untouched; no PR
