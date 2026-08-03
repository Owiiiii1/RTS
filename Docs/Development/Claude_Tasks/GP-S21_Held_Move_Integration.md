# GP-S21 — Held Move Integration
(Wire Held Command transitions to UGP_MovementComponent — analysis)

## Status
**Status: ANALYSIS_READY_IMPLEMENTATION_PENDING**

Docs-only. **No** C++ / assets / callbacks / GP-S22 completion / Nav / AI in this pass.
Depends on: GP-S18 `DONE_WITH_EXECUTORS_DEFERRED`, GP-S20 `DONE_WITH_COMMAND_INTEGRATION_DEFERRED`.

---

## 1. Current architecture

### Command pipeline (linked)

```text
Input → BuildSmartCommand → Server validation → Dispatch
→ AGP_UnitBase::ReceiveCommand
→ UGP_UnitCommandComponent::HandleCommand
→ HeldAccepted / HeldReplaced / QueueDeferred
```

### Movement pipeline (isolated)

```text
AGP_MobileUnit → UGP_MovementComponent
→ RequestMove / StopMove → physical server movement
```

**No link today.** RMB Holds Move; movement starts only via non-shipping `gp.Movement.Test`.

### Facts

| Item | State |
| --- | --- |
| Hierarchy | `UnitBase` ← `MobileUnit` (+ Movement) ← `Unit` |
| Held owner | `UGP_UnitCommandComponent` on UnitBase |
| Movement owner | `UGP_MovementComponent` on MobileUnit only |
| Tags | `FGPGameplayTags::Command_Move` / `Command_Attack` / `Command_Mine` (native; GPRuntime already uses `==`) |
| Serial allocator | `NextCommandSerial` starts 1; QueueDeferred does not allocate; zero reserved |
| After MoveReached | Movement idle (`ActiveMoveSerial=0`); **Held Move remains** until next command |
| Build.cs | GPRuntime → GPGASRuntime already; no new deps |

---

## 2. Canonical GP-S21 goal

| In scope | Out of scope |
| --- | --- |
| Held Move accepted/replaced → `RequestMove` | Movement completion callback |
| Non-Move replace of active Move → stop movement | Held clear after reach |
| QueueDeferred → no held/movement change | Stale completion / result enum |
| Authority-only sync | NavMesh / AIController / Attack/Mine execution / queue |

---

## 3. Selected integration owner (locked)

**Option A — `UGP_UnitCommandComponent` after held-state mutation**

```text
HandleCommand (authority, non-queued)
→ capture previous Held (optional)
→ store new Held + serial
→ SynchronizeMovementWithHeldCommand(Previous)
→ HeldAccepted / HeldReplaced logs
```

Sync path:

```text
Cast owner → AGP_MobileUnit
→ GetUnitMovementComponent()
→ RequestMove / StopMove(...)
```

| Rejected | Why |
| --- | --- |
| B. UnitBase after HandleCommand | Movement assumption on all UnitBase |
| C. MobileUnit override ReceiveCommand | Splits held transition from execution; Base still logs Received before forward |
| D. Extra router component | Premature abstraction |
| E. Delegates | GP-S22 territory; polling forbidden |

Criteria met: held-state owner; sync cancel on replace; no poll; UnitBase stays general receiver; immobile UnitBase children get Held without movement.

---

## 4. Transition matrix (locked)

Tags verified: Move / Attack / Mine all exist.

| Previous Held | Incoming | Queue | Held result | Movement result |
| --- | --- | ---: | --- | --- |
| None | Move | false | HeldAccepted | RequestMove |
| Move | Move | false | HeldReplaced | RequestMove (internal MoveReplaced) |
| Move | Attack | false | HeldReplaced | StopMove(CommandReplaced) if IsMoving |
| Move | Mine | false | HeldReplaced | StopMove(CommandReplaced) if IsMoving |
| Attack | Move | false | HeldReplaced | RequestMove |
| Attack | Attack | false | HeldReplaced | no movement |
| Mine | Move | false | HeldReplaced | RequestMove |
| Mine | Mine | false | HeldReplaced | no movement |
| Any | Any | true | QueueDeferred | **no** movement change |
| None | Attack | false | HeldAccepted | no movement |
| None | Mine | false | HeldAccepted | no movement |

