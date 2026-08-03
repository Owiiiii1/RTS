# GP-S18 — Unit Layer
(Unit-side Held Command ownership after GP-S17 delivery — analysis)

## Status
**Status: CODE_DONE_NETWORK_VALIDATED**

**GP-S18 Status: `DONE_WITH_EXECUTORS_DEFERRED`**

Held-command layer implemented and operator-validated on `feature/gp-s18-unit-layer-implementation` (base = `main` `8b64405`).

Validated pipeline:

```text
AGP_UnitBase::ReceiveCommand
→ UGP_UnitCommandComponent::HandleCommand
→ HeldAccepted / HeldReplaced / QueueDeferred
```

**Held Command does not mean executing.** Explicitly remaining deferred / absent:

- no movement exists
- no routing exists
- no executor exists
- no queue exists
- no completion/cancel callbacks exist
- no AI / NavMesh / GAS execution

Depends on: GP-S17 `DONE_WITH_EXECUTION_DEFERRED`.

---

## 1. Current architecture inventory

### Unit hierarchy (code)

| Class | Base | Specifiers | Role |
| --- | --- | --- | --- |
| `AGP_UnitBase` | `APawn` | Abstract, Blueprintable | TeamId + CapabilityTags + `ReceiveCommand` (log-only) |
| `AGP_Unit` | `AGP_UnitBase` | Blueprintable | Concrete placeable: capsule root + cylinder mesh |

**Absent:** `AGP_MobileUnit`, `AGP_BuildingBase`, `AGP_Worker`, building pawns.

### Concrete map unit

| Fact | State |
| --- | --- |
| Placeable class | `AGP_Unit` (C++ placeable; operator-validated; **map not saved**; no BP content) |
| GameMode DefaultPawn | `AGP_CameraPawn` — does **not** spawn units |
| Content unit assets | **None** |

### Components / identity on units

| Concern | Location |
| --- | --- |
| Collision / root | `AGP_Unit` → `UCapsuleComponent` |
| Visual | `AGP_Unit` → `UStaticMeshComponent` |
| Team | `AGP_UnitBase::TeamId` (replicated) |
| Capabilities | `AGP_UnitBase::CapabilityTags` (CDO; not replicated) |
| Selection | Capability tags + PC selection (not unit component) |
| Movement component | **Absent** |
| ASC on unit | **Absent** (ASC on `AGP_PlayerState` only) |
| Replication | UnitBase `bReplicates` + `SetReplicateMovement(true)` |

### Movement / AI / GAS

| Area | Reality |
| --- | --- |
| Movement / Nav / MoveTo | **Absent** in `GP/Source`; no AIModule/NavigationSystem in GPRuntime Build.cs |
| Custom AIController | **Absent** |
| Unit ASC / abilities | **Absent**; `UGP_UnitAttributeSet` exists in GPGASRuntime but **unattached** |
| Combat / Mining / Cargo components | **Absent** |
| State machine / order / task / executor classes | **Absent** |

### GP-S17 delivery (complete — unchanged)

```text
Input → BuildSmartCommand → Server_RequestCommand → ValidateAndNormalize
→ DispatchValidatedCommand → AGP_UnitBase::ReceiveCommand (authority + log)
```

`FGP_UnitCommand` delivery payload: `CommandTag`, `TargetLocation`, `AActor* TargetActor` (**sync-only**), `bQueue`.
**Do not change** `FGP_UnitCommand`.

### Module deps

`GPRuntime` → `GPGASRuntime` (already). UnitBase does **not** include ASC headers.
**GP-S18 Build.cs impact: NO.**

### TDD / roadmap discrepancy (document only — TDD/13 not rewritten)

| Source | Claim |
| --- | --- |
| TDD/13 GP-S18 | `AGP_UnitBase` abstract + `SetSelectionHighlight` (MID) |
| GP-S17 handoff | “GP-S18 unit layer”; Move = GP-S20–S22 via ReceiveCommand → MovementComponent |
| TDD/13 GP-S20 | `UGP_MovementComponent` (NavMesh, MoveTo) |
| TDD/13 GP-S21 | `AGP_MobileUnit` base |
| TDD/13 GP-S22 | Server_RequestCommand routing → unit.ReceiveCommand → MovementComponent |

