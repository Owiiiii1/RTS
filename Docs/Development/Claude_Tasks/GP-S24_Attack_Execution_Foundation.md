# GP-S24 Attack Execution Foundation

## Status
**CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `462f9bba0bc64c5a7da4fdd5eae9207f36524a7f` (Merge GP-S24 attack execution analysis)

Depends on: GP-S23 `DONE_WITH_FAILED_RESULT_DEFERRED`; GP-S24 analysis merged.

Branch: `feature/gp-s24-attack-execution-implementation`

Target stage close after operator validation: **`DONE_WITH_DAMAGE_DEFERRED`**

---

## Current code findings

### Hierarchy / ownership
```text
AGP_UnitBase (TeamId, UnitCommandComponent, ReceiveCommand)
← AGP_MobileUnit (UGP_MovementComponent)
← AGP_Unit
```

| Fact | Detail |
| --- | --- |
| Held owner | `UGP_UnitCommandComponent` on `AGP_UnitBase` |
| Serial allocator | `NextCommandSerial` (1+; wraps past 0; QueueDeferred does not allocate) |
| Movement | Authority XY straight-line; `RequestMove` → `FGP_MovementRequestOutcome`; terminal `OnMovementResult` |
| Held Move clear | `HandleMovementResult` clears **only** exact Held **Move** tag + serial |
| Non-Move Held + moving | `StopMove(CommandReplaced)` → Cancelled/CommandReplaced; Held Attack already installed |
| Attack delivery | `BuildSmartCommand` / `ValidateAndNormalizeCommand` already produce `Command_Attack` + `TargetActor` |
| ASC / attributes on units | **Not wired** — `UGP_UnitAttributeSet::AttackRange` exists in GPGASRuntime but UnitBase comment: ASC deferred |
| Damage / death | Not implemented on units |
| Tick on command component | `bCanEverTick = false` today |

### Attack-related call-site inventory

| Symbol / path | Actual behavior today |
| --- | --- |
| `FGPGameplayTags::Command_Attack` | Native tag `GP.Command.Attack` |
| `UGP_CommandComponent::BuildSmartCommand` | Enemy UnitBase → Attack; same team → Move; neutral/unassigned UnitBase → speculative Attack |
| `ValidateAndNormalizeCommand` Attack | Requires valid `AGP_UnitBase` target; rejects same-team (`FriendlyAttackTarget`); allows TeamId 0/-1; snapshots target location |
| `DispatchValidatedCommand` | Builds `FGP_UnitCommand` → `ReceiveCommand` |
| `AGP_UnitBase::ReceiveCommand` | Authority log → `UnitCommandComponent->HandleCommand` |
| `HandleCommand` | Stores Held for Attack; `SynchronizeMovementWithHeldCommand` only **stops** active move (does not start Attack executor) |
| `TargetActor` | Delivery: raw `AActor*`; Held: `TWeakObjectPtr<AActor>` |
| `RequestMove` / `HandleMovementResult` | Move Held only; Attack Held → `MovementResultIgnored HeldTagNotMove` |
| `ClearHeldCommand` | Private; EndPlay HeldCleared; Move reject / Move finish / (future Attack terminal) |
| Team | `AGP_UnitBase::GetTeamId()` — `-1` unassigned, `0` neutral, `1+` playable |
| Actor destruction | No Attack-specific bind today; Held uses weak ptr |

### Gap after GP-S23
Attack is accepted into Held and cancels prior movement, then **idles**. No validation re-check, no approach, no Ready, no terminal Attack lifecycle.

---

## Problem

Need a minimal authority-only Attack executor that proves composite lifecycle **without damage**:

```text
Held Attack
├─ validate target
├─ Approaching (optional RequestMove tied to Attack serial)
├─ Ready (in range; Held retained; track target)
└─ Terminal (clear Held + reset executor) OR replace by newer command
```

Must integrate with GP-S23 result contract without double-consuming movement results or clearing Attack Held as if it were Move.

---

## Goals

