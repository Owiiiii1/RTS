# GP-S20 — Movement Foundation
(Server-authoritative unit movement foundation after GP-S18 Held Command — analysis)

## Status
**Status: CODE_DONE_NETWORK_VALIDATED**

**GP-S20 Status: `DONE_WITH_COMMAND_INTEGRATION_DEFERRED`**

Implemented and operator-validated on `feature/gp-s20-movement-foundation-implementation`
(base = `main` `416ba39bba38c4a85e692ffef700443f5c8abd15`).
Depends on: GP-S18 `DONE_WITH_EXECUTORS_DEFERRED`.

**Canonical stage: GP-S20 — Movement Foundation.**
**No** Held Command integration. **No** navigation / AIController.

TDD/13 is **not** rewritten. Stage mapping below is **project continuation mapping**, not a change to historical TDD numbering.

---

## 1. Canonical roadmap mapping

| Stage | Continuation mapping (project) | Historical TDD/13 |
| --- | --- | --- |
| **GP-S19** | `FGP_CommandRequest` / command mapping. **Absorbed and completed by GP-S17.** | Same intent; not movement |
| **GP-S20** | **Movement Foundation.** Canonical next implementation stage. | `UGP_MovementComponent` (NavMesh, MoveTo) — Nav deferred within S20 checkpoint |
| **GP-S21** | Held Move command integration and replacement cancellation. | TDD: `AGP_MobileUnit` base (owner pulled into S20) |
| **GP-S22** | Serial-aware movement completion/failure callback and Held Command clearing. | TDD: delivery routing (already done by GP-S17+S18 for delivery) |

Do **not** reuse GP-S19 for movement.

---

## 2. Current architecture (facts)

```text
Input → Server validation → Dispatch
→ AGP_UnitBase::ReceiveCommand
→ UGP_UnitCommandComponent → Held Command
```

| Fact | State |
| --- | --- |
| `AGP_UnitBase` | Abstract `APawn`; TeamId; CapabilityTags; UnitCommandComponent |
| `AGP_Unit` | Placeable; capsule r=42, hh=88; QueryOnly Visibility Block |
| Replication | `bReplicates` + `SetReplicateMovement(true)` |
| Movement / AI / Nav classes | **Absent** |
| AIModule / NavigationSystem | **Not** in Build.cs / `.uproject` |
| Saved map / NavMesh | **None** |
| Debug console commands | **None** in `GP/Source` |

---

## 3. Canonical GP-S20 hierarchy

```text
AGP_UnitBase
└── command / team / selection identity
    └── AGP_MobileUnit
        └── UGP_MovementComponent
            └── AGP_Unit
```

Inheritance:

```text
AGP_UnitBase
← AGP_MobileUnit
← AGP_Unit
```

**`AGP_UnitBase` does not receive a movement component.**

Reason: UnitBase may later base buildings / immobile objects; mobility is expressed by type; current `AGP_Unit` is the mobile concrete unit.

---

## 4. MobileUnit contract (locked)

```cpp
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_MobileUnit : public AGP_UnitBase
```

Matches `AGP_UnitBase` Abstract + Blueprintable style.

| Owns | Does not own |
| --- | --- |
| `UGP_MovementComponent` default subobject | Command routing / storage |
| Plain C++ `GetMovementComponent()` | Selection / team validation |
| Mobility composition boundary | Attack / mining / AIController / NavMesh |

`AGP_Unit` inherits from `AGP_MobileUnit`.

