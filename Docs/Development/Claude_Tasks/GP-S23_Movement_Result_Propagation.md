# GP-S23 Movement Result Propagation

## Status
**CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `966d0e7a884af02593608ea398eb1627d9f5a58f` (Merge GP-S23 movement result analysis)

Depends on: GP-S22 `DONE_WITH_FAILURE_PROPAGATION_DEFERRED`; GP-S23 analysis merged.

Branch: `feature/gp-s23-movement-result-implementation`

Target stage close: **`DONE_WITH_FAILED_RESULT_DEFERRED`** (after operator validation).

---

## Current Behavior

After GP-S22:

```text
HandleCommand (authority, non-queued)
→ AllocateCommandSerial
→ Held = new command
→ SynchronizeMovementWithHeldCommand
   → Move: RequestMove(dest, serial) → bool
      accepted → MoveExecutionRequested log
      rejected → MoveExecutionRejected log; Held remains
   → non-Move while IsMoving: StopMove(CommandReplaced) → no result broadcast
→ HeldAccepted / HeldReplaced log

Tick reach:
→ ClearActiveMovementState
→ MoveReached log
→ OnMovementCompleted(Serial, Reached)
→ UnitCommandComponent clears Held only if authority + Reached + Held Move + serial match

StopMove (Manual / CommandReplaced / EndPlay):
→ clear movement-local state + MoveStopped log
→ never broadcast completion
```

Facts from code:

| Item | Fact |
| --- | --- |
| Result enum | `EGP_MovementCompletionResult { Reached }` only |
| Delegate | `FGP_OnMovementCompleted(uint32, EGP_MovementCompletionResult)` native multicast |
| Bind | Authority `BeginPlay` on MobileUnit; unbind before `HeldCleared` in `EndPlay` |
| `RequestMove` | `bool`; reject paths log `MoveRejected`; no delegate |
| Move→Move | In-place replace (`MoveReplaced`); previous serial gets no callback |
| Manual stop | `gp.Movement.Stop` → `StopMove(Manual)`; Held Move remains |
| EndPlay | `StopMove(EndPlay)` silent; command layer clears Held locally |
| Failed path | None (straight-line XY; no Nav/blocked/timeout) |
| Phantom Held | Rejected `RequestMove` leaves Held Move set (gap after GP-S21/S22) |

---

## Problem

Command layer can observe only successful natural reach. It cannot distinguish, via a single serial-aware contract:

1. destination reached;
2. move request refused before active movement;
3. active movement cancelled;
4. future runtime failure of an active move.

Without that contract, a future composite Attack executor cannot safely:

```text
Attack → optional RequestMove(serial M) → wait terminal(M) → continue only on allowed result → ignore stale
```

GP-S22 explicitly deferred failure/cancellation propagation. GP-S23 closes the result contract for producers that already exist; it does not invent Nav/blocked failures.

---

## Goals

1. One serial-aware terminal result channel for moves that became active.
2. Synchronous, reentrancy-safe rejection path for `RequestMove` failures.
3. Exact-serial Held policy for Reached / Cancelled / Rejected.
4. Cancellation emission on existing stop/supersede paths that need consumers.
5. Safe EndPlay (no decorative teardown callbacks).
6. Non-shipping synthetic hooks for each implemented result.
7. Contract shaped for future Attack executor; Attack itself not implemented.

---

## Non-Goals

- NavMesh / pathfinding / blocked detection / collision sweep
- Timeout / retry
- `Failed` runtime producer
- Attack / Mine / damage / health executors
- Queue storage / execution
- Formation / avoidance
- GAS movement abilities
- Replicated Held / client prediction / UI
- Command-layer public Cancel API (contract defined; implementation deferred unless required by Manual path via existing `StopMove`)
- Builds / UHT / asset edits on this analysis branch

---

## Current Code Findings

### Classes / ownership

| Class | Role |
| --- | --- |
| `AGP_UnitBase` | Owns `UGP_UnitCommandComponent`; `ReceiveCommand` → `HandleCommand` |
| `AGP_MobileUnit` | Owns `UGP_MovementComponent`; getter `GetUnitMovementComponent()` |
| `UGP_MovementComponent` | Authority XY move; `RequestMove` / `StopMove` / Tick reach / completion broadcast |
| `UGP_UnitCommandComponent` | Held + serial allocator; movement sync; completion handler |
| `FGP_StoredUnitCommand` | Tag, location, weak target, serial; plain C++ |