Unknown tags (should not pass validator): treat as non-Move → stop if moving; hold as today.

---

## 5. Ordering (locked)

For non-queued commands:

1. Capture previous Held (`TOptional` copy / empty)
2. Allocate serial; store new Held
3. Call `SynchronizeMovementWithHeldCommand(Previous)`
4. Log HeldAccepted / HeldReplaced
5. MovementComponent emits its own MoveStarted / MoveReplaced / MoveStopped

Guarantees:

- Attack replacing Move stops movement in the same authoritative call
- Move replacing Move passes **new** Held serial into `RequestMove`
- QueueDeferred never calls StopMove / RequestMove
- Movement rejection does **not** roll back Held Command

### RequestMove failure policy (locked)

**Option A:** Held Command remains authoritative intent; log `MoveExecutionRejected`; no rollback/clear (GP-S22 failure semantics deferred).

---

## 6. Cancellation rules (locked)

| Case | Behavior |
| --- | --- |
| Move → non-Move | `StopMove(CommandReplaced)` only if MovementComponent exists and `IsMoving()` |
| Move → Move | **No** separate StopMove; `RequestMove` performs MoveReplaced |
| Attack/Mine → Move | `RequestMove` starts motion |
| QueueDeferred | movement unchanged |
| Idle after Reach + Attack | Held becomes Attack; StopMove not needed (`!IsMoving`) |

---

## 7. Stop reason decision (locked)

**Option B — plain C++ stop reason**

```cpp
enum class EGP_MovementStopReason : uint8
{
	Manual,
	CommandReplaced,
	EndPlay
};

void StopMove(EGP_MovementStopReason Reason = EGP_MovementStopReason::Manual);
```

| Why not A | `Reason=Manual` for Attack cancel is incorrect production logging |
| Why not C | Separate method duplicates clear/log paths |
| Why B now | Minimal API widen; EndPlay/console use Manual/EndPlay; GP-S21 command cancel is distinct; GP-S22 can reuse enum |
| Reflection | **Not** UENUM / Blueprint |

Update MoveStopped log to print `Manual` / `CommandReplaced` / `EndPlay`.

Console `gp.Movement.Stop` → `StopMove(Manual)`.
EndPlay → `StopMove(EndPlay)` or existing EndPlay path with Reason=EndPlay.

---

## 8. Non-mobile / missing component (locked)

| Case | Behavior |
| --- | --- |
| Held Move on non-`AGP_MobileUnit` | Hold succeeds; log `MovementUnavailable`; no crash |
| Missing MovementComponent | Same |
| Held Attack/Mine on non-mobile | Hold only; no movement call |

No command-layer rejection; no Held rollback.

---

## 9. Tag comparison (locked)

```cpp
CommandTag == FGPGameplayTags::Get().Command_Move
```

Exact equality (same as `GPCommandComponent` validator). **Not** `MatchesTag` parent match; **not** string compare.

---

## 10. Exact proposed APIs (locked)

### UnitCommandComponent (private)

```cpp
void SynchronizeMovementWithHeldCommand(
	const TOptional<FGP_StoredUnitCommand>& PreviousCommand);
```

Called only after non-queued Held store. Uses current `HeldCommand` as the new intent.

No new public/Blueprint API on UnitCommandComponent.

### MovementComponent (public plain C++)

```cpp
void StopMove(EGP_MovementStopReason Reason = EGP_MovementStopReason::Manual);
```

`RequestMove` unchanged.

### Includes

`GPUnitCommandComponent.cpp` includes `GPMobileUnit.h`, `GPMovementComponent.h`, `Tags/GPGameplayTags.h`.
Forward-declare / include enum in `GPMovementComponent.h`.

---

## 11. Logging (locked)

Keep existing Held* and Move* logs.

