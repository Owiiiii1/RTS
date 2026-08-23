# Cursor Work Report

## Status

**WORKER_COMMAND_INTENT_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

## Branch / base / head

- Branch: `fix/gp-worker-command-intent`
- Base: `origin/main` @ `aa84960c57bb4ee4180de80e44a49caece413549`
- Head: commit SHA recorded after push on this branch

## Established factual root cause

Worker command intent was not a single ownership SoT. Mining-cycle assignment, automatic haul, and player Move/Base/Stop were mixed across:

1. Held command (`Command_Mine` + `TargetActor` = ResourceNode, or `Command_Move` with `TargetActor` forced null)
2. Executor `MineTarget` / `LastMineDepositForHaul`
3. Haul `LastHaulDeposit` + `bShouldReturnToDepositAfterHaul` + haul serial
4. MiningComponent current node (occupancy/execution only)

Two concrete bugs followed from that split:

1. **Explicit `Mine(ResourceNode)` while cargo was already full was rejected** (`MineRejected Reason=CargoFull`) unless a haul chain for that deposit was already active. Valid explicit Mine therefore could not assign the node and enter the haul/deposit phase.
2. **Explicit click on friendly MainBase was not a deposit command.** Smart-command + Move validation stripped `TargetActor` to null, so the Worker received a ground Move. Cargo was never deposited, and any previous Mine held/return-to-deposit state could survive incorrectly depending on replacement cleanup.

`WaitingForDropOff` replacement (Move/Stop/Mine) already reset haul/wait via `ResetMineExecutorForReplacement` → `ResetHaulExecutor` (storage/register/retry unbind + serial zero). The remaining hole was Mine-full-cargo reject and Base-as-Move, not a missing unbind in Stop/Move.

## Previous state ownership

| Concern | Previous SoT |
| --- | --- |
| Player Mine assignment | Held `Command_Mine` + `TargetActor` |
| Cycle return-to-node | `bShouldReturnToDepositAfterHaul` + `LastHaulDeposit` + held Mine serial match |
| Auto-reassign after haul | Held Mine serial match even without return flag |
| Explicit Base | Not represented; friendly MainBase → Move with null target |
| Full-cargo explicit Mine | Rejected before accept |
| Drop-off wait | `HaulState == WaitingForDropOff` + `ActiveHaulSerial`; wake/retry required that pair |

## New intent / ownership semantics

No new gameplay tag. Two intents are distinguished by held command:

1. **Mining cycle** — explicit `Mine(ResourceNode)`
   - Always accepted for a valid node (CargoFull is not a reject).
   - Held Mine + `MineTarget` / `LastMineDepositForHaul` / search anchor own the assignment.
   - If cargo has space: approach and mine until full.
   - If cargo is already full: accept, keep/set assignment, `StartHaulReturnToBase(..., bReturnToDeposit=true)` immediately.
   - After successful unload: return to the assigned node and continue mining.

2. **One-shot deposit** — explicit `Move` whose `TargetActor` is a friendly `AGP_MainBase`
   - Smart-command and server validate keep that `TargetActor`.
   - `ResetMineExecutorForReplacement` cancels prior mine/haul/wait.
   - `TryStartOneShotMainBaseDeposit` starts haul with `bReturnToDeposit=false` and the clicked MainBase (no generic `RequestMove`).
   - After unload (or empty-cargo arrival): held Move matching haul serial is cleared; Worker idles at base. Old Mine is not restored.

Generic Move (null target) and Stop still fully replace orchestration. Stale wait/haul callbacks cannot resume because serial/state are zeroed and wake handlers require `WaitingForDropOff && ActiveHaulSerial != 0`.

## Exact changed files

- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Private/Command/GPCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Units/GPWorker.h` (contract runner UCLASS only)
- `GP/Source/GPRuntime/Private/Debug/GPWorkerCommandIntentContractTest.cpp` (new)
- `GP/Source/GPRuntime/Private/Debug/GPMineReassignmentHaulContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md`

## Cleanup timers / delegates / callbacks

Replacement (Stop / Move / Mine / one-shot Base) still goes through `ResetMineExecutorForReplacement` → `ResetHaulExecutor` / `ClearDropOffSubscriptionsAndTimer`:

- MainBase register/unregister drop-off wakes
- storage-changed drop-off wake
- drop-off safety retry timer
- pending next-tick haul resume serial/deposit/return flag
- mining-state delegate unbind + StopMining
- resource-registry wait-for-resource unbind
- pending movement ownership (replaced by new serial or StopMove)
- `ActiveMineSerial` / `ActiveHaulSerial` / `MineTarget` / `LastHaulDeposit` / `HaulMainBase` / `bShouldReturnToDepositAfterHaul`

`FinishHaulChain` now also clears a matching held **Move** (one-shot deposit) so leftover Move cannot resurrect work. Matching held Mine is still cleared only when the haul chain is allowed to clear held.

Wake/retry paths still no-op unless `HaulState == WaitingForDropOff` and `ActiveHaulSerial != 0`.

## Contracts and results

| Command | Result |
| --- | --- |
| `gp.Worker.RunCommandIntentContractTest` | **Complete Failures=0 Cancelled=None** (A–E: full-cargo Mine accept+return A; explicit Base breaks cycle; wait+Move no resurrect; wait+Mine B returns to B; Stop clears latent) |
| `gp.Resource.RunMineReassignmentHaulContractTest` | **Complete Failures=0 Cancelled=None** (stage 8 updated: full-cargo explicit Mine is accepted and starts cycle haul) |
| `gp.Resource.RunDepletionReassignmentContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Resource.RunDropOffResilienceContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Worker.RunContractTest` | **Complete Failures=0** |
| `gp.Worker.RunHaulingContractTest` | Complete Failures=15 — **not a command-intent regression**. Log shows map `BP_GP_SalvageWalkerLONGRAGE` killed `GP_DiagWorker` mid-haul (`OwnerDied`). First cargo-full haul/drop-off/return case had already **PASS**. Re-run in isolation reproduced walker kill. |

## GPEditor / UHT

- `GPEditor Win64 Development` **Succeeded** (UHT processed, GPRuntime linked)
- `GP Win64 Development` / `GP Win64 Shipping` **not run** (not required before operator PASS)

## Protected-file check

Diff vs `origin/main` for this slice does **not** include:

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/Content/Basic_VFX/`
- `GP/Content/GrimProtocol/Blueprint/`
- `GP/Content/GrimProtocol/Materials/`
- `GP/Content/Mixed_Magic_VFX_Pack/`
- `GP/Content/RocketThrusterExhaustFX/`
- `Tools/`
- WBP_GP_HUD / WBP_GP_LaunchContainerRow
- operator-authored unit/building Blueprints/DataAssets

Those remain local dirty/untracked and were not staged.

## Operator test now required

PIE on `L_PrototypeArena` (or equivalent navigable map), authority Worker:

1. **Full cargo + explicit Mine on node A** — command accepted (no CargoFull reject); Worker hauls; after unload returns to A and continues mining.
2. **Mine A / auto-haul then explicit click on friendly MainBase** — deposits; stays at Base; does **not** return to A; idle until next player command. Empty cargo Base click also ends at Base without restoring Mine.
3. **Storage full wait + Move X** — waiting cancelled; Move executes; later storage space does **not** resume old haul.
4. **Storage full wait + Mine B** — accepted despite full cargo; B is the cycle assignment; after unload returns to B not A.
5. **Wait/haul/mine + Stop** — latent orchestration cleared; no timer/delegate resumes old cycle.

Also confirm mixed selection: combat units clicking MainBase still Move; only Workers one-shot deposit.

## Status

**WORKER_COMMAND_INTENT_READY_FOR_OPERATOR_VALIDATION**