### Call sites (complete in `GP/Source`)

| Symbol | Locations |
| --- | --- |
| `RequestMove` | `GPMovementComponent.*`; `SynchronizeMovementWithHeldCommand`; `gp.Movement.Test` |
| `StopMove` | `GPMovementComponent.*`; Move→non-Move sync; `EndPlay`; `gp.Movement.Stop` |
| `OnMovementCompleted` | Bind/unbind + Broadcast + DebugBroadcast in movement/command components |
| `DebugBroadcastCompletion` | Non-shipping synthetic Reached only |
| `EGP_MovementStopReason` | Manual / CommandReplaced / EndPlay |
| `EGP_MovementCompletionResult` | Reached only |
| `HeldMoveCompleted` | Clear log after matching Reached |
| `MovementCompletionIgnored` | NoAuthority / UnsupportedResult / NoHeld / HeldTagNotMove / SerialMismatch |
| `MovementCancelledByCommand` | After `StopMove(CommandReplaced)` from sync |
| `MovementUnavailable` | Non-mobile / missing component |
| `MoveExecutionRejected` | `RequestMove` returned false after Held already stored |

### Ordering risks (today)

1. **Held-before-RequestMove:** serial allocated and Held mutated before `RequestMove`. Reject leaves phantom Held.
2. **Sync reject broadcast would reenter Held mutation** if rejection used the completion delegate while `HandleCommand` still runs (HeldAccepted/HeldReplaced logs after sync).
3. **Move→Move silent supersession:** previous serial has no terminal event for an external waiter.
4. **Manual stop:** movement stops; Held Move remains (intentional in S22 “cancel ≠ Reached”; production cancel clear was deferred).
5. **EndPlay component order:** Movement `EndPlay` stops; Command `EndPlay` unbinds then clears Held. Broadcasting during teardown is unsafe.

### Build.cs

`GPRuntime.Build.cs` already depends on Engine/GameplayTags/GPGASRuntime. **No module change required for GP-S23.**

---

## Canonical Result Contract

### Selected architecture (locked)

| Decision | Selection |
| --- | --- |
| Terminal channel | **Single native multicast** (Option A expanded; rename for clarity → Option C naming) |
| Sync rejection | **Immediate structured return from `RequestMove`** (Option C). **No sync reject broadcast** |
| `Failed` | **Not in enum** until a real producer exists (Nav stage) |
| Blueprint / UPROPERTY / replication | **Forbidden** |

### Exact types

```cpp
/** Terminal result for a movement serial that was accepted/active. */
enum class EGP_MovementResult : uint8
{
	Reached,
	Cancelled
};

/** Reason accompanying a terminal result. Reached uses None. */
enum class EGP_MovementResultReason : uint8
{
	None,
	Superseded,        // Move→Move replace inside RequestMove
	CommandReplaced,   // StopMove while non-Move Held installed
	Manual             // StopMove(Manual)
};

/** Synchronous RequestMove outcome. Not delivered via delegate. */
enum class EGP_MovementRequestStatus : uint8
{
	Accepted,
	Rejected
};

enum class EGP_MovementRejectReason : uint8
{
	None,
	MissingOwner,
	NoAuthority,
	InvalidSerial,
	InvalidDestination,
	InvalidMoveSpeed,
	InvalidAcceptanceRadius
};

struct FGP_MovementRequestOutcome
{
	EGP_MovementRequestStatus Status = EGP_MovementRequestStatus::Rejected;
	EGP_MovementRejectReason RejectReason = EGP_MovementRejectReason::None;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FGP_OnMovementResult,
	uint32 /*Serial*/,
	EGP_MovementResult /*Result*/,
	EGP_MovementResultReason /*Reason*/);
```

### API surface (MovementComponent)

```cpp
FGP_MovementRequestOutcome RequestMove(const FVector& Destination, uint32 CommandSerial);
void StopMove(EGP_MovementStopReason Reason = EGP_MovementStopReason::Manual);
FGP_OnMovementResult& OnMovementResult();

#if !UE_BUILD_SHIPPING
void DebugBroadcastResult(uint32 Serial, EGP_MovementResult Result, EGP_MovementResultReason Reason);
#endif
```

