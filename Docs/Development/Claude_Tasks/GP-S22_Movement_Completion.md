# GP-S22 — Movement Completion
(Serial-aware Held Move clear on Reach — analysis)

## Status
**Status: CODE_READY_OPERATOR_VALIDATION_PENDING**

Implemented on `feature/gp-s22-movement-completion-implementation`
(base = `main` `664c30d4c7e8d1df52cf1a6caa2e87c366d84c1a`).
Depends on: GP-S21 `DONE_WITH_COMPLETION_DEFERRED`.

Operator validation **pending**. Failure propagation remains deferred.

---

## 1. Current issue

Pipeline after GP-S21:

```text
Input → server validation → Dispatch
→ UnitCommandComponent stores Held
→ Held Move → RequestMove(serial)
→ MovementComponent moves authority actor
→ MoveReached clears only movement-local active state
```

Problems:

| Problem | Detail |
| --- | --- |
| Stale Held after reach | Held Move remains after `MoveReached` |
| No completion callback | Command layer never notified |
| Serial safety | Older completion must never clear newer Held |
| Cancellation ≠ success | `StopMove(CommandReplaced)` must not clear Held as Reached |
| QueueDeferred | Unrelated; unchanged |

---

## 2. Canonical GP-S22 goal

| In scope | Out of scope |
| --- | --- |
| Reach → completion(serial, Reached) | Failure / blocked / pathfinding |
| UnitCommandComponent clears Held only on exact Move serial match | Queue / Attack / Mine execution |
| Stale / non-Move / no-Held → ignore + log | Prediction / replicated Held / UI / gameplay tasks |
| StopMove / EndPlay / Manual stop → **no** Reached broadcast | |

---

## 3. Selected callback architecture (locked)

**Option A — native non-dynamic multicast delegate on MovementComponent**

```text
UGP_MovementComponent::OnMovementCompleted
← UnitCommandComponent binds in BeginPlay (authority)
← unbinds in EndPlay
```

| Rejected | Why |
| --- | --- |
| B. Owner interface | Couples MobileUnit to command completion |
| C. Polling | Forbidden; tick coupling |
| D. Movement finds UnitCommandComponent | Wrong dependency direction |
| E. Raw function object registration | More ad-hoc than project multicast pattern (`FGPOnSelectionChanged`, lobby delegates) |

Lifecycle: both components are default subobjects on the same actor (`UnitBase` / `MobileUnit`); BeginPlay order among sibling components is not relied upon for existence — lookup via `Cast<AGP_MobileUnit>(GetOwner())->GetUnitMovementComponent()` at bind time.

---

## 4. Completion result model (locked)

**Option B — explicit result enum; emit only Reached today**

```cpp
enum class EGP_MovementCompletionResult : uint8
{
	Reached
};
```

| Option | Verdict |
| --- | --- |
| A. Serial only | Rejected — ambiguous if future cancel/fail shares channel |
| B. Serial + Result (Reached only) | **Selected** — extensible without inventing unused cancel/fail values |
| C. Unified termination enum including Stop* | Rejected — would tempt clearing Held on cancel; Stop already has `EGP_MovementStopReason` |

**Rule:** `StopMove` / EndPlay / Manual **never** broadcast completion. Only physical Reach broadcasts `Reached`.

---

## 5. Exact delegate contract (locked)

In `GPMovementComponent.h` (before UCLASS):

```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FGP_OnMovementCompleted,
	uint32 /*CompletedSerial*/,
	EGP_MovementCompletionResult /*Result*/);
```

API:

```cpp
FGP_OnMovementCompleted& OnMovementCompleted();
```

Private member:

```cpp
FGP_OnMovementCompleted MovementCompleted;
```

| Rule | Policy |
| --- | --- |
| UPROPERTY / BlueprintAssignable / dynamic | **No** |
| Replication | **No** |
| External Broadcast | Only MovementComponent Reach path |
| Accessor | Returns reference; no public mutable broadcast helper in Shipping |

