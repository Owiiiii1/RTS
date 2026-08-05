# GP-S26 — UGP_MiningComponent

## Status
**GP-S26_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `693a36b8777babaea6085cb799397e9e0cddb77f` (GP-S25 merged)

Branch: `feature/gp-s26-mining-component`  
Candidate: `4d334a7f4fe331757e4e245d2979a27117a6b660`  
Diagnostic-host correction: `b58fce2072a9340e258a332b701f477c52181e25`  
Contract-test crash correction: `2801c73c8ef02ba4ae4286812d61ffd12c8410e6`

## Canonical roadmap position
`GP-S23R` → `GP-S24R` → `GP-S25` → **GP-S26 MiningComponent** → GP-S27 Worker → GP-S28 Storage+ThreatValue

Flow implemented: **ResourceNode → CargoComponent** only.

## Component ownership
- `UGP_MiningComponent : UActorComponent` (GPRuntime)
- Owner-agnostic; finds `UGP_CargoComponent` on owner
- Does not create Cargo in production logic
- Not permanently on combat units; Worker (GP-S27) will own Mining+Cargo
- Diagnostic: transient `AGP_MiningDiagnosticHost` (Cargo + Mining; NotPlaceable)

## State enum
`EGP_MiningState`: Idle, WaitingForSlot, Mining, CargoFull, DepositDepleted, OutOfRange, Invalid

## Begin / Stop API
- `BeginMining(AGP_ResourceNode*)` → `EGP_BeginMiningResult`
- `StopMining(EGP_MiningStopReason)` — idempotent
- Queries: IsMining, IsWaitingForSlot, GetMiningState, GetCurrentResourceNode, GetCargoComponent, tunables, range helpers

## Result / reason enums
**Begin:** Started, WaitingForSlot, AlreadyMiningTarget, RejectedNoAuthority, RejectedInvalidOwner, RejectedMissingCargo, RejectedInvalidNode, RejectedDepleted, RejectedCargoFull, RejectedOutOfRange, RejectedResourceMismatch

**Stop:** None, ManualStop, CargoFull, DepositDepleted, OutOfRange, InvalidTarget, MissingCargo, InvariantFailure, OwnerEndPlay, ComponentEndPlay, TargetEndPlay, ResourceMismatch

## ResourceDefinition values
From Ferronite DA (no competing writable defaults):

| Field | Value |
| --- | --- |
| AmountPerMiningCycle | 10 |
| MiningCycleDurationSeconds | 1 |
| InteractionRangeCm | 200 |

## First-cycle timing policy
First cycle fires **after** a full `MiningCycleDurationSeconds` (looping timer). No instant transfer on BeginMining.

## Range contract
BeginMining rejects OutOfRange without occupying a slot. Active cycles re-check range; failure → OutOfRange, timer cleared, slot released. No movement in S26.

## Occupancy integration
Uses ResourceNode `RequestMiningSlot` / `ReleaseMiningSlot` / HasActive / IsWaiting. No bypass.

## FIFO promotion event
Server-local `FGP_OnMinerSlotStateChanged` on `AGP_ResourceNode` (Granted/Waiting/release/promotion/cleanup/EndPlay). MiningComponent binds while targeting; Waiting → Active starts timer.

## Transfer transaction
Authority: `min(cycle, floor(remaining cargo), node amount)` → `ConsumeResource` → `AddCargo(consumed)`. Ensure Accepted == Consumed; else InvariantFailure stop. Integer transfers only.

## Partial-cycle handling
Deposit 5 or cargo remaining 5 → transfer exact remainder; terminal DepositDepleted or CargoFull; slot released.

## Timer lifecycle
Created only in Mining; cleared on Stop / CargoFull / Depleted / OutOfRange / invalid / EndPlay / target change. Absent in Idle/Waiting/terminal states.

## Replication
Replicated: CurrentMiningState (OnRep), CurrentResourceNode, LastStopReason. Timer/subscriptions/internals server-only. No client mutation RPC.

## Delegates
`OnMiningStateChanged` (authority SetState + client OnRep). `OnMiningCycleCompleted` (authority cycle). No Tick polling.

## Authority policy
Begin/Stop/cycle mutation require owner authority. Diagnostic cmds reject clients.