### Migration from GP-S22

| Old | New |
| --- | --- |
| `EGP_MovementCompletionResult` | `EGP_MovementResult` |
| `FGP_OnMovementCompleted` | `FGP_OnMovementResult` (+ Reason) |
| `OnMovementCompleted()` | `OnMovementResult()` |
| `HandleMovementCompleted` | `HandleMovementResult` |
| `DebugBroadcastCompletion` | `DebugBroadcastResult` |
| `gp.Movement.TestCompletion` | alias → Reached; primary `gp.Movement.TestResult` |

`EGP_MovementStopReason` remains the **StopMove input** enum (Manual / CommandReplaced / EndPlay). It is not the terminal result enum. Mapping:

| StopMove reason | Emit terminal? | Result / Reason |
| --- | --- | --- |
| Manual | **Yes** if was moving | Cancelled / Manual |
| CommandReplaced | **Yes** if was moving | Cancelled / CommandReplaced |
| EndPlay | **No** | silent clear only |

### Why not multiple delegates

| Option | Verdict |
| --- | --- |
| A. Expand one terminal delegate | **Selected** (with rename) — one serial wait point for Attack; one “at most one terminal per serial” rule |
| B. Separate rejected/cancelled/failed delegates | Rejected — double-subscribe risk; Attack must fan-in; higher ignore-matrix cost |
| C. Rename-only without sync/async split | Incomplete — sync reject via multicast causes reentrancy with Held mutation |

### Why RequestRejected is not a delegate value

Synchronous `Broadcast(RequestRejected)` from inside `RequestMove` can run while `HandleCommand` still owns the stack (`Held` already set; `HeldAccepted` log still pending). A handler that clears/replaces Held reenters command state under the caller. **Rejected is therefore return-only.** Future Attack executor treats reject as immediate local outcome of its `RequestMove` call, not as a deferred callback.

---

## Result and Reason Semantics

| Value | When | Move physically started? | Callback includes serial? | Command clears matching Held Move? | Terminal? | Attack executor use |
| --- | --- | --- | --- | --- | --- | --- |
| `Reached` | Tick acceptance / snap | Yes | Yes (delegate) | **Clear** if exact Move serial | Yes | Continue attack stage that waited on that serial |
| `Cancelled` + `Superseded` | Move→Move replace after new active state committed | Yes (old serial was active) | Yes (old serial) | **Ignore** if Held already newer; clear only if still matching (should not happen in sync replace path) | Yes for old serial | Abort wait on old serial |
| `Cancelled` + `CommandReplaced` | `StopMove(CommandReplaced)` while moving | Yes | Yes | **Ignore** (Held already non-Move or newer); clear only exact matching Move | Yes | Abort wait on cancelled serial |
| `Cancelled` + `Manual` | `StopMove(Manual)` while moving | Yes | Yes | **Clear** exact matching Held Move | Yes | Abort wait |
| Sync `Rejected` | `RequestMove` validation fail | **No** | **No delegate**; outcome struct + logs | **Clear** the Held Move that just requested (exact serial) | Terminal for that request attempt | Abort immediately; do not wait |
| `Failed` | — | — | — | — | — | **Deferred** (no producer) |

Speculative results **not** added: Blocked, NoPath, TargetLost, Timeout, InterruptedByCombat.

---

## RequestMove Contract

### Signature (locked)

```cpp
FGP_MovementRequestOutcome RequestMove(const FVector& Destination, uint32 CommandSerial);
```

`bool` removed. Call sites use `Outcome.Status == Accepted`.

### Reject reasons (locked producers today)

Map existing `MoveRejected` string reasons to `EGP_MovementRejectReason`:

MissingOwner, NoAuthority, InvalidSerial, InvalidDestination, InvalidMoveSpeed, InvalidAcceptanceRadius.

TickWithoutAuthority remains an internal safety clear (not a `RequestMove` reject); it does **not** emit Cancelled/Failed in GP-S23 (authority loss is not a designed gameplay cancel path). Log Warning + silent clear only.

### Canonical HandleCommand / sync ordering (locked)