**This Claude task follows the GP-S17 handoff:** unit-side Held Command ownership after delivery.
`SetSelectionHighlight` / fuller UnitBase ASC/death remain **deferred** (historical TDD S18 highlight / GP-S16 deferred integrations) — not the GP-S18 Held Command checkpoint.

TDD S19 (`FGP_CommandRequest`) is **already absorbed** by GP-S17 prerequisite.

GP-S20–S22 handoff mapping used by this analysis (conceptual; TDD wording differs — see §24):

| Analysis handoff | Intent |
| --- | --- |
| GP-S20 | Create movement execution foundation / `UGP_MovementComponent` |
| GP-S21 | Connect held Move → movement start + serial-aware cancellation |
| GP-S22 | Movement completion/failure callback returns serial to UnitCommandComponent |

---

## 2. Selected owner (locked)

**`UGP_UnitCommandComponent`**

| Item | Decision |
| --- | --- |
| Placement | Default subobject on **`AGP_UnitBase`** |
| Why component | Matches PC Selection/Command convention; keeps UnitBase thin |
| Why UnitBase not only Unit | Issuing list is `AGP_UnitBase*`; future UnitBase children share ReceiveCommand |

### Responsibilities

- Authority-only command acceptance
- Lifetime-safe conversion delivery payload → held payload
- Replace policy
- QueueDeferred policy
- Local command serial allocation
- Held-state diagnostics
- Future routing boundary (not implemented in GP-S18)

### Does not own

- Movement / AI / pathfinding / combat / mining / GAS / animation
- Completion detection / executor ticking

### Rejected alternatives

| Option | Why rejected |
| --- | --- |
| A. UnitBase stores inline | Monolith; mixes identity with lifecycle |
| B. Only `AGP_Unit` stores | Breaks shared UnitBase ReceiveCommand for future types |
| D. Movement/AI component | **Absent**; wrong layer |
| E. AIController | **Absent**; transport, not ownership |
| F. GAS/ASC | ASC not on units; premature |

---

## 3. Component configuration (locked)

| Item | Policy |
| --- | --- |
| Replicated | **NO** (`SetIsReplicatedByDefault(false)`) |
| Tick | **Disabled** |
| Default subobject | **YES** on `AGP_UnitBase` |
| Authority-owned state | **YES** |
| Client RPC | **NO** |
| Unit RPC | **NO** |
| Blueprint API | **NO** |

Component exists on client copies as a default subobject, but:

- does **not** accept commands
- does **not** store authoritative held state
- does **not** perform processing
- does **not** replicate command state

Network entry remains **only** PlayerController server RPC.

---

## 4. Stored command type (locked)

```cpp
struct GPRUNTIME_API FGP_StoredUnitCommand
{
	FGameplayTag CommandTag;
	FVector TargetLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> TargetActor;
	bool bQueue = false;
	uint32 CommandSerial = 0;
};
```

| Item | Decision |
| --- | --- |
| Header | `GP/Source/GPRuntime/Public/Command/GPStoredUnitCommand.h` |
| Kind | Plain C++ struct |
| USTRUCT / BlueprintType | **NO** |
| Replicated / NetSerialize / generated.h | **NO** |
| Issuer list / controller / team / timestamps / executor state | **NO** |

**Why public header:** future GP-S20 MovementComponent may consume a read-only stored snapshot; avoids exposing component private implementation through UnitBase; keeps command-domain types together.

**Do not change** Phase E delivery `FGP_UnitCommand` (`AActor*` remains sync-only).

---

## 5. TargetActor lifetime (locked)

| Boundary | Representation |
| --- | --- |
| Delivery (`FGP_UnitCommand`) | `AActor* TargetActor` |
| Held (`FGP_StoredUnitCommand`) | `TWeakObjectPtr<AActor> TargetActor` |

Conversion occurs inside `HandleCommand`.