1. Authority-only Attack executor for Held `Command_Attack`.
2. Target validation matrix aligned with existing command validation.
3. Approach via existing `UGP_MovementComponent` when out of range.
4. Ready state without damage / GAS / projectiles.
5. Exact serial / stale-safe movement result handling.
6. Deterministic terminal / replace / EndPlay behavior.
7. Operator-validatable with gameplay input + minimal non-shipping hooks.

## Non-Goals

Damage, health, death, cooldown, wind-up, animation, projectile, hit reaction, GAS abilities/effects, armor, aggro, auto-target, chase timeout, NavMesh, avoidance, formation, queue execution, UI, replicated Attack state, prediction, AttackMove, Mine executor.

---

## Selected ownership architecture

### Selection: **Option A — Attack executor state machine inside `UGP_UnitCommandComponent`**

Private plain-C++ runtime fields + private methods on the existing Held owner. No new UObject component in GP-S24. No generic executor framework.

| Criterion | Why A wins on GP-S24 |
| --- | --- |
| Minimal scope | One file pair already owns Held, serials, movement sync, movement result bind |
| Authority-only | Same BeginPlay bind gate as GP-S22/S23 |
| Serial safety | Same allocator; Attack serial == Held serial |
| No Blueprint / replication | Private state; no UPROPERTY runtime machine |
| Future Mine | Mine can add a sibling private machine later; extract shared base only when duplication is real |
| No cycles | Still depends downward on Movement via MobileUnit getter |
| Lifecycle | Single EndPlay already unbinds movement + clears Held |

### Rejected alternatives

| Option | Verdict |
| --- | --- |
| B. `UGP_AttackComponent` | Extra subobject, EndPlay/bind order vs command Held, split of “who clears Held”; no payoff until Mine/combat shares it |
| C. Generic executor framework now | Premature abstraction; violates project “no framework without necessity” |
| D. Put machine on `UGP_MovementComponent` | Wrong layer; Attack is command lifecycle, not locomotion |

---

## Runtime state model

```cpp
enum class EGP_AttackExecutionState : uint8
{
	Idle,
	Approaching,
	Ready
};

enum class EGP_AttackTerminalResult : uint8
{
	Cancelled,
	Failed
};

enum class EGP_AttackTerminalReason : uint8
{
	CommandReplaced,   // newer non-queued command replaced Held
	InvalidTarget,     // failed validation (accept-time or runtime)
	TargetDestroyed,   // weak target lost / pending kill
	MovementRejected,  // approach RequestMove rejected
	MovementCancelled, // approach cancelled externally (e.g. Manual stop) while not range-entry stop
	EndPlay            // teardown (silent clear preferred; reason for logs only)
};
```

### Persistent executor fields (private on `UGP_UnitCommandComponent`)

| Field | Role |
| --- | --- |
| `AttackState` | Idle / Approaching / Ready |
| `ActiveAttackSerial` | Held Attack serial while executor active; `0` when Idle |
| `AttackTarget` | `TWeakObjectPtr<AGP_UnitBase>` (narrowed from Held Actor) |
| `LastApproachDestination` | Last issued approach XY destination |
| `LastApproachIssueTime` | World time of last `RequestMove` for throttle |
| `bExpectRangeEntryStop` | Set only around executor-issued in-range `StopMove(Manual)` |

**No separate Validating enum state.** Validation is a synchronous function at start and on each reevaluation.

**Ready is not terminal.** Held Attack remains until replace, invalidation, destruction, or EndPlay.

---

## Serial / sub-operation model

### Selection: **Option A — Attack Held serial is the approach movement serial**

```text
Held Attack Serial = N
→ RequestMove(dest, N)
→ wait OnMovementResult Serial=N
```

| Option | Verdict |
| --- | --- |
| A. Attack serial == movement serial | **Selected** — matches GP-S23 Attack-waiter design; no second allocator |
| B. Separate sub-operation serial | Extra allocator / collision rules; not required if self-supersede guard exists |
| C. Compound token | Overkill for one approach channel |