```text
1. Authority + non-queue checks
2. PreviousCommand = Held
3. AllocateCommandSerial → Stored.CommandSerial
4. Held = Stored                          // Held mutation before RequestMove
5. SynchronizeMovementWithHeldCommand(Previous):
   if current Held is Move:
     a. Outcome = RequestMove(dest, Held.Serial)
     b. if Accepted:
          log MoveExecutionRequested
        else:
          log MoveRequestRejected (Unit, Serial, RejectReason, Destination, Role, NetMode)
          if Held still exact Move serial == Stored.Serial:
             clear Held
          // NO delegate broadcast
   else if IsMoving:
     StopMove(CommandReplaced)            // emits Cancelled/CommandReplaced after local clear
     log MovementCancelledByCommand
6. Log HeldAccepted / HeldReplaced based on whether Held is still set after step 5
   - if reject cleared Held: log HeldMoveRejected (or skip HeldAccepted; use explicit reject log only)
```

**HeldAccepted/HeldReplaced logging rule after reject:** do **not** emit HeldAccepted for a Move that was cleared by sync reject. Emit `HeldMoveRejected` with Serial + RejectReason instead. If previous Held existed and new Move was rejected and cleared, previous was already overwritten — that is intentional replace-then-reject; log `HeldReplacedThenRejected` is **not** required; `HeldMoveRejected` + earlier capture of PreviousSerial in MoveRequestRejected fields is enough.

### Reentrancy rule for RequestMove

- `RequestMove` **never** broadcasts on Rejected.
- `RequestMove` **may** broadcast `Cancelled(Superseded)` for the previous serial when accepting a replace (see Cancellation). Broadcast occurs only after new active state is committed and before `return Accepted`.
- Callers must tolerate: during Accepted-replace broadcast, a listener may call `RequestMove` again. Post-broadcast code in the outer `RequestMove` must not mutate movement state for the superseded serial (already true if mutate-before-broadcast).

---

## Cancellation Contract

### Move→Move replacement

**Locked: emit `Cancelled` + `Superseded` for previous serial.**

Not silent supersession. External serial waiters (future Attack) require a terminal for the old serial.

Safe ordering inside `RequestMove` when `bIsMoving`:

```text
1. Capture PreviousSerial, PreviousDestination
2. Commit new MoveDestination / ActiveMoveSerial / tick enabled
3. Log MoveReplaced
4. Broadcast(PreviousSerial, Cancelled, Superseded)
5. return Accepted
```

Held is already the new Move (serial N+1) before this `RequestMove`. Handler sees Cancelled(N) vs Held(N+1) → SerialMismatch → ignore. New Held survives.

### Move→Attack / Mine / other non-Move

**Locked: `StopMove(CommandReplaced)` emits `Cancelled` + `CommandReplaced` when it actually stopped an active move.**

Consumer: any serial waiter; UnitCommandComponent applies Held policy (ignore when Held is already non-Move / newer serial).

Guarantee vs new Held: Held replaced **before** `SynchronizeMovementWithHeldCommand` calls `StopMove`. Cancelled carries old movement serial; Held serial is new → ignore.

`MovementCancelledByCommand` command-layer log remains (command context: PreviousMoveSerial, NewCommandSerial, NewCommandTag). Movement emits one `MovementResult` log; do not duplicate full field sets.

### Manual stop

**Locked production contract:**

| Item | Policy |
| --- | --- |
| Emit | `Cancelled` + `Manual` when `StopMove(Manual)` stops an active move |
| Held | UnitCommandComponent **clears** exact matching Held Move |
| Debug console | `gp.Movement.Stop` is a valid producer of production `StopMove(Manual)` |
| Separate Cancel API | **Deferred** — not required for GP-S23; `StopMove(Manual)` is the production cancel entry |

### EndPlay

**Locked: silent.** No `Cancelled` broadcast on `StopMove(EndPlay)`.

Reasons: teardown safety; bind may already be removed; external executor on a dying unit must not depend on a callback. Command layer already clears Held with `HeldCleared Reason=EndPlay`.

Ordering:

```text
CommandComponent::EndPlay:
  unbind OnMovementResult
  clear Held + HeldCleared
  Super

MovementComponent::EndPlay:
  StopMove(EndPlay)   // clear local state, MoveStopped log, NO broadcast
  Super
```