---

## 6. Broadcast ordering (locked)

Inside Reach finish path (refactor FinishReached):

1. Capture `CompletedSerial = ActiveMoveSerial`
2. Clear movement-local active state (`bIsMoving=false`, `ActiveMoveSerial=0`, tick off)
3. Log `MoveReached`
4. `MovementCompleted.Broadcast(CompletedSerial, Reached)`
5. **Return immediately** — no further mutation of movement state in that Tick after broadcast

### Reentrancy guarantee

If a future observer synchronously starts Move N+1 during the Broadcast for N:

- Old Tick path must not run after Broadcast (return immediately)
- Old path must not set `ActiveMoveSerial=0` after Broadcast (already cleared **before** Broadcast)
- Old path must not disable Tick after Broadcast if N+1 already enabled Tick (tick disabled **before** Broadcast; N+1 re-enables inside RequestMove)
- Handler for N must not clear Held N+1 (serial guard)

---

## 7. Binding lifecycle (locked)

**Option B — `FDelegateHandle` + explicit remove**

```cpp
// UnitCommandComponent
virtual void BeginPlay() override; // new
// EndPlay already exists

FDelegateHandle MovementCompletedHandle;

void HandleMovementCompleted(uint32 CompletedSerial, EGP_MovementCompletionResult Result);
```

| Step | Behavior |
| --- | --- |
| BeginPlay | `Super`; if `!Owner->HasAuthority()` return; cast MobileUnit; get Movement; if missing MobileUnit → silent; if MobileUnit but null Movement → Warning once; else `AddUObject` store handle |
| EndPlay | If handle valid → `Remove`; invalidate handle; existing HeldCleared; `Super` |
| Client | **No bind** |

`RemoveAll(this)` rejected as primary — explicit handle is clearer with one binding.

---

## 8. Handler clear rules (locked)

```cpp
void HandleMovementCompleted(uint32 CompletedSerial, EGP_MovementCompletionResult Result);
```

Authority-only. Clear only when **all** true:

1. Owner exists + HasAuthority  
2. `Result == Reached`  
3. Held exists  
4. Held tag exactly `Command_Move`  
5. `Held.CommandSerial == CompletedSerial`  

On match: log `HeldMoveCompleted`; `HeldCommand.Reset()`; **do not** reset `NextCommandSerial`.

On mismatch: no mutation; log `MovementCompletionIgnored` with Reason.

After clear: next non-queued command → HeldAccepted (not HeldReplaced). QueueDeferred with empty Held remains existing GP-S18 behavior (HeldSerial=none).

---

## 9. Stale completion matrix (locked)

| Completion | Current Held | Action |
| --- | --- | --- |
| serial 1 Reached | Move serial 1 | clear Held |
| serial 1 Reached | Move serial 2 | ignore SerialMismatch |
| serial 1 Reached | Attack serial 2 | ignore HeldTagNotMove |
| serial 1 Reached | Mine serial 2 | ignore HeldTagNotMove |
| serial 1 Reached | None | ignore NoHeldCommand |
| unsupported result | Move serial 1 | ignore UnsupportedResult |

Current sync backend does not emit stale Reach after replace — guard required for future async/Nav executors.

---

## 10. Stop / Manual / EndPlay (locked)

| Event | Broadcast Reached? | Held |
| --- | --- | --- |
| Move→Attack StopMove(CommandReplaced) | **No** | Attack remains |
| `gp.Movement.Stop` Manual | **No** | Held Move **remains** (debug bypass; document limitation) |
| EndPlay StopMove(EndPlay) | **No** | UnitCommand EndPlay clears Held independently |
| Move→Move replace | **No** completion for old serial | New Held; only N+1 eventually Reaches |

Manual stop does **not** clear Held via command layer — no new cancel API in GP-S22.