## Diagnostic hosts
- `AGP_MiningDiagnosticHost`: Transient, NotPlaceable, replicated, AlwaysRelevant, no actor tick; **USceneComponent SceneRoot** (no nav); Cargo+Mining as actor components; spawn near node via console; do not save maps.
- `AGP_MiningNoCargoDiagnosticHost`: SceneRoot + Mining only (missing-cargo rejection; no runtime DestroyComponent).

### Spawn-near-node invariant
`SpawnHostNearNode` places host at `Node + min(Range*0.5, 100)` with AlwaysSpawn, verifies actual location matches requested (±1) and `Dist < InteractionRangeCm`, else destroys host and errors. Logs `SpawnWithinRange`.

### Inspect before BeginMining
When `CurrentResourceNode == null`, Inspect uses a **DiagnosticNode** fallback for SoftDefinition / PrimaryAssetId / AmountPerCycle / CycleDuration / InteractionRangeCm / Distance / InRange. Does not mutate MiningComponent. Reports `CurrentNode=none` separately from `DiagnosticNode`.

## Diagnostics
`gp.Mining.SpawnDiagnosticHost`, `Inspect`, `Begin`, `Stop`, `RunContractTest` (uses production `ExecuteMiningCycle` via debug force path).

## Contract test
Staged `UGP_MiningContractTestRunner` (next-tick stages, weak refs, reentrancy guard). Transient test ResourceNode. Covers scene root / location / range, tunables 10/1/200, missing cargo (NoCargo host), invalid node, out-of-range, Begin, first-cycle delay, exact 10, fill 50, node −50, CargoFull, slot release, partial cargo/node, duplicate Begin, idempotent Stop, FIFO promote, EndPlay Destroy-without-Stop slot cleanup, ticks off. Does not mutate authored map nodes for cycle math.

### Crash correction (blocking defect)
Operator crash ×2 on sync `RunContractTest`: `StopMining` → `ReleaseMiningSlot` → occupancy Broadcast → `HandleMinerSlotStateChanged` → `StopMining` reentrancy. Fix: unbind-before-release + `bIsStoppingMining`; staged lifecycle-safe runner; silent invalid-miner cleanup.

## Lifecycle / EndPlay
Clear timer; **unbind before release**; release slot; clear refs. `bIsStoppingMining` blocks occupancy reentrancy. Node EndPlay broadcasts slot None → miners stop (broadcast skips pending-kill).

## In scope
MiningComponent; state machine; timer cycles; Node→Cargo transfer; definition tunables; range; occupancy+FIFO events; replication; diagnostics; host; docs.

## Out of scope
Worker; movement; Mine command unit wiring; Storage; MainBase; ThreatValue; Orbital/Score; UI; map; projectiles; VFX; GP-S27.

## Acceptance criteria
- [ ] Begin in range → Mining + timer; cargo still 0 until first cycle
- [ ] ~1s → +10 cargo / −10 node; ~5s → cargo 50 / node −50 / CargoFull / slot released
- [ ] Out of range rejected; no slot held
- [ ] 5 hosts: 4 Mining + 1 Waiting; stop active → promote
- [ ] Listen server replicates state; client Begin/Stop rejected
- [ ] RunContractTest Failures=0
- [ ] Cargo/ResourceNode/Mine validation regressions hold
- [ ] GPEditor Dev+UHT passed; GP Dev/Shipping deferred

## Operator validation steps
1. Place/find ResourceNode (5000). `gp.Mining.SpawnDiagnosticHost` near it.
2. Inspect: Idle, cargo 0/50, no slot, timer inactive.
3. `gp.Mining.Begin` → Mining, active slot, cargo 0, timer active.
4. Wait ~1.1s → cargo 10, node 4990.
5. Wait ~5.2s total → cargo 50, node 4950, CargoFull, slot false, timer inactive.
6. `gp.Cargo.Clear` on host; Begin again; `gp.Mining.Stop` → Idle, slot released.
7. Spawn far host → Begin RejectedOutOfRange.
8. Five hosts FIFO promotion test.
9. Listen server + client replication; client Begin rejected.
10. `gp.Mining.RunContractTest` → Failures=0. Do not save map.

## Known limitations
- No Worker / movement / Mine command execution
- Diagnostic host only for testing
- Contract test uses debug force cycle (same production function)
- Manual Spawn/Begin may still touch authored map nodes; contract test uses transient nodes

## Next canonical stage
**GP-S27 — AGP_Worker**
