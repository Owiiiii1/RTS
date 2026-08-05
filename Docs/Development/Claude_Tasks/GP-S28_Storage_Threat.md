# GP-S28 — UGP_StorageComponent + FerroniteThreatValue write

## Status
**GP-S28_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `4aae0121b6cfe8709e0c4f5c75392c07a247fe9e` (GP-S27 Worker merged)

Branch: `feature/gp-s28-storage-threat`  
Candidate: `cd83858390db086c6913669f348a7402ae0a5ad3`  
Diagnostic TeamId correction: `61f69dff98bb2b79f795a74d93e0b2c8a2b12b76`  
Diagnostic nav correction: `caf5bf0c947176ce5c72affadae41cbbd60be590`

## MainBase registry uniqueness + contract isolation
Operator failure after nav-ready Team1 scenario: `RunDiagnosticScenarioContractTest` logged duplicate MainBase for TeamId=1 then **still Add** → Count=2 → `MainBaseRegistryResolveFailed`.

Fixes:
- `AGP_GameState::RegisterMainBase` → `EGP_MainBaseRegisterResult`; prune stale weaks; same-actor idempotent; **reject duplicate without Add** (`RejectedDuplicate`)
- `AGP_MainBase` sets `bRegisteredWithGameState` only on Registered/AlreadyRegistered; rejected EndPlay cannot unregister existing
- Contract picks free playable TeamId (Team2 if Team1 occupied); contract-owned tags only; operator Team1 preserved
- Duplicate-rejection contract stage assertions (First/Idempotent/Rejected/CountOne/Preserved/Cleanup/Replacement)
- Operator re-spawn cleans prior operator diagnostic only; never authored/production; never second MainBase for same team
- `gp.Worker.List`: `MainBaseCountForWorkerTeam`, `RegistryUniqueForTeam`, `ResolvedMainBaseMatchesListedBase`; Ready requires count=1 + unique

## Diagnostic scenario correction (operator-blocking)
Operator failure: MainBase registered at TeamId=-1; Worker TeamId=-1; Node=None.

Fixes:
- Production-safe MainBase registry via `NotifyTeamIdChanged` + register only TeamId≥1
- Primary command: `gp.Resource.SpawnDiagnosticScenario 1` (alias `gp.Storage.SpawnDiagnosticScenario`)
- `gp.Storage.SpawnDiagnostic` / `gp.Worker.SpawnDiagnostic` create coherent full scenario
- `gp.Worker.List` → ScenarioValidation + ReadyForHaulingTest
- Contract: `gp.Resource.RunDiagnosticScenarioContractTest`

## Diagnostic nav-reachability correction
Operator failure after TeamId fix: actors OK but NavWorkerToNode/NavNodeToBase=false (hardcoded -45000 off mesh).

Fixes:
- Discover navigable anchor (authored node / mobile / PlayerStart / arena candidates / random)
- Build projected MainBase / Node / Worker; path-test approach points (not actor origins)
- Atomic spawn: validate paths first; cleanup on failure; Ok=false leaves no actors
- ReadyForHaulingTest requires NavSystemPresent + three paths (missing nav = error)
- Contract asserts nav paths; tag-scoped cleanup (operator vs contract-owned)

## Canonical roadmap position
`GP-S23R` → `GP-S24R` → `GP-S25` → `GP-S26` → `GP-S27` → **GP-S28 Storage + Threat write** → (later) GP-S36 launch / GP-S39 content MainBase → Slice 7

## Canonical source hierarchy
1. Active GDD Two-State Container model — `Docs/GDD/06_Resources.md`
2. Active TDD resource / architecture — `Docs/TDD/07_Resource_Architecture.md`, `Docs/TDD/13_Architecture_Proposal.md`
3. ADR data-driven / GAS-first / indie scope / no local production
4. Prior Slice 6 task docs GP-S23R…GP-S27
5. **Rejected for S28 drop-off semantics:** superseded pre-pivot CarriedFerronite / GE_GP_AddFerronite paths; TDD “leftover stays on Worker until storage frees” when conflicting with GDD overflow-lost (GDD wins for Two-State Container)