Do not rely on relative EndPlay order for correctness beyond unbind-before-use; Movement must not broadcast on EndPlay regardless.

---

## Held State Policy

Exact serial + exact Move tag required for any clear. Authority-only handler.

| Result | Matching Held Move | Newer Held Move | Non-Move Held | No Held |
| --- | --- | --- | --- | --- |
| Reached | **clear** + HeldMoveFinished | **ignore** SerialMismatch | **ignore** HeldTagNotMove | **ignore** NoHeldCommand |
| Cancelled | **clear** + HeldMoveFinished | **ignore** SerialMismatch | **ignore** HeldTagNotMove | **ignore** NoHeldCommand |
| Sync Rejected | **clear** in Synchronize path (no delegate) | N/A (same request stack) | N/A | N/A |

Notes:

- Cancelled clear applies to Manual and to any Cancelled that still matches (defensive). Replace paths normally hit ignore.
- Reached never clears non-Move Held.
- Stale protection unchanged: `CompletedSerial != Held.CommandSerial` → ignore.
- NoAuthority → ignore Warning.

---

## Serial Semantics

| Case | Policy |
| --- | --- |
| Rejected Move serial | Already allocated in `HandleCommand` before `RequestMove`; stored briefly as Held; cleared on reject |
| Broadcast rejection before Held mutation | **Forbidden** — reject is not broadcast |
| `NextCommandSerial` after reject | **Continues monotonically**; never rewind |
| Attack sub-move correlation | Attack (future) records the serial it passed into `RequestMove` / the Held Move serial it waits on; matches terminal Serial or sync Rejected of that same serial |
| Serial `0` | Invalid; reject; never Held |

---

## Reentrancy Guarantees

### Canonical terminal ordering (Reached / Cancelled)

```text
1. Capture Serial, Destination, Reason-related fields
2. Mutate movement-local state (clear active OR commit replace-before-cancel)
3. Disable tick if no longer moving
4. Log MovementResult (+ MoveReached / MoveStopped / MoveReplaced as applicable)
5. Broadcast OnMovementResult
6. return
```

**After terminal broadcast for serial S, MovementComponent must not mutate state belonging to S.**

### Per path

| Path | Broadcast? | Ordering notes |
| --- | --- | --- |
| Natural Tick reach | Reached / None | Clear → log MoveReached + MovementResult → Broadcast → return |
| Sync RequestMove reject | **Forbidden** | Validate → log MoveRejected → return Rejected; command layer clears Held after return |
| RequestMove replace | Cancelled / Superseded for old serial | Commit new active → log MoveReplaced → Broadcast old → return Accepted |
| StopMove(CommandReplaced) | Cancelled / CommandReplaced | Capture → clear → log MoveStopped + MovementResult → Broadcast → return |
| StopMove(Manual) | Cancelled / Manual | Same as above |
| StopMove(EndPlay) | **Forbidden** | Capture → clear → log MoveStopped → return |
| Debug synthetic | Allowed for Reached/Cancelled | Broadcast only; **no** movement-state mutation |

### Callback may start a new Move

Handler / future Attack may call `RequestMove` during Broadcast. Outer terminal path must already have finished mutating the old serial. Replace path commits new state before Broadcast of old Cancelled so a nested Accept cannot be wiped by the outer function.

---

## Logging Contract

Prefer one primary structured log per terminal event. Keep existing specialized logs only where they add distinct phase info.

| Event | Category | When | Fields |
| --- | --- | --- | --- |
| `MovementResult` | LogGPUnitMovement | Every terminal Broadcast | Unit, Serial, Result, Reason, Destination, Role, NetMode |
| `MoveReached` | LogGPUnitMovement | Reached only (keep; Destination + FinalLocation) | Unit, Serial, Destination, FinalLocation, Role, NetMode |
| `MoveStopped` | LogGPUnitMovement | StopMove cleared active (keep) | Unit, Serial, Reason(stop), Location, Role, NetMode |
| `MoveReplaced` | LogGPUnitMovement | Move→Move accept (keep) | Unit, PreviousSerial, NewSerial, destinations, Role, NetMode |
| `MoveRejected` | LogGPUnitMovement | RequestMove validation fail (keep; Reason=RejectReason name) | Unit, Serial, Destination, Reason, Role, NetMode |
| `MoveRequestRejected` | LogGPUnitCommandExecution | Replaces `MoveExecutionRejected` | Unit, Serial, Destination, RejectReason, Role, NetMode |
| `HeldMoveFinished` | LogGPUnitCommandExecution | Replaces `HeldMoveCompleted`; clear after Reached **or** Cancelled match | Unit, Serial, Tag, Result, Reason, Role, NetMode |
| `MovementResultIgnored` | LogGPUnitCommandExecution | Replaces `MovementCompletionIgnored` | Unit, Serial, HeldSerial, HeldTag, Result, Reason, IgnoreReason, Role, NetMode |
| `MovementCancelledByCommand` | LogGPUnitCommandExecution | Keep after StopMove from non-Move sync | Unit, PreviousMoveSerial, NewCommandSerial, NewCommandTag, Role, NetMode |