### Stale / reissue guards (required because A reuses serial)

When approach reissues `RequestMove` with the same serial `N` while already moving, MovementComponent broadcasts `Cancelled/Superseded` for previous `N` **after** committing new active `N`.

**Rule:** while `AttackState == Approaching` and result is `Cancelled/Superseded` for `ActiveAttackSerial`:

- if `Movement->IsMoving() && Movement->GetActiveMoveSerial() == ActiveAttackSerial` → **ignore** (self-supersede reissue);
- else → terminal `Failed` / `MovementCancelled` (external cancel path).

External `CommandReplaced` stop clears movement before new Held Attack starts its own approach — old Cancelled uses old serial; new Attack serial is `N+1` → ignore via serial mismatch.

### Allocator
Never rewind. Attack accept always uses `AllocateCommandSerial()` via existing `HandleCommand` path.

---

## Target validation matrix

Executor validates `AGP_UnitBase*` only (matches server command validation). **No new attackable interface in GP-S24.**

| Check | Fail → |
| --- | --- |
| Owner exists + authority | ignore / no-op |
| Held is Attack + serial == `ActiveAttackSerial` (when executor active) | ignore / reset inconsistency |
| Target weak valid (`IsValid`) | `Failed` / `TargetDestroyed` or accept-time `InvalidTarget` |
| Target != Owner | `Failed` / `InvalidTarget` |
| Not pending kill / tear-off | `Failed` / `TargetDestroyed` |
| `Cast<AGP_UnitBase>` | `Failed` / `InvalidTarget` |
| Same world as owner | `Failed` / `InvalidTarget` |
| Target location finite | `Failed` / `InvalidTarget` |
| Owner `TeamId >= 1` | `Failed` / `InvalidTarget` |
| Target `TeamId == Owner.TeamId` | `Failed` / `InvalidTarget` (defense; server already rejects) |
| Target TeamId `0` or `-1` or other playable | **Allowed** (matches current `ValidateAndNormalizeCommand`) |

Health/damage interfaces: **not used**.

### Accept-time vs runtime

| When | Behavior |
| --- | --- |
| After Held Attack installed, first executor start | Sync validate; on fail → clear Held Attack + reset Idle + `AttackRejected` / `AttackFinished Failed InvalidTarget` (**no phantom Held**) |
| During Approaching / Ready | Revalidate each tick window; destroyed → `Failed/TargetDestroyed`; other invalid → `Failed/InvalidTarget` |

---

## Attack range model

| Item | Locked choice |
| --- | --- |
| Storage | `UPROPERTY(EditDefaultsOnly) float AttackRange = 250.f` on `UGP_UnitCommandComponent` |
| GAS `AttackRange` attribute | **Not read in GP-S24** (ASC not on units) |
| Distance | `Distance2D(Owner.XY, Target.XY) <= AttackRange` |
| Capsule radii | **Ignored** (deterministic minimal) |
| Hysteresis | **None** in GP-S24; repath throttle prevents churn |
| Approach destination | Target current XY; **Z = owner Z** (preserve; matches Move Z policy) |
| Movement `AcceptanceRadius` | **Unchanged globally** (still ~50 for Move) |
| In-range while Approaching | If `Distance2D <= AttackRange` before natural Reached → set `bExpectRangeEntryStop`, `StopMove(Manual)`, enter Ready on matching Cancelled/Manual |
| Natural Reached | If still valid and `Distance2D <= AttackRange` → Ready; if valid but out of range (target fled) → reissue approach; else terminal |

---

## Target tracking model

### Selection: **Option A — authority-only Tick while Attack active**

| Option | Verdict |
| --- | --- |
| A. Component Tick | **Selected** — enable only when `AttackState != Idle`; disable on Idle |
| B. Timer | Extra handle; same cost |
| C. Actor delegates + occasional reevaluation | Target `OnDestroyed` optional later; not required if tick validates `IsValid` |
| D. Movement callback only | Insufficient for Ready exit / target walk-away |