Proposed property style (match PC component convention):

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Movement", meta = (AllowPrivateAccess = "true"))
TObjectPtr<UGP_MovementComponent> MovementComponent;
```

---

## 5. Movement component inheritance (locked)

```cpp
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_MovementComponent : public UActorComponent
```

**Not** `UPawnMovementComponent` on GP-S20.

Reasons:

- first backend owns explicit server movement lifecycle
- current Pawn has no existing movement framework
- no Controller / Nav path following exists
- avoids premature engine movement abstraction before navigation design
- component boundary allows backend replacement later

Constraint: component uses owner transform internally. This is **not** ad-hoc movement inside `AGP_Unit`. All physical movement stays encapsulated in one replaceable backend.

---

## 6. Straight-line server movement backend (locked)

Name: **Straight-line server movement backend**

| Is | Is not |
| --- | --- |
| Real working movement backend | Temporary hack |
| Validates authority, replication, cancel, reach | Final obstacle-aware navigation |
| XY translation + optional yaw | Pathfinding promise |

May be replaced by a NavMesh backend later **without** changing PlayerController, command request, or Held Command payload.

---

## 7. Exact GP-S20 scope

### Implements

- `AGP_MobileUnit`
- `UGP_MovementComponent`
- Reparent `AGP_Unit`
- Authority-only `RequestMove`
- One active movement request
- Server-side straight-line movement
- Destination + active serial storage
- Stop inside acceptance radius
- Explicit `StopMove`
- Transform replication through owner Pawn
- Optional yaw rotation
- Diagnostic logs
- Standalone direct-test entry (non-shipping console command)

### Does not implement

- Reading Held Command / tag routing / auto-start from `UnitCommandComponent`
- Command completion callbacks / clearing Held Command
- Attack-driven replacement / queue
- NavMesh / AIController / pathfinding / avoidance / formation

---

## 8. Direct validation mechanism (locked)

No existing console-command / CheatManager pattern in `GP/Source`.

**Canonical:** non-shipping C++ console command registered with `UGP_MovementComponent` implementation translation unit.

| Item | Decision |
| --- | --- |
| File | `GPMovementComponent.cpp` under `#if !UE_BUILD_SHIPPING` |
| Mechanism | `FAutoConsoleCommand` (or equivalent `IConsoleManager` registration) |
| Behavior | Resolve first suitable `AGP_MobileUnit` (or by name arg) + destination XYZ; call `RequestMove` with a non-zero debug serial |
| Shipping | Command **not** compiled |
| Assets | **None** required |

**Rejected:** temporary `BeginPlay` auto-move — pollutes production actors, hard to disable, wrong lifecycle for network tests.

Exact command name (implementation-time): e.g. `gp.MoveUnit <X> <Y> [Z] [UnitName]` — finalize string at code time; semantics locked here.

---

## 9. Exact public API (locked)

```cpp
bool RequestMove(const FVector& Destination, uint32 CommandSerial);
void StopMove();
bool IsMoving() const;
uint32 GetActiveMoveSerial() const;
const FVector& GetMoveDestination() const;
```

| Rule | Policy |
| --- | --- |
| Exposure | Plain C++; **not** UFUNCTION |
| BlueprintCallable / RPC / delegates | **No** |
| `StopMove(uint32 ExpectedSerial)` | **Rejected for GP-S20** — serial-aware cancel is GP-S21 |

Movement component does **not** allocate serials.

---

## 10. Internal state (locked)

```cpp
FVector MoveDestination = FVector::ZeroVector;
uint32 ActiveMoveSerial = 0;
bool bIsMoving = false;
```

**No movement enum on GP-S20.** Idle/Moving covered by `bIsMoving`. Result taxonomy arrives with GP-S22 callbacks.

---

## 11. RequestMove policy (locked)

Authority-only.

| Check | On fail |
| --- | --- |
| Owner exists | Reject |
| Owner is expected mobile pawn (`AGP_MobileUnit` / `APawn` as implemented) | Reject |
| Owner has authority | Reject |
| Destination finite | Reject |
| `CommandSerial != 0` | Reject |

Rejection: `return false`; no state mutation; log `MoveRejected` with reason.

Acceptance:

1. If already moving → replace destination + serial; log `MoveReplaced`
2. Else → store destination/serial; `bIsMoving=true`; enable Tick; log `MoveStarted`

---

## 12. Tick policy (locked)

| Item | Policy |
| --- | --- |
| Constructor | `bCanEverTick = true`; tick **initially disabled** |
| Start | `RequestMove` enables tick |
| Stop / reach / EndPlay | disable tick |
| Transform mutation | **Authority only** |
| Clients | **Do not** run physical movement |
| Logs | **No** per-frame logs |

---

## 13. Physical algorithm (locked)

**XY straight-line; Z preserved from current actor location.**

Per authority Tick:

1. Read current location
2. XY distance to destination
3. If XY distance ≤ `AcceptanceRadius` → stop at **current** location (no snap); clear moving state; log `MoveReached`
4. Else step = `MoveSpeed * DeltaTime`, clamped to remaining XY distance
5. Next XY from direction; **preserve current Z**
6. `SetActorLocation(NextLocation, false)` — **no sweep**

Consequences (explicit):

- no obstacle stopping
- units may overlap
- landscape Z not followed
- suitable only for initial movement foundation validation
- **no MoveFailed** in GP-S20

---

## 14. Terrain / Z policy (locked)

- Preserve unit current Z throughout movement
- Ignore destination Z for physical motion (may store full destination for diagnostics)
- No line trace / terrain following / slope handling
- Non-flat terrain issues deferred with navigation backend

---

## 15. Acceptance policy (locked)