New category: `LogGPUnitCommandExecution`

| Event | When |
| --- | --- |
| `MoveExecutionRequested` | Before successful path into `RequestMove` (Unit, Serial, Loc) |
| `MoveExecutionRejected` | `RequestMove` returned false (Unit, Serial, Loc) |
| `MovementCancelledByCommand` | Before/after StopMove for non-Move replace (PreviousSerial, NewTag) |
| `MovementUnavailable` | Move held but no MobileUnit/MovementComponent |

Do not duplicate full MovementComponent payloads. No per-frame logs.

---

## 12. Serial contract (locked)

- Held `CommandSerial` is execution identity
- Move accept: after RequestMove success, `Held.CommandSerial == Movement.GetActiveMoveSerial()`
- Move→Move: Held N→N+1; Movement PreviousSerial N, NewSerial N+1
- Move→Attack: Held becomes Attack N+1; Movement stops (serial cleared)
- QueueDeferred: Held serial, Movement serial, allocator **unchanged**

Stale Held after Reach: Held still Move with old serial; Movement ActiveMoveSerial=0 — **expected until GP-S22**. Document only; do not fix in S21.

---

## 13. EndPlay / authority / network (locked)

- Each component keeps its own EndPlay clear/stop; **no** cross-component EndPlay callbacks
- HandleCommand already authority-gated — sync only runs on that path
- No RPC / multicast / prediction / replicated Held state

---

## 14. Exact files

### Modified C++

| File | Change |
| --- | --- |
| `Public/Units/GPUnitCommandComponent.h` | private `SynchronizeMovementWithHeldCommand` |
| `Private/Units/GPUnitCommandComponent.cpp` | sync after Held store; tags; logs |
| `Public/Units/GPMovementComponent.h` | `EGP_MovementStopReason` + `StopMove(Reason)` |
| `Private/Units/GPMovementComponent.cpp` | reason in MoveStopped; EndPlay/console |

### Unchanged

UnitBase, Unit, MobileUnit API (getter already exists), PlayerController, CommandComponent, Stored/UnitCommand payloads, tags registry, Build.cs, assets/maps/config.

### Build.cs impact
**NO**

---

## 15. Validation matrix (operator — RMB primary)

| Case | Expected |
| --- | --- |
| Host Move | HeldAccepted Move; MoveExecutionRequested; MoveStarted; visible move; Held serial == ActiveMoveSerial |
| Move replacement | HeldReplaced Move→Move; MoveReplaced; serials match; destination changes |
| Move→Attack | HeldReplaced Move→Attack; MoveStopped Reason=CommandReplaced; no attack execution |
| Attack→Move | HeldReplaced Attack→Move; MoveStarted |
| QueueDeferred | QueueDeferred; no MoveReplaced/Stop; movement continues; serials unchanged |
| Remote client Team 2 | Client input; server Held+Move; both see transform; no client execution; team isolation |
| Multi-unit | Independent held serials; each MovementComponent starts; overlap OK |
| Regression | Console still works; GP-S17/S18 OK; selection/camera OK; no AI/Nav/GAS; no Held clear on reach |

---

## 16. GP-S22 boundary

Deferred:

- completion callback with serial
- clear Held on Reach/Fail/Cancel
- stale callback protection
- failure rollback of Held
- Attack/Mine executors

After Reach in S21 world: Held Move remains stale intent until next command — acceptable.

---

## 17. Completion status

After implementation + network validation:

**GP-S21 Status: `DONE_WITH_COMPLETION_DEFERRED`**

Means: Held Move starts/replaces movement; non-Move cancels movement; queue ignores movement; validated on network.

Does **not** mean: Reach clears Held; callbacks; stale protection; Nav; Attack/Mine gameplay.

---

## Stop condition
**ANALYSIS_READY_IMPLEMENTATION_PENDING.**
Await GP-S21 implementation assignment.
Do **not** implement sync/StopMove reason/callbacks/Nav from this pass.
Do **not** merge to main.
Do **not** start GP-S22.
