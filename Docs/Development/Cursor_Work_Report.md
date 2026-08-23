# Cursor Work Report

## Status

**WORKER_COMMAND_INTENT_FINALIZED_READY_TO_MERGE**

Operator PASS received. Tech-lead pre-finalization SHAs verified. Gameplay semantics were not changed during finalization.

## Operator PASS

Operator confirmed in PIE that the new Worker mining/haul command-intent semantics work correctly:

- explicit Mine on a ResourceNode is a mining cycle (including cargo-already-full)
- explicit friendly MainBase is a one-shot deposit and does not restore the old Mine
- explicit Move / Stop / Mine replace WaitingForDropOff without stale haul resurrection

## Branch / base / head

- Branch: `fix/gp-worker-command-intent`
- Base: `origin/main` @ `aa84960c57bb4ee4180de80e44a49caece413549`
- Implementation commit: `bd2f0af1b6db4bcaf7c781bcb22bb3858c1ff611`
- Pre-finalization head (tech-lead check): `91616b8318217ab01b7e52706a66eb0a57b0085d`
- Finalization head: `d38578b5618286c26fa14327201dd718bc3d5100`
- Ahead of `origin/main`: implementation + report + test-harness isolation
- Behind `origin/main`: **0**

## Factual root cause

Worker command intent was split across multiple SoTs instead of one unambiguous assignment:

1. Held command (`Command_Mine` + `TargetActor` = ResourceNode, or `Command_Move` with `TargetActor` forced null)
2. Executor `MineTarget` / `LastMineDepositForHaul`
3. Haul `LastHaulDeposit` + `bShouldReturnToDepositAfterHaul` + haul serial
4. MiningComponent current node (occupancy/execution only)

Two concrete gameplay defects followed:

1. **Explicit `Mine(ResourceNode)` while cargo was already full was rejected** (`MineRejected Reason=CargoFull`) unless a haul chain for that deposit was already active. A valid explicit Mine could not assign the node and enter haul/deposit.
2. **Explicit click on friendly MainBase was not a deposit command.** Smart-command + Move validation stripped `TargetActor` to null, so the Worker received a ground Move. Cargo was never deposited; previous Mine/return-to-deposit state could survive incorrectly.

`WaitingForDropOff` replacement already reset haul/wait via `ResetMineExecutorForReplacement` → `ResetHaulExecutor`. The remaining hole was Mine-full-cargo reject and Base-as-Move.

## Final Worker semantics

No new gameplay tag. Two intents are distinguished by held command.

### Mining cycle — explicit `Mine(ResourceNode)`

- Always accepted for a valid node. CargoFull is **not** a reject.
- Held Mine + `MineTarget` / `LastMineDepositForHaul` / search anchor own the assignment.
- If cargo has space: approach and mine until full.
- If cargo is already full: accept, keep/set assignment, start automatic haul with return-to-deposit armed.
- After successful unload: return to the assigned node and continue mining.

### One-shot deposit — explicit `Move` whose `TargetActor` is a friendly `AGP_MainBase`

- Smart-command and server validate keep that `TargetActor`.
- Replacement cancels prior mine/haul/wait.
- Haul starts with return-to-deposit **false** and the clicked MainBase.
- After unload (or empty-cargo arrival): matching held Move is cleared; Worker idles at base. Old Mine is not restored.

Generic Move (null target) and Stop still fully replace orchestration. Stale wait/haul callbacks cannot resume: serial/state are zeroed and wake handlers require `WaitingForDropOff && ActiveHaulSerial != 0`.

## Exact changed files (branch vs `origin/main`)

Gameplay / dispatch:

- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Private/Command/GPCommandComponent.cpp`

Contracts:

- `GP/Source/GPRuntime/Public/Units/GPWorker.h` (contract runner UCLASS only)
- `GP/Source/GPRuntime/Private/Debug/GPWorkerCommandIntentContractTest.cpp` (new)
- `GP/Source/GPRuntime/Private/Debug/GPMineReassignmentHaulContractTest.cpp` (full-cargo explicit Mine now expects accept)
- `GP/Source/GPRuntime/Private/Debug/GPContractTestCoordinator.cpp` (finalization: neutralize authored combat `AGP_Unit` the same way arena turrets already were)

Docs:

- `Docs/Development/Cursor_Work_Report.md`

## Focused contract results (post-finalization, `L_PrototypeArena` `-game -unattended`)

| Command | Result |
| --- | --- |
| `gp.Worker.RunCommandIntentContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Resource.RunMineReassignmentHaulContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Resource.RunDepletionReassignmentContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Resource.RunDropOffResilienceContractTest` | **Complete Failures=0 Cancelled=None** |
| `gp.Worker.RunContractTest` | **Complete Failures=0** |
| `gp.Worker.RunHaulingContractTest` | **Complete Failures=0** after test-harness isolation (see below) |

## `gp.Worker.RunHaulingContractTest`

Previous operator-gate run failed with Failures=15 because map-authored `BP_GP_SalvageWalkerLONGRAGE` auto-acquired and killed `GP_DiagWorker` mid-haul (`OwnerDied`). First cargo-full haul/drop-off/return assertions had already PASS. This was **not** a command-intent gameplay regression.

Finalization isolation (test harness only, no Content/map, no production combat change):

- Existing contract coordinator already set authored **turrets** to `TeamId=-1` and refreshed auto-acquire at `TryAcquire`.
- The same neutralize path now also covers authored combat `AGP_Unit` (Salvage Walker) already in the world when a contract acquires the token.
- Contract-spawned combat actors are created **after** `TryAcquire` and are unaffected.
- `#if !UE_BUILD_SHIPPING` only.

After that harness change, `gp.Worker.RunHaulingContractTest` completed **Failures=0**. Production Threat / auto-acquire semantics were not altered.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** |
| `GP Win64 Development` | **Succeeded** (`GP.exe`) |
| `GP Win64 Shipping` | **Succeeded** (`GP-Win64-Shipping.exe`) |

## Final diff audit vs `origin/main`

- Branch is **not behind** `origin/main` (behind by 0).
- Committed path set is only C++ / contract / docs listed above.
- **No** Blueprint/UI authored assets.
- **No** Content.
- **No** Config.
- **No** maps / DataAssets.
- **No** Tools.
- **No** generated/binary files in the branch diff.

## Protected-file audit

Local dirty/untracked operator work remains unstaged and was not restored/cleaned/stashed:

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
- `GP/GP.uproject` (local dirty; not committed)

Authored Blueprint / map / material / config work was **not** touched by this slice.

## Status

**WORKER_COMMAND_INTENT_FINALIZED_READY_TO_MERGE**
