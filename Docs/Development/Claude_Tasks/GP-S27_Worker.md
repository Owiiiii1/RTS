# GP-S27 — AGP_Worker

## Status
**GP-S27_FINALIZED_READY_FOR_MERGE**

Overall: **GP-S27_DONE_WORKER** (merge when operator requests)

## Baseline
`main` @ `860070c4acbcb85fd5c4334628584372bdd082ca` (GP-S26 MiningComponent merged)

Branch: `feature/gp-s27-worker`  
Candidate: `07e20fbfff36e181076d237d0596ef6f25b40951`  
Approach correction: `4d38a405729fb5766a5498e91436896ef5efda6b`  
Finalization: *(see Cursor_Work_Report / HEAD)*

## Canonical roadmap position
`GP-S23R` → `GP-S24R` → `GP-S25` → `GP-S26` → **GP-S27 Worker** → GP-S28 StorageComponent + FerroniteThreatValue

## Worker inheritance
`AGP_Worker : AGP_MobileUnit` (sibling of `AGP_Unit`)

## Component composition
- Capsule root (selection/collision)
- Inherited: UnitCommandComponent, ASC, AttributeSet, CombatPresentation, MovementComponent
- Owned: `UGP_CargoComponent`, `UGP_MiningComponent`
- No CombatComponent / TargetingComponent / StorageComponent
- No second ASC
- Actor tick disabled (base UnitBase policy); movement tick only while moving

## Worker activity model
`EGP_WorkerActivityState` derived getter (not a second writable mining store):
Idle / MovingToMine / WaitingForMiningSlot / Mining / CargoFull / DepositDepleted / CommandFailed

SoT: MiningComponent execution state + UnitCommandComponent Mine approach state + movement.

## Command routing
Existing PC → CommandComponent validate/normalize → `ReceiveCommand` → `UGP_UnitCommandComponent::HandleCommand`.
Mine execution added beside Attack (serial-aware). No Worker `Server_BeginMining` RPC.
Validate filters Mine issuers to `AGP_Worker` (`UnsupportedUnit` if none).

## Mine immediate flow
In range (≤ InteractionRangeCm): stop move → `BeginMining` → Mining or WaitingForSlot. First cycle after full 1s (GP-S26).

## Mine movement flow
Out of range: weak target via Held → `RequestMove(approach, CommandSerial)` → Approaching → on `OnMovementResult` Reached + serial match → revalidate → `BeginMining`. No distance polling Tick.

## Approach-point policy
Horizontal destination along Worker←Node with 3D-safe budget:
`D_h = sqrt(Range² − ΔZ²) − AcceptanceRadius − WorkerMineApproachSafetyMarginCm(25)`.
Predicted worst-case after 2D acceptance must be `< InteractionRange`. Impossible ΔZ → fail (no move).
One-shot corrective approach if arrival still OOR. Mining InteractionRangeCm=200 unchanged (strict 3D).

## Movement completion / request-id policy
Uses existing `UGP_MovementComponent::OnMovementResult` + Held `CommandSerial` as ActiveMineSerial. Stale serial ignored. Cancelled approach clears Held Mine.

## Interruption policy
Any new command: `ResetMineExecutor` → StopMining + release slot + clear approach. Move/Attack then proceed via existing sync. New Mine other node replaces. Duplicate same-target Mine while Approaching/Active/Mining/Waiting → idempotent accept (no second move/slot/timer).

## Mining state subscription
`OnMiningStateChanged` (dynamic): CargoFull / DepositDepleted / OutOfRange / Invalid / Idle terminal clears Mine orchestration + Held.

## CargoFull behavior
Stay in place; slot released by MiningComponent; activity CargoFull; no return/unload/Storage. New Mine while full rejected.

## DepositDepleted behavior
Stay; slot released; activity DepositDepleted; no auto next-deposit search.

## WaitingForSlot behavior
No move; FIFO promotion via MiningComponent occupancy events; activity Waiting → Mining on state change.

## Replication
Worker uses UnitBase/MobileUnit replication. Mine orchestration server-only (like Attack). Cargo/Mining replicate own state. No client mutation RPC.

## Authority policy
Begin/Stop/Mine orchestration require owner authority. Diagnostic cmds reject clients.

## Lifecycle safety
EndPlay / OwnerDied: ResetMineExecutor, unbind mining delegate, stop move, clear Held. Contract runner: next-tick stages, weak refs, reentrancy guard, world-cleanup clear.

## Diagnostics
`gp.Worker.SpawnDiagnostic|Inspect|CommandMine|CommandMove|Stop|RunContractTest`

## Contract test
Staged `UGP_WorkerContractTestRunner`: composition, rejects, immediate Mine, movement-to-Mine, interrupt, FIFO, CargoFull, deplete, EndPlay cleanup, cargo regression invoke. Local run: **Complete Failures=0**.

## Validation
`ValidateWorkerContract` (+ editor IsDataValid).

## In scope
Worker class; Mine orchestration; approach; diagnostics; contract test; docs.

## Out of scope
Storage; return-to-base; ThreatValue; Orbital/Score; repair/build; Worker Blueprint/asset; animations; UI; map; projectiles; GP-S28.

## Operator validation
**PASSED** (immediate + long-distance approach PredictedWorst=175.8 / Actual=175.6 / Range=200 → MineBegin → CargoFull; FIFO; interrupt; deplete; EndPlay; contracts Failures=0).

## Builds (finalization)
- GPEditor Win64 Development + UHT — previously **PASSED** at approach correction; **not rerun** (docs-only finalization)
- GP Win64 Development — **PASSED**
- GP Win64 Shipping — **PASSED**

## Acceptance criteria
- [x] Worker selectable/replicated mobile unit with Cargo+Mining
- [x] In-range Mine → Mining + timer; first cycle delayed
- [x] Out-of-range Mine → safe approach → BeginMining; no slot while moving
- [x] Interrupt Move clears mining slot/timer
- [x] FIFO 4+1 promote
- [x] CargoFull / DepositDepleted slot release, no auto return
- [x] EndPlay releases slots; no Editor crash
- [x] `gp.Worker.RunContractTest` Failures=0
- [x] `gp.Mining.RunContractTest` / `gp.Cargo.RunContractTest` Failures=0
- [x] GP Dev + Shipping **PASSED**

## Known limitations
- No Worker UnitDefinition / Blueprint asset
- No `GP.Capability.Mine` tag (class `AGP_Worker` + `GP.Unit.Type.Worker`)
- No auto-attack flag API (N/A); Worker still inherits Attack path if commanded
- Repair not in S27
- Contract movement wait uses time-based timeout + active-move poll (not distance Tick)

## Next canonical stage
**GP-S28 — StorageComponent + FerroniteThreatValue**

## Stop condition
Finalized on feature branch. **READY_FOR_MAIN_MERGE** when operator requests. Do **not** merge / PR / start GP-S28 from this close-out.
