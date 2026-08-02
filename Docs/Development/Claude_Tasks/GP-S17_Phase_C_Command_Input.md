# GP-S17 Phase C — Command Input
(RMB → local BuildSmartCommand — implementation)

## Status
**Status: CODE_DONE_OPERATOR_VALIDATED**

Phase C local RMB command input **works**.
`BuildSmartCommand` is invoked through a real PlayerController caller.
Request-content validation completed for **operator-checked** cases (matrix below).
Resource Mine validated via temporary `AGP_UnitBase` Blueprint with `GP.Resource.Node`; temporary asset **removed**; map **not** saved.
Request remains **local-only**: **no** RPC, **no** execution, **no** movement.
Does **not** start full GP-S18 / GP-S19 / server submission.

---

## 1. Input ownership (canonical)

| Layer | Responsibility |
| --- | --- |
| `AGP_PlayerController` | Soft-load IA/IMC; Enhanced Input bind; cursor deproject/trace; call `BuildSmartCommand`; one-shot diagnostic log |
| `UGP_CommandComponent` | Build request only — **unchanged** semantics |

CommandComponent has **no** access to Enhanced Input, mouse, cursor trace, or IMCs.

---

## 2. Assets (created)

| Item | Value |
| --- | --- |
| Creation method | `Tools/CreateCommandInputAssets.py` via `UnrealEditor-Cmd -ExecutePythonScript` |
| IA | `/Game/GrimProtocol/Input/Commands/IA_Command` — Boolean |
| IMC | `/Game/GrimProtocol/Input/Commands/IMC_GP_Commands` |
| Disk | `GP/Content/GrimProtocol/Input/Commands/IA_Command.uasset` |
| Disk | `GP/Content/GrimProtocol/Input/Commands/IMC_GP_Commands.uasset` |
| Mapping property | **`DefaultKeyMappings`** (UE 5.8) |
| Mapping | RightMouseButton → `IA_Command` (count = 1) |
| Registration priority | **120** |
| Reload-verified | YES — `DefaultKeyMappings count=1`, action=`IA_Command`, key=`RightMouseButton` |

No Blueprint wrapper. No Shift chord in IMC.

---

## 3. Context lifecycle (implemented)

Parallel to camera/selection on `AGP_PlayerController` only:

| Step | Method |
| --- | --- |
| Soft paths | Constructor → `CommandMappingContext` / `CommandAction` |
| Load + bind | `SetupInputComponent` → `LoadCommandInputAssets` + `BindCommandInputActions`; `bCommandActionBindingInstalled` |
| Add context | Local `BeginPlayingState` → `InitializeCommandInput` (priority 120) |
| Duplicate guard | `bCommandMappingContextAdded` |
| Local gate | `IsLocalController()` — non-local never adds |
| Remove | `EndPlay` → `RemoveCommandInputMapping` + clear loaded ptrs |

No alternate input bootstrap.

---

## 4. Binding / handler

```cpp
EnhancedInput.BindAction(
    LoadedCommandAction,
    ETriggerEvent::Started,
    this,
    &AGP_PlayerController::OnCommandInputStarted);
```

| Item | Value |
| --- | --- |
| Handler | `OnCommandInputStarted` |
| Event | `ETriggerEvent::Started` only |
| Local guard | `IsLocalController()` + `CommandComponent != nullptr` |

---

## 5. Trace / queue / build

Reuse of selection click path:

1. `GetMousePosition` → `DeprojectScreenPositionToWorld`
2. `LineTraceSingleByChannel(ECC_Visibility)`, simple, distance `SelectionTraceDistance` (`1e6`)
3. Ignore possessed pawn
4. Miss → silent return (no `BuildSmartCommand`)
5. Hit → `TargetActor` / `TargetLocation = ImpactPoint`
6. `bQueue = IsShiftModifierDown()`
7. Local `FGP_CommandRequest` + `CommandComponent->BuildSmartCommand(...)`
8. Failure → silent return
9. Success → one diagnostic log; **no** RPC / execution / stored request

---

## 6. Diagnostic log

| Item | Value |
| --- | --- |
| Category | `LogGPCommandInput` (`DEFINE_LOG_CATEGORY_STATIC` in `GPPlayerController.cpp`) |
| Format | `GP CommandInput: Tag=… Units=… TargetActor=… Loc=… Queue=true\|false LocalTeam=… NetMode=… Role=…` |
| Frequency | One log per successful build only |

No LastBuiltRequest / delegate / replicated state / screen messages.

---

## 7. Builds

| Target | Result |
| --- | --- |
| GPEditor Win64 Development | **PASSED** (UHT via compile path) |
| GP Win64 Development | **PASSED** |
| GP Win64 Shipping | **PASSED** |

---

## 8. Operator validation

| Case | Result |
| --- | --- |
| Standalone PIE | **PASS** |
| No selection + RMB | **PASS** (no command log) |
| Single friendly + RMB ground → Move | **PASS** |
| Multi select + RMB ground → Move | **PASS** |
| Shift + RMB → Queue=true | **PASS** |
| RMB without Shift → Queue=false | **PASS** |
| Enemy UnitBase → Attack | **PASS** |
| Friendly UnitBase target → Move (TargetActor cleared) | **PASS** |
| Neutral / unassigned UnitBase → speculative Attack | **NOT AVAILABLE** |
| Resource.Node UnitBase → Mine | **PASS** (temporary BP with `GP.Resource.Node`; removed after) |
| Unknown non-unit actor → Move fallback | **NOT AVAILABLE** |
| 2P Listen Server isolation | **VALIDATION_PENDING** |
| One log per click | **PASS** |
| RMB hold spam | **NONE** |
| Unexpected unit movement | **NONE** |
| RPC / network command effect | **NONE** |
| LMB selection regression | **NONE** |
| Marquee regression | **NONE** |
| MMB camera regression | **NONE** |
| Control groups regression | **NONE** |
| Unexpected log errors/warnings | **NONE** |
| Temporary `BP_TestResourceNode` residual | **NONE** (removed) |
| Temporary map changes saved | **NO** |
| Residual test assets | **NONE** |

---

## Stop condition
**CODE_DONE_OPERATOR_VALIDATED.**
Phase C complete. Next stage = separate analysis checkpoint for server submission / RPC.
Do **not** start RPC / execution / movement / GP-S18 / GP-S19 from this close-out.