| Policy | Detail |
| --- | --- |
| Persistence | Raw pointer is **never** persisted |
| Invalid weak | May become invalid; **must not crash** |
| Attack/Mine fail/cancel | Deferred to future executor |
| Polling | **No** target-validity poll in GP-S18 |
| Tick | **No** Tick added |

---

## 6. Exact public component API (locked)

```cpp
void HandleCommand(const FGP_UnitCommand& Command);
bool HasHeldCommand() const;
const FGP_StoredUnitCommand* GetHeldCommand() const;
```

| Requirement | Policy |
| --- | --- |
| Exposure | Plain C++; **not** UFUNCTION |
| `GetHeldCommand()` | Pointer to internal held command when present; `nullptr` when absent |
| Mutability | Read-only; caller must **not** store pointer beyond immediate synchronous use |
| Mutable getter | **Forbidden** |

### Do not add public

`StartCommand`, `CompleteCommand`, `FailCommand`, `CancelCommand`, `EnqueueCommand`, `PopCommand`, executor registration, routing table.

---

## 7. Internal component state / helpers (locked)

```cpp
TOptional<FGP_StoredUnitCommand> HeldCommand;
uint32 NextCommandSerial = 1;

void ClearHeldCommand(); // private
```

`ClearHeldCommand()` is internal only in GP-S18. Used for replacement and EndPlay cleanup.
Do **not** expose manual clear publicly yet (no executor lifecycle).

---

## 8. Command serial policy (locked)

`CommandSerial` is:

- Local to one unit component
- Allocated only on authority
- Not replicated
- Not a client request sequence
- Not globally unique
- Intended for future stale callback protection

| Event | Serial |
| --- | --- |
| First accepted held command | `1` |
| Each accepted non-queued replacement | Increments |
| QueueDeferred | Does **not** consume serial |
| Rejected / non-authority | Does **not** consume serial |

Wraparound: not engineered in GP-S18; `0` remains reserved for “no valid serial”; implementation may skip zero after overflow.
Do **not** add GUIDs or network sequence fields.

---

## 9. Exact HandleCommand policy (locked)

### Non-authority

- Log `RejectedAuthority`
- No state change
- No serial allocation

### `bQueue == true`

- Log `QueueDeferred`
- `HeldCommand` remains unchanged
- No serial allocation
- No executor call

Applies whether a held command exists or not.
Do **not** silently reinterpret queued command as replace.

### `bQueue == false`

1. Convert delivery payload → lifetime-safe stored payload
2. Allocate a new serial
3. If held exists → replace; log previous and new serial (`HeldReplaced`)
4. If empty → store; log `HeldAccepted`
5. Do **not** start execution

No whitelist or ownership validation is repeated here (GP-S17 validator remains the gate).

---

## 10. Replacement semantics (locked)

Replacement means **only**:

- old `HeldCommand` removed
- new `HeldCommand` stored

It does **not** mean: old executor cancelled; AI stopped; movement aborted; ability ended; command completed.

Future executor integration must introduce cancellation callbacks before replacement affects gameplay.

---

## 11. UnitBase integration (locked)

### Future UnitBase responsibilities

- Owns default subobject
- Exposes component getter
- Forwards `ReceiveCommand`

### Property style (match PC convention)

Existing PC pattern (`GPPlayerController.h`):

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Commands", meta = (AllowPrivateAccess = "true"))
TObjectPtr<UGP_CommandComponent> CommandComponent;
```

Exact proposed UnitBase match:

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Command", meta = (AllowPrivateAccess = "true"))
TObjectPtr<UGP_UnitCommandComponent> UnitCommandComponent;

UGP_UnitCommandComponent* GetUnitCommandComponent() const;
```

Note: UnitBase today uses `EditInstanceOnly` / `EditDefaultsOnly` for TeamId/CapabilityTags (identity facts). Subobject ownership follows **PC component convention**, not those Edit* identity properties.

### ReceiveCommand future behavior

1. Authority guard remains
2. Existing Phase E `Received` log remains (**do not remove/rename**)
3. Forward exactly once to `UnitCommandComponent->HandleCommand(Command)`
4. If component unexpectedly null: warning; no crash; no fallback storage in UnitBase