### Tick policy

| Item | Value |
| --- | --- |
| Enable | Enter Approaching or Ready |
| Disable | Enter Idle (terminal / replace / EndPlay) |
| Work | Validate target; compute Distance2D; Ready↔Approaching transitions; approach reissue |
| Spam | Log only on state transitions / issue / terminal — never per-tick distance logs |

### Approach reissue (moving target)

Issue/reissue `RequestMove` only when Approaching and **all** hold:

1. target valid and out of range;
2. `Distance2D(LastApproachDestination.XY, Target.XY) >= ApproachRetargetThreshold` (default **100** uu);
3. `Now - LastApproachIssueTime >= ApproachReissueInterval` (default **0.25** s);

Else keep current approach. Self-supersede ignore rule applies.

---

## Movement integration matrix

### Start approach
```text
Held Attack N already installed
→ AttackState = Approaching
→ ActiveAttackSerial = N
→ Outcome = RequestMove(TargetXY + OwnerZ, N)
→ Accepted: AttackApproachRequested
→ Rejected: AttackApproachRejected + terminal Failed/MovementRejected + clear Held
```

### Result routing contract (critical)

**Single subscriber remains:** `UnitCommandComponent::HandleMovementResult` (existing bind).

Canonical order inside handler:

```text
1. Authority / supported result checks
2. If AttackState == Approaching && Serial == ActiveAttackSerial:
     → AttackHandleMovementResult(...)   // consume; return
3. Else existing Held Move exact-serial clear path
4. Else MovementResultIgnored (unchanged reasons)
```

| Concern | Policy |
| --- | --- |
| Double consumption | Impossible — Attack branch returns before Move clear |
| Misleading `HeldTagNotMove` for approach | **Eliminated** for matching Attack approach serial |
| Separate Attack multicast subscriber | **Forbidden** in GP-S24 |

### Approaching movement outcomes

| Movement result | Attack action |
| --- | --- |
| Reached | valid+in range → Ready; valid+out of range → reissue; invalid → Failed/InvalidTarget or TargetDestroyed |
| Cancelled/Superseded | self-supersede ignore **or** external → Failed/MovementCancelled |
| Cancelled/CommandReplaced | normally Attack serial already replaced (new Held); if still matching Approaching → Failed/MovementCancelled then replace path usually already cleared executor |
| Cancelled/Manual + `bExpectRangeEntryStop` | clear flag → Ready (if still valid+in range) else reissue/terminal |
| Cancelled/Manual without flag | Failed/MovementCancelled + clear Held Attack |
| Sync RequestMove Rejected | Failed/MovementRejected + clear Held Attack |

Move Held clear path **never** clears Attack Held.

---

## Held mutation policy

| Event | Held Attack | Executor |
| --- | --- | --- |
| Accept valid Attack | installed by `HandleCommand` | start after movement sync |
| Accept-time invalid | cleared immediately | stay Idle |
| Reach Ready | **retain** | Ready |
| Terminal Failed/Cancelled | clear | Idle |
| Replaced by newer command | new Held already installed | reset old executor **before** starting new work |
| QueueDeferred | unchanged | unchanged |
| EndPlay | HeldCleared (existing) | silent Idle reset; unbind movement first |

**Who clears Held Attack:** only `UGP_UnitCommandComponent` (executor terminal / accept reject / EndPlay / replace overwrite). No Attack-specific public delegate required in GP-S24.

**AttackReady:** log/event only (`AttackReady`); not a Held clear.

---

## Replacement / retarget ordering

### Canonical replace sequence (extends GP-S23)

```text
HandleCommand (non-queue):
1. Previous = Held
2. Allocate serial; Held = new command
3. ResetAttackExecutorForReplacement(Previous)  // Idle; clear flags; do not clear new Held
4. SynchronizeMovementWithHeldCommand(Previous)
   - Move: RequestMove / reject clear Move Held
   - non-Move: StopMove(CommandReplaced) if moving
5. If Held still set && Held is Attack: StartAttackExecutor(Held)
6. HeldAccepted / HeldReplaced logs
```

