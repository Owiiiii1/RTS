# GP-S17 Phase E — Command Dispatch & Unit Receiver
(Final analysis contract — delivery boundary only)

## Status
**Status: ANALYSIS_READY_IMPLEMENTATION_PENDING**

Docs-only checkpoint. **No** C++ / assets / dispatch / receiver code in this pass.
Depends on: Phase D `CODE_DONE_NETWORK_VALIDATED`.
Phase E ends after synchronous delivery of command intent to unit receivers.
Gameplay execution remains deferred.

---

## 1. Phase E boundary (locked)

```text
Input
→ BuildSmartCommand
→ Server_RequestCommand
→ ValidateAndNormalizeCommand
→ DispatchValidatedCommand
→ AGP_UnitBase::ReceiveCommand
```

| In scope | Out of scope |
| --- | --- |
| Server dispatch orchestration | Movement / pathfinding / NavMesh |
| Per-unit `FGP_UnitCommand` payload | AI Controller commands |
| `AGP_UnitBase::ReceiveCommand` boundary | Attack / mining execution |
| Server diagnostic logs | GAS events / ability activation |
| Standalone + 2P delivery proof | Queue storage / current-command state |
| | Animations / resource changes |

### Terminology

| Stage | Meaning |
| --- | --- |
| **Accepted** | Server validation passed |
| **Delivered** | Receiver method invoked |
| **Received** | Unit boundary reached (log) — not gameplay |
| **Executed** | **Not used** in Phase E |

---

## 2. Dispatch ownership (locked)

| Role | Owner |
| --- | --- |
| RPC entry | `AGP_PlayerController` |
| Validation | `UGP_CommandComponent::ValidateAndNormalizeCommand` (**pure**) |
| Dispatch orchestration | `UGP_CommandComponent::DispatchValidatedCommand` |

Canonical Server RPC flow:

```text
ValidateAndNormalizeCommand
→ log Accepted
→ DispatchValidatedCommand
→ log Dispatch summary
→ return
```

- Dispatch is **not** inside the validator.
- No separate subsystem/service.

---

## 3. Exact dispatch API (locked)

```cpp
int32 DispatchValidatedCommand(
	const FGP_CommandRequest& ValidatedRequest) const;
```

| Contract | Value |
| --- | --- |
| Server-only | Yes |
| Synchronous | Yes |
| Storage | None |
| Return | Count of **actually invoked** `ReceiveCommand` calls (not gameplay executions) |
| Client RPC / rollback / retry / latent | **No** |

---

## 4. Authority policy (locked)

**CommandComponent dispatch guard:**

- owner exists;
- owner has authority.

**Unit guard (per issuer):**

- `IsValid(Unit)`;
- `Unit->HasAuthority()`.

**Do not re-run** in dispatch: TeamId auth, whitelist, target/capability/location validation — already done by validator immediately before synchronous dispatch.

Invalid between validate and dispatch → skip, increment Skipped, continue others. **No** second full validation pass.

---

## 5. Per-unit payload (locked)

```cpp
// GP/Source/GPRuntime/Public/Command/GPUnitCommand.h
struct FGP_UnitCommand
{
	FGameplayTag CommandTag;
	FVector TargetLocation = FVector::ZeroVector;
	AActor* TargetActor = nullptr;
	bool bQueue = false;
};
```

| Requirement | Value |
| --- | --- |
| Kind | Plain C++ struct |
| USTRUCT / BlueprintType / replicated / NetSerialize | **No** |
| IssuingUnits / PC / PS / TeamId / owner / timestamp | **Absent** |
| Includes / forwards | `AActor`, `FGameplayTag`, `FVector` as needed |
| `FGP_CommandRequest` changes | **NO** |

### TargetActor representation

- Type: `AActor*`
- Valid only for synchronous receiver invocation
- Phase E base receiver **must not** store the pointer
- Future async command storage needs a separate lifetime-safe representation

---

## 6. Unit receiver (locked)

```cpp
// AGP_UnitBase — public virtual C++
virtual void ReceiveCommand(const FGP_UnitCommand& Command);
```

| Policy | Value |
| --- | --- |
| UFUNCTION / BlueprintNativeEvent / Blueprint | **No** |
| Override by subclasses | Allowed later |
| Base implementation | Authority guard + structured **Received** log |
| State mutation / execution | **None** |
| All `AGP_UnitBase` are receivers | **Yes** |
| Non-UnitBase receivers | Not needed (issuing list is `AGP_UnitBase`) |

### Return policy

`void` — Phase E proves **delivery**, not execution. No receive/execution result enum; no acknowledgement object.

Delivery counts as success when defensive checks pass and `ReceiveCommand` is called.

### Rejected alternatives

| Alternative | Why not now |
| --- | --- |
| `UINTERFACE` | Issuers already UnitBase; extra abstraction |
| ActorComponent | No need for replicated/stateful component for one method |
| GAS event | Pulls in execution framework too early |
| Message bus / delegate | Obscures deterministic direct server delivery |
| Full `FGP_CommandRequest` | Exposes multi-unit issuing list to every unit |

---

## 7. Dispatch iteration order (locked)

1. Verify CommandComponent owner authority (else return 0).
2. Read `ValidatedRequest.IssuingUnits`.
3. Set `RequestedUnits = Num`, `DeliveredUnits = 0`, `SkippedUnits = 0`.
4. Build **one** common `FGP_UnitCommand` from normalized request (tag / location / actor / `bQueue`) — immutable, shared for all units.
5. Iterate issuing units in normalized order:
   - fail `IsValid` or `HasAuthority` → Skipped++;
   - else `Unit->ReceiveCommand(UnitCommand)` exactly once; Delivered++.