## Active Two-State Container model
- **Planetary Ferronite:** raw; Worker Cargo or MainBase containers; not spendable; drop-off = Storage mutation; raises `FerroniteThreatValue` by accepted × ThreatPerStoredUnit
- **Orbital Ferronite / FerroniteScore:** only on container launch (GP-S36) — **not** written on Worker drop-off

## Superseded sections explicitly rejected
- Drop-off player GE / Orbital / Score income
- Legacy `CarriedFerronite` / `GE_GP_AddFerronite` / `GE_GP_SendToOrbit`
- Treating Threat as cumulative mined lifetime counter
- Auto-launch / GE_GP_AddOrbital / GE_GP_AddScore in S28

## Existing MainBase / GameState inventory (pre-S28)
- No `AGP_MainBase` / `AGP_BuildingBase` / BuildingDefinition / DropOffRange field in C++
- No MainBase registry / player base reference
- `AGP_GameState::FerroniteThreatValue` — single global replicated float
- OrbitalFerronite / FerroniteScore on `UGP_PlayerAttributeSet` only
- `UGP_ResourceDefinition::ThreatPerStoredUnit` default **0.5** (code/DA); GDD narrative default 1.0 — runtime reads DA field

## StorageComponent host
- Minimal `AGP_BuildingBase : AGP_UnitBase` (S28 adaptation ahead of full GP-S34)
- Minimal `AGP_MainBase : AGP_BuildingBase` with `UGP_StorageComponent` (S28 host; visual/content MainBase remains GP-S39)
- No production/construction; no permanent Tick; tag `GP.Building.Type.MainBase`

## Container structure
`FGP_StorageContainer { CurrentAmount, State }`  
States: `Empty | Filling | Ready | Launching` (Launching scaffold only — no launch mutation)

## Container config source
No BuildingDefinition yet → component defaults marked temporary canonical placeholders:
- `ContainerCapacity = 100`
- `ContainerCount = 5`
- Soft `ResourceDefinition` → Ferronite DA

## Fill algorithm
Authority `AddPlanetaryFerronite(Requested)`:
- Reject NaN/Inf/non-positive
- Fill first available non-full / non-Launching container by index
- Filling → Ready on capacity
- Spill remainder to next containers
- Return `FGP_StorageAddResult` (Requested/Accepted/Rejected/ContainersTouched/StorageFullAfter/ReadyContainerCreated)
- Overflow stays with caller (no silent delete inside Storage)

## Accepted / overflow semantics
Worker drop-off uses Accepted for Cargo remove + Threat.  
**Storage-full policy (GDD/06):** remainder **LOST** (`ClearCargo` after accepted remove) + diagnostic `HaulLostOverflow`. Threat only on Accepted.

## Exact GP-S28 launch boundary
**In S28:** Empty → Filling → Ready; Ready stays ready.  
**Out of S28 (GP-S36):** Launching → Departed → Empty; Orbital/Score GEs; Threat decrease; VFX/timers/UI.

## FerroniteThreatValue model
Fluctuating stock = raw Planetary Ferronite currently in that team’s MainBase containers × ThreatPerStoredUnit.  
Writer: drop-off path → `AGP_GameState::AddFerroniteThreatValueForTeam`.

## Per-player / team decision
GDD requires per-player. Pre-S28 code had one scalar.  
**S28 minimal compatible model:**
- SoT: replicated `TArray<FGP_TeamFerroniteThreat>` + `Get/Set/AddFerroniteThreatValueForTeam`
- Legacy `FerroniteThreatValue` scalar kept; synced from TeamId **1** if present else first entry
- `SetFerroniteThreatValue` forwards to Team 1

## Threat invariant
`GetFerroniteThreatValueForTeam(T) == sum(containers of that team’s MainBase) × ThreatPerStoredUnit`  
(after each successful drop-off write; launch decrease is S36)