`ResetAttackExecutorForReplacement`:

- if previous was Attack Approaching/Ready: log `AttackCancelled Reason=CommandReplaced` (no second Held clear — already overwritten);
- zero `ActiveAttackSerial`; `AttackState=Idle`; clear target/flags;
- any in-flight Cancelled for old approach serial sees Idle/mismatch → ignore.

### Replacement matrix

| Old state | New command | Expected |
| --- | --- | --- |
| Attack Approaching | Move | reset Attack Idle; RequestMove for Move; Cancelled/CommandReplaced or Superseded for old approach ignored vs Move Held |
| Attack Approaching | Attack new target | reset; StopMove if needed; start new Attack N+1 |
| Attack Ready | Move | reset Attack; RequestMove Move |
| Attack Ready | Attack new target | reset; start new Attack |
| Attack active | Mine | reset Attack; StopMove if approaching; Mine Held idle (Mine executor deferred) |
| Move active | Attack | existing StopMove CommandReplaced + start Attack |
| Attack active | Queue=true | QueueDeferred; no mutation |

### Attack→Attack retarget

```text
Attack N target A (Approaching)
→ Attack N+1 target B installed
→ ResetAttackExecutor (Idle; ActiveAttackSerial=0)
→ StopMove(CommandReplaced) if still moving under N
→ Cancelled/CommandReplaced Serial=N → ignored (executor Idle / serial mismatch)
→ StartAttackExecutor for N+1 / B
```

Old movement callbacks must not write Ready/Approaching for N+1.

---

## Reentrancy guarantees

1. Capture locals (serials, state, target) before mutation.
2. Mutate executor / Held / movement locally.
3. Log transition.
4. Only then call `RequestMove` / `StopMove` (may broadcast).
5. After broadcast returns, do not apply logic for superseded Attack serial.
6. Callbacks may call into `HandleCommand` / `RequestMove`; guards use `ActiveAttackSerial` + state.

Mirror GP-S23: **no post-broadcast mutation of the completed serial’s movement state.**

---

## Lifecycle / EndPlay

| Step | Policy |
| --- | --- |
| BeginPlay | Existing movement bind (authority MobileUnit) |
| Attack start | Enable component tick |
| Attack Idle | Disable component tick |
| Target destroyed | Detected via `IsValid` on tick / before issue; terminal Failed/TargetDestroyed |
| EndPlay | Unbind movement → silent Attack Idle reset (no Attack terminal broadcast/delegate) → existing HeldCleared → Super |
| Attack EndPlay vs Movement | Align with GP-S23: **silent teardown**; log `AttackCancelled Reason=EndPlay` at most once as diagnostic, not a gameplay consumer API |

No Attack multicast delegate in GP-S24 (logs only). Future combat UI can add later.

---

## Logging contract

Minimal transition logs (no per-tick spam):

| Event | When | Key fields |
| --- | --- | --- |
| `AttackAccepted` | Executor starts after Held Attack accept | Unit, AttackSerial, Target, State, AttackRange, Role, NetMode |
| `AttackRejected` | Accept-time validation fail (Held cleared) | Unit, AttackSerial, Target, Reason, Role, NetMode |
| `AttackStateChanged` | Idle↔Approaching↔Ready | Unit, AttackSerial, Target, OldState, NewState, Distance, AttackRange |
| `AttackApproachRequested` | RequestMove Accepted for approach | Unit, AttackSerial, MovementSerial(=AttackSerial), Destination, Target |
| `AttackApproachRejected` | RequestMove Rejected | Unit, AttackSerial, RejectReason |
| `AttackApproachResult` | Consumed movement result | Unit, AttackSerial, MovementResult, MovementReason, Distance |
| `AttackReady` | Enter Ready | Unit, AttackSerial, Target, Distance |
| `AttackFinished` | Terminal clear Held | Unit, AttackSerial, Result, Reason, Target |
| `AttackResultIgnored` | Stale / self-supersede / mismatch | Unit, AttackSerial, ResultSerial, IgnoreReason |