6. Log one aggregate Dispatch summary.
7. Return Delivered count.

**Do not** call receiver on: `TargetActor`, all world units, client proxies, units outside normalized issuing list.

---

## 8. Validator purity (locked)

`ValidateAndNormalizeCommand` must **not**:

- call `ReceiveCommand`;
- log delivery;
- mutate actors;
- fire events/delegates;
- store requests;
- run gameplay.

Validator and dispatcher remain separate methods.

---

## 9. Queue / unit storage (locked)

| Item | Policy |
| --- | --- |
| `bQueue` | Delivered + logged only |
| Queue list / replace current / behavior branch | **No** |
| LastCommand / CurrentCommand / CommandQueue / CurrentTarget / PendingTargetLocation / replicated command state / diagnostic UPROPERTY | **No** |
| Base receiver | **Stateless** |

---

## 10. GAS / module / Build.cs (locked)

Phase E stays entirely in **GPRuntime**.

Do **not** add dependency pressure for: AbilitySystemComponent, Gameplay Events, ability tags, or new Build.cs edges for Phase E.

**Build.cs impact: NO**

(`GPRuntime` already depends on `GPGASRuntime` for tags elsewhere; Phase E must not require new ASC/event usage.)

Future execution may convert `FGP_UnitCommand` → GAS/AI intent separately.

---

## 11. Logging (locked)

### Dispatch summary — existing `LogGPCommandServer`

```text
GP CommandDispatch: PC=... Team=... Tag=... RequestedUnits=... DeliveredUnits=... SkippedUnits=... TargetActor=... Queue=... NetMode=...
```

PC/Team from authoritative server context (not payload).

### Unit receiver — `LogGPUnitCommand` in `GPUnitBase.cpp`

```text
GP UnitCommand Received: Unit=... Team=... Tag=... TargetActor=... Loc=... Queue=... Role=... NetMode=...
```

| Rule | Policy |
| --- | --- |
| One Received per unit per click | Yes |
| Server world only | Yes |
| On-screen / tick | No |
| Development Log verbosity | OK for validation |

---

## 12. Dispatch failure semantics (locked)

Validation result is **not** rewritten by dispatch.

| Case | Behavior |
| --- | --- |
| Delivered > 0 | Partial OK; no retry |
| Delivered == 0 | Log zero-delivery; no Client RPC |
| Rollback / undo | None |
| RPC storage after return | None |

---

## 13. Network expectations (locked)

Remote client chain:

```text
Client: GP CommandInput
Server: GP CommandServer Accepted
Server: GP CommandDispatch
Server: GP UnitCommand Received
```

No `GP UnitCommand Received` on autonomous/simulated client worlds.
Host and remote deliver only to authoritative units of their own normalized issuing list.

---

## 14. Operator validation matrix (future)

### Standalone
| Case | Expected |
| --- | --- |
| Single Move | Accepted; Requested=1 Delivered=1; one Received; no movement |
| Multi Move | Requested=N Delivered=N; exactly N unique Received |
| Queue true | Dispatch+Received Queue=true; no queue behavior |
| Attack | Received enemy TargetActor + authoritative loc; no attack |
| Mine | Received Resource target when available; no mining |

### 2P Listen Server
| Case | Expected |
| --- | --- |
| Host | Dispatch to host Team 1 unit only |
| Remote client | Server Accepted Team 2; dispatch Team 2 authority unit; Received server-side only |
| Isolation | No cross-team delivery; TargetActor not invoked as receiver unless also in issuing list |

### Regressions
No movement / AI / ability / state mutation; no duplicate Received; no RPC/UHT runtime warnings.

---

## 15. GP-S17 completion state (locked)

After Phase E implementation + Standalone/2P validation:

**GP-S17 Status: `DONE_WITH_EXECUTION_DEFERRED`**

Means present and validated:

- CommandComponent; canonical request; smart builder; RMB caller
- Server RPC; authoritative validation
- Dispatch; unit receiver boundary
- Network delivery proof

Does **not** mean Move/Attack/Mine gameplay works.

### Actual next roadmap stages (from docs)

| Stage | Source | Scope |
| --- | --- | --- |
| GP-S18 | TDD/13 | `AGP_UnitBase` abstract completion (highlight / fuller unit layer) — request already pulled into GP-S17 |
| GP-S19 | TDD/13 | Historically `FGP_CommandRequest` + tags — largely absorbed by GP-S17 prerequisite |
| GP-S20 | TDD/13 | `UGP_MovementComponent` (NavMesh, MoveTo) |
| GP-S21 | TDD/13 | `AGP_MobileUnit` base |
| GP-S22 | TDD/13 | Route `ReceiveCommand` → MovementComponent (execution) |

Attack/Mine execution remains later combat/mining component work (TDD/04) — not Phase E.

---

## 16. Exact next implementation checkpoint

**GP-S17 Phase E — dispatch + receiver (log only)**

### Expected new file
- `GP/Source/GPRuntime/Public/Command/GPUnitCommand.h`

### Expected modified C++
- `GP/Source/GPRuntime/Public/Command/GPCommandComponent.h`
- `GP/Source/GPRuntime/Private/Command/GPCommandComponent.cpp`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`

### Prefer unchanged
- `GPPlayerController.h`
- `GPCommandRequest.h`
- Build.cs / gameplay tags / assets / maps / config

### Request struct changes required
**NO**

---

## Stop condition
**ANALYSIS_READY_IMPLEMENTATION_PENDING.**
Await Phase E implementation assignment.
Do **not** implement dispatch/receiver/execution from this analysis.
Do **not** merge to main from this docs-only pass alone.