## Worker return-to-base
On Mine terminal CargoFull or DepositDepleted with cargo > 0:
- Keep Held Mine serial as chain identity
- Stash last deposit; resolve own MainBase via GameState registry
- Move → drop-off → optional return to live deposit → BeginMining

## DropOffRange source
No BuildingDefinition → `AGP_MainBase::DropOffRangeCm = 400` (TDD/07, TDD/10 placeholder)

## Safe approach
Shared 3D-safe helper (GP-S27 geometry) with DropOffRange, AcceptanceRadius, SafetyMargin=25, one corrective attempt, stale serial protection.

## Transaction order
1. Storage.AddPlanetaryFerronite(CargoAmount)  
2. Cargo.RemoveCargo(Accepted) — must match  
3. On mismatch: Storage.RemovePlanetaryFerronite(Accepted) rollback; stop chain  
4. Threat += Accepted × ThreatPerStoredUnit  
5. Overflow LOST clear remainder

## Command identity / cancellation
Haul uses Mine CommandSerial. New Move/Stop/Mine replaces → ResetMine + ResetHaul; stale arrivals ignored. Same-deposit Mine while hauling → idempotent. Stop keeps Cargo; no further auto actions.

## Replication
Storage: capacity/count/containers replicated; no client mutation.  
GameState: team threat array + legacy scalar.  
Haul orchestration: server-only (like Mine/Attack).

## Lifecycle
MainBase BeginPlay register / EndPlay unregister. Worker/MainBase destroy during haul → fail safe, no dangling writes. Contract runners: weak ptrs, next-tick, world cleanup.

## Diagnostics
- `gp.Storage.List|SpawnDiagnostic|Inspect|Add|RunContractTest`
- `gp.Worker.List|Inspect` (haul fields)|`RunHaulingContractTest`
- Existing Worker/Mining/Cargo contract tests unchanged entry points

## Contract tests
Storage: fill/Ready/multi-container/partial/full/invalid/tick/Launching scaffold/threat metadata.  
Hauling: cargo-full return+threat; depleted+5 idle; partial storage LOST; interrupt; ownership; lifecycle.

## In-scope
UGP_StorageComponent; minimal MainBase/BuildingBase; registry; container fill; Planetary storage; Threat write; Worker haul chain; diagnostics; docs.

## Out-of-scope
Orbital/Score GEs; launch VFX/UI/timers; SWARM curves; Logistics Hub; HUD; Worker/MainBase Blueprints; map; projectiles; Slice 7.

## Acceptance criteria
- Drop-off never writes Orbital/Score
- Threat rises by Accepted × ThreatPerStoredUnit only
- Cargo/Storage conservation on success; rollback on remove mismatch
- Own-team MainBase only
- GPEditor Dev+UHT builds
- Contract tests Failures=0 in operator PIE

## Operator validation steps
1. PIE listen server: `gp.Resource.SpawnDiagnosticScenario 1`
2. Expect Ok=true, NavWorkerToNode/NavNodeToBase/NavBaseToNode=true, ReadyForHaulingTest=true
3. `gp.Worker.List` → ScenarioValidation Ready=true + SuggestedCommand
4. Mine via SuggestedCommand → haul → Storage/Threat → return to deposit
5. `gp.Resource.RunDiagnosticScenarioContractTest` (nav asserts) + Storage/Hauling/Worker/Mining/Cargo
6. Confirm no Orbital/Score change on drop-off

## Known limitations
- MainBase is code-minimal (no content BP); DropOffRange/container counts are placeholders
- Legacy GameState scalar mirrors Team 1 / first entry only
- WaitingForStorage activity enum reserved; overflow policy is LOST (not wait)
- Launching state scaffold only

## Next roadmap action after Slice 6
Operator validate → finalize → merge S28; then Slice 7 combat reconciliation / later GP-S36 launch + GP-S39 content MainBase.
