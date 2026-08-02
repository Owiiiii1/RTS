# GP-S17 Phase C — Command Input
(RMB → local BuildSmartCommand — final analysis contract)

## Status
**Status: ANALYSIS_READY_IMPLEMENTATION_PENDING**

Docs-only checkpoint. **No** C++ / assets / maps / config in this pass.
Depends on: Phase B `BuildSmartCommand` (`CODE_DONE_FUNCTIONAL_VALIDATION_DEFERRED`).
Phase B **real functional validation** is performed through this Phase C caller.
Phase C does **not** send the request to the server and does **not** execute commands.
Does **not** start full GP-S18 / GP-S19.

---

## 1. Input ownership (canonical)

| Layer | Responsibility |
| --- | --- |
| `AGP_PlayerController` | Soft-load IA/IMC; Enhanced Input bind; cursor deproject/trace; call `BuildSmartCommand`; one-shot diagnostic log |
| `UGP_CommandComponent` | Build request only |

`UGP_CommandComponent` must **not** access:

- Enhanced Input subsystem
- mouse position
- cursor trace
- input mapping contexts

Matches existing camera / selection architecture. Do **not** move bindings into CommandComponent.

---

## 2. Canonical Input Action / IMC

### Input Action

| Item | Value |
| --- | --- |
| Name | `IA_Command` |
| Soft path | `/Game/GrimProtocol/Input/Commands/IA_Command.IA_Command` |
| Content path | `GP/Content/GrimProtocol/Input/Commands/IA_Command.uasset` |
| Value type | Boolean / Digital |
| Key | Right Mouse Button |
| Trigger | `ETriggerEvent::Started` |

**Rationale:** one command per press; no spam while RMB is held.

### Input Mapping Context

| Item | Value |
| --- | --- |
| Name | `IMC_GP_Commands` |
| Soft path | `/Game/GrimProtocol/Input/Commands/IMC_GP_Commands.IMC_GP_Commands` |
| Content path | `GP/Content/GrimProtocol/Input/Commands/IMC_GP_Commands.uasset` |
| Priority | **120** (Camera **100**, Selection **110**) |
| Mapping property | **`DefaultKeyMappings`** (UE 5.8; not deprecated `mappings`) |
| Mapping | RMB → `IA_Command` |

Do **not** add RMB to Camera or Selection IMCs.

---

## 3. Mapping-context lifecycle (confirmed existing pattern)

Canonical bootstrap is **only** `AGP_PlayerController` — same pattern as camera/selection. **No** alternate input bootstrap / loader.

| Step | Existing camera/selection method | Phase C parallel |
| --- | --- | --- |
| Soft paths | Constructor assigns `TSoftObjectPtr` soft paths | `CommandMappingContext`, `CommandAction` soft paths |
| Load + bind | `SetupInputComponent` → `Load*InputAssets` + `Bind*InputActions`; one-shot bind flags (`b*BindingInstalled`) | `LoadCommandInputAssets` + `BindCommandInputActions`; `bCommandActionBindingInstalled` |
| Add context | Local-only `BeginPlayingState` → `InitializeCameraInput` / `InitializeSelectionInput` | `InitializeCommandInput` from same local `BeginPlayingState` |
| Local gate | `IsLocalController()` before add | Same — never add for server non-local PC |
| Duplicate guard | `bCameraMappingContextAdded` / `bSelectionMappingContextAdded` | `bCommandMappingContextAdded` |
| Remove | `EndPlay` → `Remove*InputMapping` + clear loaded ptrs | `RemoveCommandInputMapping` + clear |

Confirmed call sites today:

- Soft defaults: constructor
- Bind: `SetupInputComponent`
- Add IMC: `BeginPlayingState` when `IsLocalController()` (after cursor/`GameAndUI` setup)
- Remove: `EndPlay`

Priority 120 must not alter Camera/Selection bindings (different keys; higher priority only among overlapping actions).

---

## 4. Input conflict policy

| Input | Role | Phase C |
| --- | --- | --- |
| LMB | Selection / marquee | Unchanged |
| MMB | Camera rotate toggle | Unchanged |
| RMB | Command | **New** (`IA_Command`) |
| WASD / wheel | Camera pan/zoom | Unchanged |
| Digits | Control groups | Unchanged |
| Esc / cancel | — | **Out of scope** |
| Drag-command | — | **Out of scope** |

Existing RMB usage: **none**. Camera rotate remains MMB.

---

## 5. Cursor trace (canonical)

1. Local PlayerController only.
2. `GetMousePosition` + `DeprojectScreenPositionToWorld` (existing selection style).
3. `LineTraceSingleByChannel`.
4. Channel: `ECC_Visibility`.
5. Complexity: simple (`false`) — existing project policy.
6. Distance: `1e6` (`SelectionTraceDistance` / shared constant).
7. Ignore: possessed pawn if present.
8. **Hit:** `TargetActor = Hit.GetActor()`, `TargetLocation = Hit.ImpactPoint`.
9. **Miss:** silent no-op; **do not** call `BuildSmartCommand`.

**Not added:** ground-plane fallback; new collision channel; config changes; custom trace subsystem.

---

