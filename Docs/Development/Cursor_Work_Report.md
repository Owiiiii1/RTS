# Cursor Work Report

## Status

**MINING_NIAGARA_UE583_REGRESSION_DIAGNOSED**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A is not complete. No Content, map, Blueprint, Niagara asset, Config, uproject, or plugin file was saved or committed.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `3cebeff54e67a91b9edc09126c67e0c37b68ead2` |
| Checkpoint commit | `c3c30347f2e42a98660d1623b5f9ea87414350b7` |
| Engine | UE 5.8.3, changelist 58210709 |

## Worker Blueprint

`/Game/GrimProtocol/Blueprint/Units/BP_GP_Worker`

Parent class `AGP_Worker`. Both `L_PrototypeArena` and `L_VoxelArena_2P` place `BP_GP_Worker_C`. There is no second Worker Blueprint.

## Niagara asset

`/Niagara/DefaultAssets/Templates/Systems/RadialBurst`

Engine template. Description in the asset: "Simple omnidirectional burst with ribbon trails." It is not a project `NS_*` duplicate.

## Presentation chain

Persistent component, not a one-shot spawn.

`BP_GP_Worker` has one Niagara component, `miningEffect` (`miningEffect_GEN_VARIABLE`). Auto Activate is false. It is attached to `MiningEffectAnchor`. The system asset is RadialBurst.

Event graph, BeginPlay:

1. Bind `OnCargoVisualStateChanged` to `UpdateCargoVisual`.
2. Create the cargo dynamic material.
3. Bind `OnMiningEffectStateChanged` to custom event `UpdateMiningEffect`.
4. `Deactivate` `miningEffect`.

`UpdateMiningEffect`:

- `bEffectActive` true: `Activate` on `miningEffect` with `bReset = false`.
- `bEffectActive` false: `Deactivate` on `miningEffect`.

No `SpawnSystemAtLocation`, no dynamic component create/destroy, no per-cycle reset.

## Gameplay signal

`UGP_MiningComponent::SetMiningState` broadcasts only when the state actually changes. `AGP_Worker::HandleMiningStateChanged` sets `bEffectActive` only for `EGP_MiningState::Mining`.

`gp.Resource.RunPresentationContractTest` on `L_PrototypeArena`, 2026.09.27-18.35.27:

- Enter Mining: exactly one effect event, `bEffectActive` true. `MiningEffectOneEventOnEnter` PASS.
- `BeginMining` again on the same node returns `AlreadyMiningTarget`, state stays Mining, event count does not increase. `RepeatBeginNoExtraEffectEvent` PASS.
- Leave Mining: exactly one effect event, `bEffectActive` false. `MiningEffectOneEventOnLeave` PASS.
- Complete Failures=0.

Mining cycles stay in `Mining` until a terminal stop. C++ does not emit one effect event per cycle. The mining timer was not changed.

## Niagara lifecycle (compiled / rapid-iteration values)

`gp.Worker.DumpMiningVFX` on 30 placed Workers, 2026.09.27-18.24.50 and the later UseLoop pass. Same system for every Worker.

System `/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst`:

| Property | Value |
| --- | --- |
| Ignore System State | true |
| Run spawn / update scripts | true / true |
| Loop Behavior in `FNiagaraSystemStateData` | Once |
| Loop Duration in that struct | 0, 0 |
| Loop Count | 1 |
| Loop Delay | off, 0 |
| Inactive Response | Complete |
| Warmup | 0 seconds, 0 ticks |
| GPU emitters | false |
| Fixed bounds box | +/- 100 cm |
| Effect type | none |
| Scalability overrides | empty |

`bIgnoreSystemState` is true, so `FNiagaraSystemInstance::TickSystemState` does not apply that Once/duration-0 struct. The system update script runs.

Rapid-iteration constants on the update script:

| Constant | Value |
| --- | --- |
| `Constants.SystemState.Loop Duration` | 2 |
| `Constants.SystemState.Loop Delay` | 0 |
| `Constants.OmnidirectionalBurst.EmitterState.Loop Duration` | 1 |
| `Constants.Ribbon_Trail_Leader.EmitterState.Loop Duration` | 1 |
| `Constants.RIbbonTrailFollower.EmitterState.Loop Duration` | 1 |
| OmnidirectionalBurst Spawn Burst Loop Count Limit | 1 |
| OmnidirectionalBurst Spawn Count | 100 |
| Ribbon_Trail_Leader Spawn Burst Loop Count Limit | 1 |
| Ribbon_Trail_Leader Spawn Count | 10 |
| OmnidirectionalBurst Lifetime Min / Max | 1 / 2.25 |
| Ribbon follower Lifetime | 0.5 |

`UseLoopCountLimit` is not an overridden rapid-iteration value. Loop Behavior is not an overridden rapid-iteration value either. The compile still wrote `ENiagaraLoopBehavior::Once` into `FNiagaraSystemStateData`.

Emitters, all enabled, mode Standard, sim target CPU, bounds mode Dynamic, local space false:

- `OmnidirectionalBurst`
- `Ribbon_Trail_Leader`
- `RIbbonTrailFollower` (receives location events from the leader; no spawn burst of its own)

The Spawn Burst Instantaneous module text in UE 5.8.3 says a Loop Count Limit of N allows the burst on the first N loops only. Limit 1 means the first loop only.

## Runtime state

At map start, after BeginPlay's `Deactivate`, every dumped component was:

- Mining state Idle (`0`), effect expected false
- Niagara count 1
- IsActive false
- IsComplete true
- Execution state Complete
- Visible true, Hidden in Game false, Auto Activate false
- Parent `MiningEffectAnchor`
- Bounds collapsed to about 0.12 cm because the system is already complete

That is the idle state, not a frame taken while Mining.

`-NullRHI` cannot activate Niagara (`UNiagaraComponent::Activate` returns when the process cannot render). A rendered sample of IsComplete during an active Mining session was not taken.

## Proven cause

The effect is one finite burst. After those particles die, nothing else is spawned, while Mining stays Mining and C++ does not send another start.

Two asset facts do that together:

1. Spawn Burst Instantaneous on `OmnidirectionalBurst` and `Ribbon_Trail_Leader` has Loop Count Limit 1. Later emitter loops do not burst. This is failure mode D, and it is the direct reason the spray does not continue.
2. The compiled system loop behavior is Once, and the script loop duration constant is 2 seconds. The system is not an infinite loop. This is failure mode A. `bIgnoreSystemState` means the update script owns that, not the C++ tick helper.

Not the cause:

- B as a separate bug. Emitter loop duration is 1 second, but the burst limit already stops spawning after the first loop.
- C. Inactive Response Complete is on the struct that `TickSystemState` ignores while `bIgnoreSystemState` is true. Leaving Mining still deactivates the component from the Blueprint.
- E. All three emitters are CPU. There is no effect type and no scalability override. The tiny bounds in the dump are the completed idle system.
- F. `Activate` uses `bReset = false`. BeginPlay has already deactivated the component, so the first Mining `true` still starts it. `Activate(false)` only skips work when the instance is already Active. It is not what ends the first burst.
- G. No effect-type scalability.
- H. No Niagara C++ change in 5.8.3 was identified as the bug. 5.8.1 is not installed here, so the engine template was not diffed against 5.8.1. The installed 5.8.3 template is a one-shot burst with Loop Count Limit 1. That matches the freeze after the engine update. It is not a proven engine code regression.

## Fix

Do not edit the engine template in place. An engine update would replace it.

Operator, in the Unreal editor:

1. Content Browser, show engine content. Duplicate `/Niagara/DefaultAssets/Templates/Systems/RadialBurst` to `/Game/GrimProtocol/VFX/NS_GP_WorkerMining`. Do not save RadialBurst.
2. Open `NS_GP_WorkerMining`.
3. System stack, module **System State**:
   - Loop Behavior = **Infinite**
   - Loop Duration = **2** (the current constant; keep it positive so each loop has a period)
   - Inactive Response can stay **Complete**
4. Emitter **OmnidirectionalBurst**, module **Spawn Burst Instantaneous**:
   - Turn **Use Loop Count Limit** off if that checkbox is on.
   - If only **Loop Count Limit** is shown, it is **1** today. Set it to **0** and scrub the Niagara preview past 3 seconds. If a second burst does not appear, set Loop Count Limit to **100000**.
5. Emitter **Ribbon_Trail_Leader**, same Spawn Burst Instantaneous change. The follower has no burst of its own.
6. Each emitter, module **Emitter State**:
   - Life Cycle Mode = **System**
   - If Life Cycle Mode stays Self, set Loop Behavior = **Infinite** and leave Loop Duration at **1**
7. Open `BP_GP_Worker`. Select component `miningEffect`. Set the Niagara system to `NS_GP_WorkerMining`. Leave Auto Activate off. Leave the graph as it is: Mining true calls `Activate` with Reset false, false calls `Deactivate`.

C++ was not changed to restart the effect every mining cycle. No Niagara asset path was hardcoded into gameplay.

## Tests / build

| Step | Result |
| --- | --- |
| GPEditor Win64 Development | Succeeded |
| `gp.Resource.RunPresentationContractTest` | Failures=0, including one event on enter, no extra event while Mining, one event on leave |
| `gp.Worker.RunContractTest` | Failures=0 |
| `gp.Mining.RunContractTest` | Failures=0 |
| `gp.Resource.RunDiagnosticScenarioContractTest` | Failures=0 |
| `gp.Worker.DumpMiningVFX` | 30 Workers, system values above |

## Files changed

- `GP/Source/GPRuntime/GPRuntime.Build.cs` — non-shipping Niagara dependency and internal include, for the diagnostic only
- `GP/Source/GPRuntime/Private/Debug/GPWorkerMiningVFXDump.cpp` — `gp.Worker.DumpMiningVFX`
- `GP/Source/GPRuntime/Private/Units/GPWorker.cpp` — presentation contract counts one effect event per Mining enter and leave
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not modified and not committed: `GP/Content/`, maps, Niagara assets, Blueprint assets, `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.