EndPlay: unbind before/with Held clear; UObject binding + Remove prevents invoke on destroyed listener; do not rely on component EndPlay order for Broadcast during teardown (no Reached on EndPlay).

---

## 11. Logging (locked)

Category: `LogGPUnitCommandExecution` (existing)

| Event | Level | When |
| --- | --- | --- |
| `HeldMoveCompleted` | Log | Exact match clear |
| `MovementCompletionIgnored` Reason=`SerialMismatch` | Log | Stale serial |
| `MovementCompletionIgnored` Reason=`HeldTagNotMove` | Log | Attack/Mine Held |
| `MovementCompletionIgnored` Reason=`NoHeldCommand` | Log | Empty Held |
| `MovementCompletionIgnored` Reason=`UnsupportedResult` | Warning | Future/non-Reached |
| `MovementCompletionIgnored` Reason=`NoAuthority` | Warning | Defensive |

Fields (Ignored): Unit, CompletedSerial, HeldSerial, HeldTag, Result, Reason, Role, NetMode.  
Fields (Completed): Unit, Serial, Tag, Role, NetMode.

Preserve MoveReached / HeldAccepted / HeldReplaced / QueueDeferred.

---

## 12. Authority / network (locked)

- Broadcast only on authority Reach path  
- Clients: transform replication only; no Tick movement; no bind; no Held clear  
- No RPC / multicast / new replication  

---

## 13. Debug validation decision (locked)

**Natural path** validates matching clear.

**Stale path** cannot be produced naturally with current sync replace (old serial never Reaches).

**Selected:** non-shipping console + private helper:

```text
gp.Movement.TestCompletion <Serial>
```

- Near existing `gp.Movement.Test` / `Stop` in `GPMovementComponent.cpp`
- Finds first authority `AGP_MobileUnit`
- Calls `#if !UE_BUILD_SHIPPING` `DebugBroadcastCompletion(Serial)` which Broadcasts `Reached` **without** mutating physical movement state
- Same delegate path as real Reach (not private handler bypass)
- Shipping: excluded

Matching synthetic while a physical move is active is **forbidden** for operator tests (would clear Held while body still moves). Use synthetic only for SerialMismatch / NoHeld / TagNotMove scenarios.

Rejected: UnitCommand private-handler inject; Shipping public Broadcast; automation-only.

---

## 14. Validation plan

| Case | Expected |
| --- | --- |
| Natural completion | MoveReached N → HeldMoveCompleted N; next Move → HeldAccepted (not Replaced) |
| Move replacement | No completion for N; Reach N+1 → HeldMoveCompleted N+1 |
| Move→Attack | StopMove CommandReplaced; Attack Held remains; no HeldMoveCompleted for N |
| Stale synthetic | Held Move N+1; `TestCompletion N` → Ignored SerialMismatch; N+1 continues |
| Manual Stop | Stop Manual; Held remains; no HeldMoveCompleted |
| QueueDeferred | Unchanged |
| Remote Team 2 | Server Reach + HeldMoveCompleted; no client duplicate |
| Multi-unit | Independent clear per unit serial |

---

## 15. Exact proposed APIs

```cpp
// GPMovementComponent.h
enum class EGP_MovementCompletionResult : uint8 { Reached };

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FGP_OnMovementCompleted, uint32, EGP_MovementCompletionResult);

FGP_OnMovementCompleted& OnMovementCompleted();
#if !UE_BUILD_SHIPPING
void DebugBroadcastCompletion(uint32 Serial);
#endif

// GPUnitCommandComponent.h
virtual void BeginPlay() override;
void HandleMovementCompleted(uint32 CompletedSerial, EGP_MovementCompletionResult Result); // private
FDelegateHandle MovementCompletedHandle; // private
```

Forward-declare `EGP_MovementCompletionResult` in UnitCommandComponent.h if needed; include `GPMovementComponent.h` in `.cpp` for bind.

---

## 16. Exact files