---

## 12. Routing policy (locked)

**GP-S18 does not implement command-specific routing.**

The component stores any command already accepted by server validation.
No switch on Move / Attack / Mine in GP-S18.

| Item | Policy |
| --- | --- |
| Future routing owner | `UGP_UnitCommandComponent` |
| GP-S18 tag routing | **None** — command-agnostic held-state shell |
| Reason | Switch with no receivers is premature |

GP-S20 introduces Move consumption; later combat/resource layers add adapters; routing decision may then live in UnitCommandComponent.

---

## 13. Executor boundary (locked)

GP-S18 creates **no**:

- executor interface (`IGP_UnitCommandExecutor` forbidden)
- delegate / event bus / Gameplay Event
- movement adapter / AI adapter / component registry

GP-S20 determines concrete Move connection after MovementComponent exists.

---

## 14. AI boundary (locked)

Current project: no movement architecture, no AIController architecture.

GP-S18:

- does not create AIController
- does not call MoveTo
- does not add NavMesh dependency
- does not possess units
- does not change AutoPossessAI

Conceptual future chain only (not implemented):

```text
UnitCommandComponent → MovementComponent → future movement transport / AI / navigation
```

---

## 15. GAS boundary (locked)

ASC is on `AGP_PlayerState` only.

GP-S18:

- does not attach ASC to units
- does not use unit AttributeSet
- does not emit Gameplay Events / activate abilities
- does not add GPGASRuntime dependency
- does not treat command state as GAS state

**Build.cs impact: NO**

---

## 16. EndPlay policy (locked)

Future component `EndPlay`:

- Clears held command
- Does not send callbacks
- Does not attempt execution cancellation
- Does not log as gameplay cancellation

Diagnostic event when held state existed:

```text
HeldCleared Reason=EndPlay
```

No tick or timer cleanup.

---

## 17. Capability checks (locked)

Do **not** repeat in UnitCommandComponent:

- team authorization
- target whitelist
- location validation
- server request normalization

Actual unit support for Move/Attack/Mine is deferred to executors.
No new capability tags.
Held ≠ executable.

---

## 18. Completion and stale callback handoff (locked)

GP-S18 does **not** expose completion APIs.

Future GP-S20+ requirement (document only):

1. Executor receives `CommandSerial`
2. Callback includes the same serial
3. UnitCommandComponent compares callback serial with current held serial
4. Stale callback cannot clear or complete a newer command

Do not implement callbacks or result enums in GP-S18.

---

## 19. Exact implementation checkpoint (locked)

Future GP-S18 code checkpoint is limited to:

1. Add `FGP_StoredUnitCommand`
2. Add `UGP_UnitCommandComponent`
3. Add component as UnitBase default subobject
4. Forward `ReceiveCommand`
5. Store one non-queued held command
6. Replace existing held command
7. Reject/defer `bQueue=true` without state mutation
8. Allocate local serial
9. Expose read-only held state
10. Clear held state on EndPlay
11. Add diagnostic logs
12. Build / UHT
13. Standalone and 2P validation

**No execution.**

---

## 20. Expected C++ files (locked)

### New

| File | Role |
| --- | --- |
| `GP/Source/GPRuntime/Public/Command/GPStoredUnitCommand.h` | Stored payload (command-domain type) |
| `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h` | Per-unit component |
| `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp` | Implementation |

Placement rationale: component is per-unit → `Units/`; payload types remain under `Command/`.

### Modified

| File | Role |
| --- | --- |
| `GP/Source/GPRuntime/Public/Units/GPUnitBase.h` | Subobject, getter, ReceiveCommand forward |
| `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp` | CreateDefaultSubobject + forward |

### Expected unchanged

`GPUnit.h/.cpp`, `GPUnitCommand.h`, `GPCommandComponent`, PlayerController, Build.cs, gameplay tags.

### Build.cs impact
**NO**

---

## 21. Logging contract (locked)

Category: `LogGPUnitCommandState`

### Events (terminology)