| Property | Default | Rationale |
| --- | --- | --- |
| `AcceptanceRadius` | `50.0f` | Slightly larger than capsule radius 42; 2D distance |

On reach:

- `bIsMoving = false`
- `ActiveMoveSerial = 0`
- tick disabled
- **retain** `MoveDestination` for diagnostics

No Held Command callback in GP-S20.

---

## 16. Rotation policy (locked)

| Property | Default |
| --- | --- |
| `bRotateToMovement` | `true` |
| `RotationSpeed` | `360.0f` deg/sec |

- Yaw only; shortest-angle interpolation via `FMath::RInterpConstantTo`
- Independent of translation step
- Server-authoritative; replicates with actor transform
- No pitch/roll / animation / controller rotation

---

## 17. Configuration defaults (locked)

| Property | Default | Specifiers |
| --- | --- | --- |
| `MoveSpeed` | `600.0f` | `EditDefaultsOnly, Category = "GP\|Movement"` |
| `AcceptanceRadius` | `50.0f` | same |
| `RotationSpeed` | `360.0f` | same |
| `bRotateToMovement` | `true` | same |

**Not** `EditAnywhere` / `BlueprintReadOnly` — project ActorComponents do not expose Blueprint tuning APIs today; matches `EditDefaultsOnly` identity defaults on UnitBase.

---

## 18. Replication (locked)

| Item | Policy |
| --- | --- |
| Component | Non-replicated |
| Destination / serial / `bIsMoving` | Not replicated |
| Motion visibility | Server updates location/rotation; existing `SetReplicateMovement(true)` |
| Prediction / unit RPC / multicast | **No** |
| Client movement Tick | **No** |

---

## 19. Collision policy (locked)

- No sweep
- Capsule remains for selection Visibility traces
- Movement does not stop at obstacles
- Overlap allowed
- Blocked/failure results **do not exist** yet

---

## 20. Logging contract (locked)

Category: `LogGPUnitMovement`

| Event | When |
| --- | --- |
| `MoveStarted` | First accept while idle |
| `MoveReplaced` | Accept while already moving |
| `MoveStopped` | Manual `StopMove` or EndPlay |
| `MoveReached` | Within acceptance radius |
| `MoveRejected` | Failed validation |

**Not** in GP-S20: `MoveFailed`, `StaleResultIgnored`.

Suggested fields: Unit, Serial, Destination, CurrentLocation, Speed, Role, NetMode.  
`MoveStopped`: previous serial + Reason=`Manual`|`EndPlay`.  
No command-replacement reason until GP-S21.

---

## 21. EndPlay (locked)

1. Disable Tick  
2. Clear active movement state (`bIsMoving=false`, `ActiveMoveSerial=0`)  
3. Optional `MoveStopped Reason=EndPlay` if was moving  
4. No callbacks  
5. `Super::EndPlay`

---

## 22. GP-S21 integration boundary

Future (not S20):

```text
UGP_UnitCommandComponent accepts Held Move
→ finds AGP_MobileUnit / UGP_MovementComponent
→ RequestMove(Destination, CommandSerial)
```

On non-queued replace: stop existing movement; if new is Move → start; if Attack/Mine → stop only.

---

## 23. GP-S22 completion boundary

Future (not S20):

```text
UGP_MovementComponent → completion(CommandSerial, Result)
→ UnitCommandComponent verifies held serial/tag
→ clears matching Held Command
```

GP-S20 only logs `MoveReached`. No delegate/callback yet.

---

## 24. Exact files

### New

| File | Role |
| --- | --- |
| `Public/Units/GPMobileUnit.h` | `AGP_MobileUnit` |
| `Private/Units/GPMobileUnit.cpp` | Subobject |
| `Public/Units/GPMovementComponent.h` | Component API |
| `Private/Units/GPMovementComponent.cpp` | Move + tick + `#if !UE_BUILD_SHIPPING` console command |

### Modified

| File | Role |
| --- | --- |
| `Public/Units/GPUnit.h` | Inherit `AGP_MobileUnit` |
| `Private/Units/GPUnit.cpp` | Ctor if needed |

### Unchanged

UnitBase, UnitCommandComponent, PlayerController, CommandComponent, Build.cs, tags, assets/maps/config/`.uproject`, TDD/13.

### Build.cs impact
**NO** — no AIModule / NavigationSystem / Mass / plugins.

---

## 25. Validation matrix (future implementation)

### Builds
GPEditor Dev / GP Dev / GP Shipping / UHT.

### Standalone (debug path)
Starts via console; visible server move; XY approach; Z preserved; stop in radius; `MoveReached`; tick off after reach.