## 6. UI policy (Phase C limit)

- No CommonUI / input-capture gate in Phase C.
- Command input behaves like current selection input (no cursor-over-UI helper today).
- Modal / UI blocking = future integration checkpoint.
- No new UI abstraction.

This is **not** final production behavior for RMB over UI.

---

## 7. Queue modifier

| Item | Policy |
| --- | --- |
| Source | Existing `IsShiftModifierDown()` |
| Value | `bQueue = IsShiftModifierDown()` |
| New Shift IA | **No** |
| Queue execution | **Deferred** — Phase C only passes intent into request + diagnostic log |

---

## 8. Exact call flow

```text
IA_Command Started
→ verify local controller
→ cursor Visibility trace
→ trace miss: return
→ obtain TargetActor / TargetLocation
→ bQueue = IsShiftModifierDown()
→ CommandComponent->BuildSmartCommand(...)
→ failure: return
→ log built request
→ stop
```

After successful build there is **no**:

- RPC
- server validation
- dispatch
- movement
- AI
- execution

---

## 9. Request diagnostic

One diagnostic entry per **successful** RMB build:

```text
GP CommandInput: Tag=<tag> Units=<count> TargetActor=<name|None> Loc=<vector> Queue=<0|1> LocalTeam=<id> NetMode=<mode> Role=<role>
```

| Rule | Value |
| --- | --- |
| `LastBuiltRequest` | **NO** |
| Delegate | **NO** |
| Replicated state | **NO** |
| Per-frame / hold spam | **NO** |
| Normal failures as Warning | **NO** |
| Host/client distinguish | `LocalTeam` + `NetMode`/`Role` in same line |

### Log category (confirmed)

Project has **no** named `LogGP` (or similar) category; PlayerController diagnostics use `LogTemp` (`GP ControlGroup:`, selection logs).

**Phase C:** `UE_LOG(LogTemp, Log, TEXT("GP CommandInput: ..."))` — matches existing PC style. Do **not** invent a scattered new category without a project-wide log header; a dedicated category is optional later, not required for Phase C.

---

## 10. Failure policy

Silent no-op when:

- non-local controller
- cursor trace miss
- no selection
- missing PlayerState / team
- missing CommandComponent
- `BuildSmartCommand == false`

Allowed: Verbose / VeryVerbose developer diagnostics only.

**Forbidden:** on-screen messages; warning spam; Error for normal empty selection.

---

## 11. Functional validation matrix

| # | Case | Expected |
| --- | --- | --- |
| 1 | No selection + RMB ground | No command log |
| 2 | One friendly selected + RMB ground | `Command_Move`; Units=1; TargetActor null; unit does **not** move |
| 3 | Multiple selected + RMB ground | `Command_Move`; Units = selection count |
| 4 | Enemy `UnitBase` | `Command_Attack`; target actor kept |
| 5 | Friendly `UnitBase` | `Command_Move`; TargetActor null |
| 6 | Neutral / unassigned `UnitBase` | Speculative `Command_Attack` |
| 7 | `UnitBase` with `GP.Resource.Node` | `Command_Mine` |
| 8 | Unknown non-unit actor | `Command_Move`; TargetActor null |
| 9 | Shift + RMB | Queue=true |
| 10 | RMB without Shift | Queue=false |
| 11 | 2P Listen Server | Host log = host selection only; client = client only; LocalTeam + net context distinguishable |
| 12 | All cases | No RPC; no unit movement; no command execution |

Regression: camera / selection / marquee / control groups / MMB rotate unchanged.

---

## 12. Exact next implementation checkpoint

**GP-S17 Phase C — local RMB command caller**

### Allowed

- Create `IA_Command` + `IMC_GP_Commands` (`DefaultKeyMappings`, RMB)
- Wire into existing local-controller input lifecycle on `AGP_PlayerController`
- Visibility cursor trace
- Call `BuildSmartCommand`
- One diagnostic log entry
- Docs; builds/UHT; operator validation

### Forbidden

- RPC / server validation / command dispatch
- Movement / AI / NavMesh / unit receiver
- Queue execution
- `LastBuiltRequest` / delegates / replicated request state
- Production UI feedback / CommonUI redesign
- Collision / config / Build.cs / `.uproject` / native tags / CommandComponent API changes / request struct changes

### Expected C++

- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`

No new input loader. Soft refs + Initialize/Remove/Load/Bind parallel to selection.

### Expected assets

- `GP/Content/GrimProtocol/Input/Commands/IA_Command.uasset`
- `GP/Content/GrimProtocol/Input/Commands/IMC_GP_Commands.uasset`

### Asset creation policy

Choose existing safe workflow at implementation time:

- approved Unreal Editor automation, **or**
- operator creation in Editor

UE 5.8: mapping must live in **`DefaultKeyMappings`**; open/verify asset after create; file existence alone is insufficient.

**Asset creation is not performed in this analysis checkpoint.**

---

## Stop condition

**ANALYSIS_READY_IMPLEMENTATION_PENDING.**

Await Phase C implementation assignment.
Do **not** create IA/IMC, bindings, RPC, or execution from this analysis.
Do **not** merge to main from this analysis branch alone.