| Event | When |
| --- | --- |
| `HeldAccepted` | First held store (`bQueue=false`, was empty) |
| `HeldReplaced` | Non-queued replace of existing held |
| `QueueDeferred` | `bQueue=true` — held unchanged |
| `RejectedAuthority` | Non-authority HandleCommand |
| `HeldCleared` | EndPlay clear when held existed |

Do **not** log: Executed, Started, Completed, Failed, Cancelled.
No tick logs. No on-screen logs.

### Exact formats

```text
GP UnitCommandState HeldAccepted: Unit=... Serial=... Tag=... TargetActor=... Loc=... Queue=false Role=Authority NetMode=...
GP UnitCommandState HeldReplaced: Unit=... PreviousSerial=... NewSerial=... PreviousTag=... NewTag=... Role=Authority NetMode=...
GP UnitCommandState QueueDeferred: Unit=... Tag=... HeldSerial=<serial/none> Role=Authority NetMode=...
GP UnitCommandState RejectedAuthority: Unit=... Tag=... Role=... NetMode=...
GP UnitCommandState HeldCleared: Unit=... Serial=... Reason=EndPlay Role=... NetMode=...
```

---

## 22. Operator validation (Listen Server / 2P)

### Host Team 1

| Case | Evidence | Result |
| --- | --- | --- |
| First Move | `HeldAccepted Serial=1 Tag=GP.Command.Move Queue=false Role=Authority NetMode=ListenServer` | **PASS** — first held; serial starts at 1; authority-only; no movement |
| Second Move | `HeldReplaced PreviousSerial=1 NewSerial=2 PreviousTag=Move NewTag=Move` | **PASS** — replace; serial increment; old held replaced |
| Multi-selection | Unit A continued `2→3`; Unit B independent `Serial=1` | **PASS** — per-unit independent state; no shared serial |
| Attack replacement | Unit A `3→4` Move→Attack; Unit B `1→2` Move→Attack | **PASS** — command-agnostic storage; Attack replaces Move; no Attack execution |

### Remote client Team 2

| Case | Evidence | Result |
| --- | --- | --- |
| Client input | `LocalTeam=2 NetMode=Client Role=AutonomousProxy` | **PASS** |
| Server acceptance | `PC=GP_PlayerController_1 Team=2 NetMode=ListenServer` | **PASS** |
| Authority receiver | `Team=2 Role=Authority NetMode=ListenServer` | **PASS** |
| Held state | `HeldAccepted Serial=1` then `HeldReplaced 1→2` | **PASS** — remote submission; server-only held processing; no client duplicate; Team 2 isolated |
| QueueDeferred | `Queue=true HeldSerial=2 Role=Authority NetMode=ListenServer` | **PASS** — held unchanged; no serial consumed; no silent replace; no queue execution |
| Remote Attack after queue | `HeldReplaced PreviousSerial=2 NewSerial=3 Move→Attack` | **PASS** — queued did not consume serial; next non-queued got 3; Attack held; no execution |

### Validation summary

| Item | Result |
| --- | --- |
| Host HeldAccepted | **PASS** |
| Host HeldReplaced | **PASS** |
| Independent per-unit serials | **PASS** |
| Multi-unit held state | **PASS** |
| Remote-client held state | **PASS** |
| Server-authoritative processing | **PASS** |
| QueueDeferred | **PASS** |
| Queue does not replace state | **PASS** |
| Queue does not consume serial | **PASS** |
| Attack held state | **PASS** |
| Target is not command-state owner | **PASS** |
| Team isolation | **PASS** |
| Duplicate state handling | **NONE** |
| Unexpected movement | **NONE** |
| AI / NavMesh / GAS execution | **NONE** |
| Runtime warnings/errors | **NONE** in supplied excerpts |
| Weak target destruction | **NOT CAPTURED** |
| EndPlay HeldCleared log | **NOT CAPTURED** |
| Mine held state | **DEFERRED** |

Do **not** invent PASS for uncaptured cases.

---

## 23. GP-S18 completion status

**GP-S18 Status: `DONE_WITH_EXECUTORS_DEFERRED`**