Do not emit both `HeldMoveCompleted` and `HeldMoveFinished`. Rename to `HeldMoveFinished`.

---

## Debug Validation Contract

All hooks: `#if !UE_BUILD_SHIPPING` only.

| Command | Behavior |
| --- | --- |
| `gp.Movement.TestResult <Serial> <Result> [Reason]` | Synthetic `DebugBroadcastResult`; **no** movement mutation. Result ∈ {Reached, Cancelled}. Reason optional (default None for Reached; Manual for Cancelled). Target resolution: first authority moving unit, else fallback first authority (same as S22 fix) |
| `gp.Movement.TestCompletion <Serial>` | **Compatibility alias** → `TestResult Serial Reached` |
| `gp.Movement.Stop` | Production `StopMove(Manual)` → real Cancelled path |
| `gp.Movement.Test` | Unchanged RequestMove helper; adapt to outcome struct |

Synthetic paths validate stale serial / Held policy only. They must not clear movement-local active state.

Rejected path validation: use real reject producer (e.g. `RequestMove` with Serial=0 via test helper, or temporarily invalid destination through a non-shipping reject helper). Prefer real `RequestMove` reject over synthetic Rejected broadcast (Rejected is not a delegate value).

---

## File Plan

| File | Change |
| --- | --- |
| `GPMovementComponent.h/.cpp` | Enums, outcome struct, RequestMove return, Cancelled emits, rename delegate/API, debug hooks, logs |
| `GPUnitCommandComponent.h/.cpp` | Bind rename; handler for Reached/Cancelled; sync Held clear on reject; log renames |
| `GP-S23_Movement_Result_Propagation.md` | This document → implementation record later |
| `AI_Project_Log.md` | Checkpoints |
| `Cursor_Work_Report.md` | Report |

| File | GP-S23 change? |
| --- | --- |
| `GPMobileUnit.*` | **NO** |
| `GPStoredUnitCommand.h` | **NO** |
| `GPUnitCommand.h` | **NO** |
| `GPUnitBase.*` | **NO** |
| Gameplay tags | **NO** |
| `GPRuntime.Build.cs` | **NO** |
| Assets / maps / config | **NO** |

---

## Implementation Sequence

1. Add `EGP_MovementResult`, `EGP_MovementResultReason`, `EGP_MovementRequestStatus`, `EGP_MovementRejectReason`, `FGP_MovementRequestOutcome` in `GPMovementComponent.h`.
2. Replace `EGP_MovementCompletionResult` / `FGP_OnMovementCompleted` with `FGP_OnMovementResult` three-param delegate; update accessor name.
3. Change `RequestMove` to return `FGP_MovementRequestOutcome`; map reject reasons; on in-place replace commit-then-Broadcast Cancelled/Superseded.
4. Change `StopMove`: Manual/CommandReplaced broadcast Cancelled after clear; EndPlay remains silent.
5. Update reach path to Broadcast `(Serial, Reached, None)`.
6. Update `UGP_UnitCommandComponent` bind/unbind/handler: clear on matching Reached **or** Cancelled; rename ignore/finish logs.
7. In `SynchronizeMovementWithHeldCommand`: on Rejected, clear matching Held Move; emit `MoveRequestRejected`; adjust HeldAccepted logging.
8. Add `DebugBroadcastResult` + `gp.Movement.TestResult`; keep `TestCompletion` alias.
9. Update GP-S23 doc status + AI log + Cursor report at candidate/finalization stages.
10. Candidate builds: GPEditor Development + UHT. Finalization: GP Dev + Shipping after operator validation.