### Replacement
Serial 1 then 2 before complete → `MoveReplaced`; active serial 2.

### Manual stop
`StopMove` → halt; serial cleared; tick off.

### 2P Listen Server
Host authority moves; remote simulated sees transform; no client execution; no duplicate; no RPC; no command integration.

### Regression
GP-S17/S18 unchanged; selection/input OK; no AI/GAS/Nav.

---

## 26. GP-S20 completion status

After code/build/operator validation:

**GP-S20 Status: `DONE_WITH_COMMAND_INTEGRATION_DEFERRED`**

Means: mobile hierarchy exists; physical server movement exists; transform replicates; direct lifecycle works.

Does **not** mean: Move command drives motion; pathfinding exists; command completion reported; queue/formation/avoidance exist.

---

## 27. Implementation record

### Actual hierarchy

```text
AGP_UnitBase ← AGP_MobileUnit ← AGP_Unit
```

`UGP_MovementComponent` default subobject on `AGP_MobileUnit` only.

### Compile deviation

`GetMovementComponent()` renamed to **`GetUnitMovementComponent()`** — `APawn::GetMovementComponent()` returns `UPawnMovementComponent*` and blocks covariant override.

### Actual metadata

- `AGP_MobileUnit`: `UCLASS(Abstract, Blueprintable)`
- `UGP_MovementComponent`: `UCLASS(ClassGroup=(GP), meta=(BlueprintSpawnableComponent))` : `UActorComponent`
- Tick: `bCanEverTick=true`, start disabled; enabled only while moving on authority
- Replication: component non-replicated; pawn `SetReplicateMovement(true)`

### Exact APIs

```cpp
bool RequestMove(const FVector& Destination, uint32 CommandSerial);
void StopMove();
bool IsMoving() const;
uint32 GetActiveMoveSerial() const;
const FVector& GetMoveDestination() const;
UGP_MovementComponent* GetUnitMovementComponent() const;
```

### Defaults

`MoveSpeed=600`, `AcceptanceRadius=50`, `RotationSpeed=360`, `bRotateToMovement=true` (`EditDefaultsOnly, BlueprintReadOnly`).

### Console validation (non-shipping only)

```text
gp.Movement.Test X Y [Serial]
gp.Movement.Stop
```

- Finds first authority `AGP_MobileUnit` in world
- Destination Z = unit current Z
- Default serial = 1
- Absent from Shipping builds (`#if !UE_BUILD_SHIPPING`)

### Builds

GPEditor Dev / GP Dev / GP Shipping / UHT — **PASSED**.

### Operator validation (Listen Server / 2P)

Validated pipeline:

```text
gp.Movement.Test / Stop
→ UGP_MovementComponent::RequestMove / StopMove
→ authority Tick XY straight-line
→ MoveStarted / MoveReplaced / MoveReached / MoveStopped
→ remote clients observe replicated Pawn transform
```

| Case | Result |
| --- | --- |
| MoveStarted (`gp.Movement.Test -4000 8000`, Serial=1, Speed=600, AcceptanceRadius=50, Role=Authority, NetMode=ListenServer) | **PASS** |
| Straight-line movement visible | **PASS** |
| Z preserved at 88 (start and reach) | **PASS** |
| MoveReached FinalLocation inside AcceptanceRadius (not exact destination snap) | **PASS** |
| Tick stops after reach (observed stop) | **PASS** |
| Second independent move after completion | **PASS** |
| Debug serial reuse after completion accepted | **PASS** (by design; component does not allocate/order serials) |
| MoveReplaced PreviousSerial=2 → NewSerial=1 | **PASS** |
| Manual `gp.Movement.Stop` MoveStopped Reason=Manual | **PASS** |
| Remote client sees transform; no duplicate client execution; no RPC warnings | **PASS** |
| Selection / camera / input regression | **PASS** |
| RMB still Held Command only; no auto-move from Held Move | **PASS** |
| No AI / Nav / GAS | **PASS** |

### Retained limitations

- no sweep / collision blocking / pathfinding / NavMesh / AIController
- no terrain following / formation / avoidance
- no completion callback / Held Command clearing
- no automatic RMB movement

---

## Stop condition
**CODE_DONE_NETWORK_VALIDATED.** **GP-S20: DONE_WITH_COMMAND_INTEGRATION_DEFERRED.**
Commit/push `feature/gp-s20-movement-foundation-implementation` only.
Do **not** merge to main.
Do **not** start GP-S21 Held wiring / GP-S22 callbacks / Nav / AI from this close-out.
Do **not** rewrite TDD/13.
