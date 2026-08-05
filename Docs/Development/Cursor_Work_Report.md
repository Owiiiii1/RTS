# Cursor Work Report — GP-S28 Storage + FerroniteThreatValue

## Task
GP-S28 — `UGP_StorageComponent` + FerroniteThreatValue write + Worker auto-return/drop-off haul chain

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28-storage-threat`

## Base
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e`

## Canonical dependencies
GP-S23R → GP-S24R → GP-S25 → GP-S26 → GP-S27 (merged) → **GP-S28**

## Docs inspected
- Docs/README.md, DOCUMENTATION_INDEX.md
- GDD/02_Core_Gameplay_Loop.md, GDD/06_Resources.md
- TDD/07_Resource_Architecture.md, TDD/13_Architecture_Proposal.md, TDD/05_Unit_Architecture.md, TDD/06_Building_Architecture.md
- ADR_0002, ADR_0003, ADR_0006; ADR_0009 no-local-production (scope)
- Claude_Tasks GP-S23R…GP-S27
- Live: GameState, Worker, Cargo, Mining, UnitCommand, Movement, UnitBase/MobileUnit, PlayerState TeamId, GameplayTags, diagnostic runners

## Superseded docs rejected
- Pre-pivot CarriedFerronite / GE_GP_AddFerronite drop-off income
- TDD leftover-wait overflow when conflicting with GDD Two-State “overflow lost”
- Auto-launch / Orbital / Score as part of S28 (belongs GP-S36)

## Actual existing MainBase architecture
**None in C++ before S28.** No BuildingBase/MainBase/BuildingDefinition/DropOffRange. Tags only (`GP.Building.Type.MainBase`).

## Actual existing GameState threat architecture
Single replicated `float FerroniteThreatValue` (global). Docs require per-player stock.

## Selected host / reference solution
- Minimal `AGP_BuildingBase` + `AGP_MainBase` (adaptation: S34/S39 deferred for content)
- Authority `AGP_GameState` MainBase registry (`Register/Unregister/FindMainBaseForTeam`) — no GetActorOfClass / name lookup
- Per-team threat array SoT + legacy scalar synced from Team 1 / first entry

## Files changed
- `GP/Source/GPRuntime/Public|Private/Resources/GPStorageComponent.*` (new)
- `GP/Source/GPRuntime/Public|Private/Buildings/GPBuildingBase.*` (new)
- `GP/Source/GPRuntime/Public|Private/Buildings/GPMainBase.*` (new)
- `GP/Source/GPRuntime/Public|Private/Game/GPGameState.*`
- `GP/Source/GPRuntime/Public|Private/Units/GPUnitCommandComponent.*`
- `GP/Source/GPRuntime/Public|Private/Units/GPWorker.*`
- `Docs/Development/Claude_Tasks/GP-S28_Storage_Threat.md` (new)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md` (this file)

## StorageComponent API
`AddPlanetaryFerronite` → `FGP_StorageAddResult`; `RemovePlanetaryFerronite` rollback; totals/Ready/Launching counts; `GetThreatPerStoredUnit`; `ValidateStorageContract`; no permanent Tick

## Container data model
`FGP_StorageContainer` + `EGP_StorageContainerState` Empty/Filling/Ready/Launching(scaffold)

## Container defaults / data source
Capacity **100**, Count **5**, soft Ferronite ResourceDefinition — temporary placeholders until BuildingDefinition

## Fill / partial / full behavior
Index-order fill; Ready on full; partial accept; full storage Accepted=0; invalid inputs rejected without mutation

## Launch boundary
S28: fill to Ready only. Launch/Orbital/Score/Threat decrease → GP-S36

## FerroniteThreatValue implementation
`AddFerroniteThreatValueForTeam(TeamId, Accepted × ThreatPerStoredUnit)` on successful drop-off only

## Per-player / team semantics
`TArray<FGP_TeamFerroniteThreat>` replicated; getters/setters/add; legacy float mirrored

## Threat invariant
Per-team stock × ThreatPerStoredUnit (DA field, code default 0.5)

## Worker auto-return flow
CargoFull → return own MainBase → drop-off → return live deposit → BeginMining (serial-aware)

## Depleted partial flow
DepositDepleted + cargo>0 → return → drop → Idle (no return to depleted). Cargo==0 → Idle immediately

## MainBase lookup
`GameState->FindMainBaseForTeam(Worker->GetTeamId())`

## DropOffRange
`AGP_MainBase::DropOffRangeCm = 400` (TDD placeholder)

## Safe approach
GP-S27 3D-safe geometry reused for DropOffRange; safety 25; one corrective

## Command serial / stale callback protection
Haul shares Mine CommandSerial; replacements reset haul; stale movement ignored

## Interruption
Move/Stop/new command cancels haul; Cargo retained unless LOST overflow already applied

## Transaction / rollback
Storage accept → Cargo remove exact → Threat; mismatch rolls back Storage

## Storage-full policy
GDD LOST overflow + `HaulLostOverflow` log; Threat on Accepted only

## Replication
Storage containers/config; GameState team threat + legacy scalar; haul state server-only

## Tick policy
Storage/MainBase/BuildingBase: no permanent Tick. UnitCommand Attack tick unchanged policy

## Lifecycle
MainBase register/unregister; EndPlay haul reset; runners world-cleanup

## Diagnostics
`gp.Storage.*`, `gp.Worker.List`, extended Inspect, `gp.Worker.RunHaulingContractTest`

## Contract results (code-side)
Staged Storage + Hauling runners implemented; operator must confirm Failures=0 in PIE

## Worker / Mining / Cargo / Move-Attack regression
Existing runners unchanged entry points; haul hooks only on Mine terminals + cancel paths; Attack/Move paths not redesigned

## Map / LFS / content
No map, LFS, Blueprint, or projectile changes

## GPEditor / UHT result
**PASSED** — GPEditor Win64 Development (+UHT)

## GP Development
**Not run** (post-operator finalization)

## GP Shipping
**Not run** (post-operator finalization)

## Scope exclusions
Orbital/Score GEs; launch VFX/UI/timers; SWARM; Logistics Hub; HUD; content MainBase/Worker BP; map; Slice 7; PR/merge

## Operator validation steps
See `Docs/Development/Claude_Tasks/GP-S28_Storage_Threat.md` — SpawnDiagnostic, List/Inspect, haul PIE, contract tests, confirm no Orbital/Score on drop-off

## Known limitations
Minimal code MainBase; placeholder DropOff/containers; legacy threat scalar Team1 mirror; WaitingForStorage unused (LOST policy)

## Commit SHA
`cd83858390db086c6913669f348a7402ae0a5ad3`

## Git state
Branch `feature/gp-s28-storage-threat` pushed; main untouched; no PR