---

## Operator Validation Plan

2P Listen Server. Authority-focused; remote/multi-unit as structural checks.

### Reached
Natural Move → `MovementResult Reached` → `HeldMoveFinished` clears matching Held → next Move → HeldAccepted.

### Rejected
Force real reject (InvalidSerial via controlled non-shipping path, or InvalidDestination). Expected: `MoveRejected` + `MoveRequestRejected`; Held not left as phantom Move; next valid Move → HeldAccepted with new serial; allocator monotonic.

### Cancelled by replacement (Move→Attack)
Active Move → Attack Held → `StopMove(CommandReplaced)` → `Cancelled/CommandReplaced` for old serial → Attack Held retained → old Cancelled does not clear Attack.

### Move→Move
Move N → Move N+1 → `Cancelled/Superseded` for N (ignored vs Held N+1) → natural Reach clears only N+1.

### Manual cancellation
`gp.Movement.Stop` during Held Move → `Cancelled/Manual` → matching Held cleared.

### Stale result
Active Held Move serial 2; inject `TestResult 1 Cancelled Manual` (or Reached) with Selection=MovingUnit → MovementResultIgnored SerialMismatch → movement continues → natural finish clears 2.

### Reentrancy
During Cancelled/Reached handler, issuing a new Move must leave new active serial intact; old path must not clear new Held.

### Remote and multi-unit
Authority-only handler; client does not bind; completion/cancel on unit A never clears unit B.

### EndPlay
Destroy/moving unit teardown: no crash; no Cancelled broadcast required; no use-after-unbind.

---

## Compatibility Matrix

| Scenario | Post-GP-S23 expected |
| --- | --- |
| Natural Move | Reached clears matching Held (unchanged success path; Result+Reason fields added) |
| Next Move after reach | HeldAccepted (not Replaced); new serial |
| Move→Move | Old serial Cancelled/Superseded (new); latest continues; latest Reach clears latest Held |
| Move→Attack | Cancelled/CommandReplaced for old move serial; Attack Held retained |
| Attack→Move | RequestMove Accepted; later Reach clears Move Held |
| QueueDeferred | Unchanged; no serial; no movement sync |
| Remote client authority path | Server authority bind/handler only; same as S21/S22 structural model |
| Multi-unit | Per-unit bind; exact serial local Held only |
| EndPlay | Silent movement stop; HeldCleared; no Cancelled emit |
| `gp.Movement.Stop` | Now clears matching Held via Cancelled/Manual |
| `gp.Movement.TestCompletion` | Alias to Reached synthetic; still no movement mutation |

---

## Deferred

- `Failed` result value and all Nav/blocked/timeout producers
- Request rejection UX / UI
- Dedicated command-layer Cancel API (beyond `StopMove(Manual)`)
- Attack / Mine executors consuming the contract
- Queue execution advancing on HeldMoveFinished
- Prediction / replicated Held
- Formation / avoidance
- Changing TickWithoutAuthority into a gameplay Failed/Cancelled product

---

## Target Final Status

**`DONE_WITH_FAILED_RESULT_DEFERRED`**

Means: Reached / Cancelled / sync Rejected contracts implemented and operator-accepted; exact-serial Held policy applied; EndPlay silent; Attack/Nav/`Failed` not implemented.

Implementation status labels (later):

| Phase | Label |
| --- | --- |
| This analysis | `ANALYSIS_READY_IMPLEMENTATION_PENDING` |
| After code candidate | `CODE_READY_OPERATOR_VALIDATION_PENDING` |
| After operator accept | `CODE_DONE_OPERATOR_ACCEPTED` |
| Stage close | `DONE_WITH_FAILED_RESULT_DEFERRED` |

Do **not** mark `CODE_DONE_NETWORK_VALIDATED` unless remote GP-S23 result paths are separately executed.

---

## Implementation record (candidate)