Completion means:

- UnitCommandComponent exists
- Server-authoritative held state exists
- Lifetime-safe target storage exists
- Replace semantics exist
- Queue intent is explicitly deferred (`QueueDeferred`)
- Serial identity exists (per-unit, independent)
- UnitBase forwards correctly
- Host + remote-client validated

It does **not** mean: commands execute; movement exists; Attack/Mine gameplay works; queue exists; routing/executor/callbacks exist.

---

## 24. GP-S20–GP-S22 handoff

### Analysis handoff (post Held Command)

| Stage | Role |
| --- | --- |
| GP-S20 | Create movement execution foundation / component |
| GP-S21 | Connect held Move command to movement start and serial-aware cancellation |
| GP-S22 | Movement completion/failure callback returns serial to UnitCommandComponent |

### TDD/13 actual stage wording (quoted paraphrase — not rewritten)

| Stage | TDD/13 purpose |
| --- | --- |
| GP-S20 | `UGP_MovementComponent` (NavMesh, MoveTo) |
| GP-S21 | `AGP_MobileUnit` base |
| GP-S22 | Server_RequestCommand routing → unit.ReceiveCommand → MovementComponent |

**Mismatch:** TDD S22 still describes delivery routing that GP-S17 already completed; analysis handoff reassigns S21–S22 toward Move start/cancel/completion against Held Command. Do **not** rewrite TDD from this pass — resolve numbering when movement work is assigned.

---

## 25. Implementation record

### Actual files

| File | Role |
| --- | --- |
| `GP/Source/GPRuntime/Public/Command/GPStoredUnitCommand.h` | Plain `FGP_StoredUnitCommand` |
| `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h` | Component API / state |
| `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp` | HandleCommand / EndPlay / logs |
| `GP/Source/GPRuntime/Public/Units/GPUnitBase.h` | Subobject + getter |
| `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp` | CreateDefaultSubobject + forward |

### Exact APIs

```cpp
// UGP_UnitCommandComponent
void HandleCommand(const FGP_UnitCommand& Command);
bool HasHeldCommand() const;
const FGP_StoredUnitCommand* GetHeldCommand() const;

// AGP_UnitBase
UGP_UnitCommandComponent* GetUnitCommandComponent() const;
virtual void ReceiveCommand(const FGP_UnitCommand& Command); // Received log + forward
```

### Component metadata / settings

- `UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))` — matches Selection/Command components
- `PrimaryComponentTick.bCanEverTick = false`
- `SetIsReplicatedByDefault(false)`
- No UFUNCTION / RPC / replicated properties

### Serial wrap policy (implemented)

`AllocateCommandSerial()` returns current `NextCommandSerial` (starts at 1), then increments.
If post-increment equals `0`, reset to `1` (zero reserved).

### Policies implemented

| Policy | Behavior |
| --- | --- |
| Non-authority | `RejectedAuthority`; no state/serial |
| `bQueue=true` | `QueueDeferred`; held unchanged; no serial |
| `bQueue=false` empty | `HeldAccepted`; serial allocated |
| `bQueue=false` held | `HeldReplaced`; serial allocated |
| EndPlay with held | `HeldCleared Reason=EndPlay` then clear |
| Routing / execution | **NO** |
| Movement / AI / GAS | **NO** |

### Target storage

Delivery `AActor*` → held `TWeakObjectPtr<AActor>` in `HandleCommand`. No raw persistence. No validity polling.

### Validation

Host + remote-client Listen Server validation — **CODE_DONE_NETWORK_VALIDATED** (§22).
Weak target destruction / EndPlay HeldCleared / Mine — **NOT CAPTURED** or **DEFERRED**.

---

## Stop condition
**CODE_DONE_NETWORK_VALIDATED.** **GP-S18: DONE_WITH_EXECUTORS_DEFERRED.**
Commit/push `feature/gp-s18-unit-layer-implementation` only.
Do **not** merge to main from this close-out.
Do **not** start GP-S20 / Move / AI / NavMesh / GAS / routing / executor / queue / callbacks from this pass.
Do **not** rewrite TDD.