`AttackCancelled` may alias into `AttackFinished Result=Cancelled` — **use one:** `AttackFinished`.

---

## Debug hooks

Non-shipping only; minimal:

| Hook | Purpose |
| --- | --- |
| `gp.Attack.Dump` | Log first authority unit AttackState / serial / target / distance / Held |
| Gameplay RMB Attack | Primary validation path (in/out range, retarget, Move replace) |
| Destroy target actor | TargetDestroyed during Approaching/Ready |
| `gp.Movement.Stop` | Manual cancel during approach (expects AttackFinished MovementCancelled) |
| Existing `gp.Movement.TestResult` | Stale serial against Attack Approaching |

No force-damage hooks. No public production Attack API beyond Held + executor.

---

## Expected implementation files

| File | Change |
| --- | --- |
| `GPUnitCommandComponent.h/.cpp` | Attack state, tick, validation, approach, routing in `HandleMovementResult`, replace hooks |
| Optional `GPAttackTypes.h` (Public or Private Units) | Plain enums only if header clutter requires split |
| `GP-S24_Attack_Execution_Foundation.md` | Implementation record |
| `AI_Project_Log.md` / `Cursor_Work_Report.md` | Checkpoints |

| File | GP-S24 change? |
| --- | --- |
| `GPMovementComponent` | **NO** unless blocked (prefer no) |
| `GPMobileUnit` / `GPUnitBase` | **NO** |
| `GPStoredUnitCommand` / `GPUnitCommand` | **NO** |
| Gameplay tags | **NO** |
| `GPCommandComponent` | **NO** (validation already sufficient) |
| `GPRuntime.Build.cs` | **NO** |
| GAS / AttributeSet wiring | **NO** |
| Assets / maps / config | **NO** |

---

## Build impact

| Phase | Builds |
| --- | --- |
| Analysis | None |
| Implementation candidate | GPEditor Win64 Development + UHT |
| Finalization | GP Win64 Development + GP Win64 Shipping |

---

## Operator validation plan

2P Listen Server. Authority-focused.

| ID | Scenario | Expected |
| --- | --- | --- |
| A | Attack target already in range | No approach RequestMove (or immediate range-entry stop); `AttackReady`; Held Attack retained |
| B | Attack target outside range | `AttackApproachRequested`; on range entry → Ready; Held retained |
| C | Target moves farther during approach | reissue after threshold; self-supersede ignored; eventually Ready |
| D | Target exits range from Ready | `AttackStateChanged` → Approaching; new approach |
| E | Move replaces Attack | AttackFinished/Cancelled CommandReplaced path; Move Held; no stale Attack Ready |
| F | Attack retarget A→B | old results ignored; B active |
| G | Invalid / same-team / self | reject at server and/or `AttackRejected`; no phantom Held |
| H | Target destroyed during approach | AttackFinished Failed TargetDestroyed; movement stopped; Held cleared |
| I | Target destroyed in Ready | same terminal |
| J | Queue=true while Attack | QueueDeferred; no mutation |
| K | Remote Team 2 | authority-only executor; no client duplicate |
| L | Multi-unit | independent Attack states |
| M | EndPlay during Attack | no crash; silent teardown |

---

## Deferred scope

- Damage / health / death / kill credit
- Attack interval / cooldown / wind-up / animations / projectiles
- GAS ability Attack / GameplayEffects / AttackRange attribute binding
- Armor / resistance / threat / aggro / auto-acquire / AttackMove
- NavMesh / pathfinding / avoidance / formation
- Queue execution after AttackFinished
- Replicated Attack state / prediction / UI
- Mine executor (may copy this pattern)
- Dedicated `UGP_AttackComponent` extraction
- Attack multicast delegate for external systems

---

## Exact implementation task outline