### Actual APIs
```cpp
enum class EGP_MovementResult : uint8 { Reached, Cancelled };
enum class EGP_MovementResultReason : uint8 { None, Superseded, CommandReplaced, Manual };
enum class EGP_MovementRequestStatus : uint8 { Accepted, Rejected };
enum class EGP_MovementRejectReason : uint8 {
  None, MissingOwner, NoAuthority, InvalidSerial,
  InvalidDestination, InvalidMoveSpeed, InvalidAcceptanceRadius
};
struct FGP_MovementRequestOutcome { Status; RejectReason; IsAccepted(); };
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGP_OnMovementResult, uint32, EGP_MovementResult, EGP_MovementResultReason);
FGP_MovementRequestOutcome RequestMove(const FVector&, uint32);
FGP_OnMovementResult& OnMovementResult();
#if !UE_BUILD_SHIPPING
void DebugBroadcastResult(uint32, EGP_MovementResult, EGP_MovementResultReason);
#endif
```

### Migration names
| Old (GP-S22) | New (GP-S23) |
| --- | --- |
| `EGP_MovementCompletionResult` | `EGP_MovementResult` |
| `FGP_OnMovementCompleted` | `FGP_OnMovementResult` (+ Reason) |
| `OnMovementCompleted()` | `OnMovementResult()` |
| `HandleMovementCompleted` | `HandleMovementResult` |
| `DebugBroadcastCompletion` | `DebugBroadcastResult` |
| `HeldMoveCompleted` | `HeldMoveFinished` |
| `MovementCompletionIgnored` | `MovementResultIgnored` |

### Rejection behavior
- `RequestMove` returns structured outcome; **never** broadcasts Rejected.
- Reject does not mutate active movement state.
- Command layer: Held set → `RequestMove` → on Reject clear exact Move serial → `MoveExecutionRejected` + `HeldMoveRejectedCleared` → **no** `HeldAccepted`/`HeldReplaced`.
- Allocator continues monotonically.
- Non-shipping `gp.UnitCommand.TestRejectedMove` exercises Held-before-RequestMove via non-finite destination.

### Cancellation ordering
- Move→Move: commit new active → `MoveReplaced` → Broadcast Cancelled/Superseded (old) → return Accepted.
- Move→non-Move: Held already new → `StopMove(CommandReplaced)` → Cancelled/CommandReplaced → ignore HeldTagNotMove.
- Manual: Cancelled/Manual → clear matching Held Move.
- EndPlay: silent clear; no broadcast.

### Held policy
Exact authority + Move tag + serial match clears on Reached or Cancelled. Else ignore with `MovementResultIgnored`.

### Debug commands
| Command | Role |
| --- | --- |
| `gp.Movement.TestResult <Serial> <Reached\|Cancelled> [Reason]` | Primary synthetic (no mutation) |
| `gp.Movement.TestCompletion <Serial>` | Deprecated alias → Reached/None |
| `gp.Movement.Stop` | Real Manual cancel producer — selects first **moving** authority unit only (no idle fallback) |
| `gp.Movement.Test` | Structured RequestMove outcome |
| `gp.UnitCommand.TestRejectedMove` | Command-layer phantom-Held reject path |

### Builds
- GPEditor Win64 Development — **PASSED** (candidate + Stop target fix)
- UHT — **PASSED** (processed with builds)
- GP Dev / Shipping — deferred to finalization

### Operator validation
**CODE_READY_OPERATOR_VALIDATION_PENDING** (Manual retest required after debug Stop target fix).

| Case | Result |
| --- | --- |
| Natural Reached | **PASS** |
| Move→Move Superseded | **PASS** |
| Move→Attack CommandReplaced | **PASS** |
| Rejected Move (phantom Held clear) | **PASS** |
| Stale result | **PASS** |
| Compatibility alias TestCompletion | **PASS** |
| EndPlay | **PASS** |
| Manual Stop (`gp.Movement.Stop`) | **RETEST** — wrong debug target selected idle first authority; production `StopMove` contract not at fault |

Debug fix: `gp.Movement.Stop` now uses `FindFirstAuthorityMovingMobileUnit`; no fallback; logs `ActiveSerialBefore` / `WasMovingBefore` / `Selection=MovingUnit`.

---

## Stop condition (candidate)

**CODE_READY_OPERATOR_VALIDATION_PENDING.**
Manual cancellation requires re-validation after Stop target fix.
Commit/push `feature/gp-s23-movement-result-implementation` only.
Do **not** merge to main.
Do **not** start Attack/Mine/Nav/`Failed`/queue from this candidate.