| File | Role |
| --- | --- |
| `Public/Units/GPMovementComponent.h` | Enum, delegate, accessor, optional debug decl |
| `Private/Units/GPMovementComponent.cpp` | Broadcast after Reach; console TestCompletion |
| `Public/Units/GPUnitCommandComponent.h` | BeginPlay, handle, handler |
| `Private/Units/GPUnitCommandComponent.cpp` | Bind/unbind, clear rules, logs |

Unchanged: UnitBase, MobileUnit, Unit, PC, CommandComponent, payloads, tags, Build.cs.  
**Build.cs: NO**

---

## 17. Build workflow (project)

| Phase | Builds |
| --- | --- |
| Analysis | **None** |
| Implementation candidate | GPEditor Win64 Development + UHT |
| Finalization after operator validation | GP Win64 Development + GP Win64 Shipping |

---

## 18. Failure boundary / final status

Deferred: RequestMove reject propagation; blocked; aborted-as-failure; timeout; retry; UI.

After implementation + validation:

**GP-S22 Status: `DONE_WITH_FAILURE_PROPAGATION_DEFERRED`**

Means: natural Reach clears matching Held Move; stale ignored; cancel/manual/EndPlay do not succeed-clear; network validated. Failure/blocked results deferred.

---

## 19. Implementation record

### Actual APIs
```cpp
enum class EGP_MovementCompletionResult : uint8 { Reached };
DECLARE_MULTICAST_DELEGATE_TwoParams(FGP_OnMovementCompleted, uint32, EGP_MovementCompletionResult);
FGP_OnMovementCompleted& OnMovementCompleted();
#if !UE_BUILD_SHIPPING
void DebugBroadcastCompletion(uint32 Serial);
#endif
```

### Broadcast ordering
Capture CompletedSerial → ClearActiveMovementState → MoveReached log → Broadcast(Reached) → return.

### Binding
Authority BeginPlay: MobileUnit → AddUObject; `FDelegateHandle` + `TWeakObjectPtr`.
EndPlay: Remove before HeldCleared.
Client: no bind.

### Handler
Clear Held only on authority + Reached + Held Move + serial match.
Ignored: NoAuthority / UnsupportedResult / NoHeldCommand / HeldTagNotMove / SerialMismatch.

### Debug
`gp.Movement.TestCompletion <Serial>` → synthetic Broadcast; no movement mutation; Shipping excluded.

Target resolution (fixed after operator feedback):

1. First authority `AGP_MobileUnit` with `IsMoving()==true`
2. Else fallback first authority mobile unit (+ fallback log)

Console dispatch fields: Unit, InjectedSerial, ActiveMoveSerial, IsMoving, Selection=`MovingUnit|FallbackFirstAuthority`, Result=Reached.

### Builds
GPEditor Development + UHT — **PASSED** (candidate + debug-target fix). GP Dev/Shipping deferred to finalization.

### Operator validation (partial)

| Case | Result |
| --- | --- |
| Natural MoveReached → HeldMoveCompleted | **PASS** |
| Held clear; next Move → HeldAccepted | **PASS** |
| Move→Move replacement; completion only for latest serial | **PASS** |
| Move→Attack → CommandReplaced; no HeldMoveCompleted for stopped Move | **PASS** |
| Attack→Move then Reach clears | **PASS** |
| Z preservation (88) | **PASS** |
| Synthetic on idle unit → NoHeldCommand | **PASS** (wrong target; pre-fix) |
| Stale SerialMismatch on active Held Move | **PENDING** (retest after MovingUnit preference) |
| Remote Team 2 / multi-unit / Manual stop | **PENDING** |

---

## Stop condition
**CODE_READY_OPERATOR_VALIDATION_PENDING.**
Await operator stale SerialMismatch (MovingUnit target) + remaining matrix.
Do **not** merge to main.
Do **not** start failure propagation / Nav / Attack/Mine / queue from this pass.