1. Add private Attack enums/fields + `AttackRange` / retarget threshold / reissue interval defaults on `UGP_UnitCommandComponent`.
2. Enable authority Tick only while Attack active.
3. Hook `HandleCommand` after Held+movement sync: `StartAttackExecutor` for Attack; `ResetAttackExecutorForReplacement` when replacing.
4. Implement validate / approach issue / Ready / terminal clear helpers.
5. Extend `HandleMovementResult` with Attack Approaching consume-first routing + self-supersede + range-entry Manual flag.
6. Add transition logs + `gp.Attack.Dump`.
7. Update GP-S24 doc status; candidate GPEditor+UHT; operator plan A–M; finalization GP Dev/Shipping.
8. Do **not** wire GAS damage or change MovementComponent API unless blocked.

---

## Final recommended status name

**`DONE_WITH_DAMAGE_DEFERRED`**

Means: Attack composite lifecycle (validate / approach / Ready / terminal / replace) implemented and operator-accepted; no damage/health/combat effects; Nav/`Failed` movement still deferred at movement layer.

| Phase label | When |
| --- | --- |
| `ANALYSIS_READY_IMPLEMENTATION_PENDING` | This document |
| `CODE_READY_OPERATOR_VALIDATION_PENDING` | After candidate implementation |
| `CODE_DONE_OPERATOR_ACCEPTED` | After operator accept |
| `DONE_WITH_DAMAGE_DEFERRED` | Stage close |

---

## Implementation record (candidate)

### Files
- `GP/Source/GPRuntime/Public/Units/GPUnitCommandComponent.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- (no MovementComponent / UnitBase / tags / Build.cs changes)

### Enums
`EGP_AttackExecutionState { Idle, Approaching, Ready }`
`EGP_AttackTerminalResult { Cancelled, Failed }`
`EGP_AttackTerminalReason { CommandReplaced, InvalidTarget, TargetDestroyed, MovementRejected, MovementCancelled, EndPlay }`

### Config defaults
`AttackRange=250`, `AttackReissueDistance=100`, `AttackReissueInterval=0.25` (EditDefaultsOnly on UnitCommandComponent; GAS attribute unused)

### Runtime fields
`AttackState`, `ActiveAttackSerial`, `AttackTarget` (weak UnitBase), `LastApproachDestination`, `LastApproachIssueTime`, `bExpectRangeEntryStop`, `bFinishingAttack`

### Tick policy
`bCanEverTick=true`, start disabled; enable only while Attack active + authority; disable on Idle / EndPlay.

### Target validation
Owner authority UnitBase; valid UnitBase target ≠ self; same world; finite location; owner TeamId≥1; target TeamId ≠ owner (0/-1 allowed). Destroyed → TargetDestroyed; else InvalidTarget.

### Serial / routing
Attack Held serial == approach `RequestMove` serial. `HandleMovementResult` → `TryConsumeAttackMovementResult` first, then Held Move path. Self-supersede Cancelled/Superseded ignored while still moving on same serial. Range-entry uses Manual stop + `bExpectRangeEntryStop`.

### Ready / Held
Ready retains Held Attack; no damage. Terminal / accept reject clears exact Attack Held. Replacement: `ResetAttackExecutorForReplacement` (no Held clear) → sync → `StartAttackExecutor`.

### Debug commands (non-shipping)
`gp.Attack.Inspect`, `gp.Attack.DestroyTarget`, `gp.Attack.MoveTarget X Y`, `gp.Attack.TestInvalid <Self|Friendly|Null>`

### Builds
GPEditor Win64 Development + UHT — **PASSED**. GP Dev/Shipping deferred to finalization.

### Operator validation
**Pending.** Plan A–M in this document (in-range Ready, approach, reissue, Ready→Approaching, Move replace, retarget, invalid, destroy, QueueDeferred, remote, multi-unit, EndPlay).

---

## Stop condition (candidate)

**CODE_READY_OPERATOR_VALIDATION_PENDING.**
Commit/push `feature/gp-s24-attack-execution-implementation` only.
Do **not** merge to main.
Do **not** start damage/GAS/Nav/Mine/queue from this candidate.
