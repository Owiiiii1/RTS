# GP-S17 Phase E — Command Dispatch & Unit Receiver
(Implementation — delivery boundary only)

## Status
**Status: CODE_DONE_NETWORK_VALIDATED**

Phase E dispatch + unit receiver **complete** and network-validated in 2P Listen Server.
Remote client Team 2 Move, host Team 1 multi Move, and Attack payload delivery **PASS**.
One receiver call per accepted unique issuing unit; TargetActor not auto-dispatched; Delivered/Skipped aggregation confirmed.
Authoritative server-only receiver confirmed. **No** gameplay execution (Move/Attack/Mine/queue by design).

---

## 1. Implemented contracts

### Payload — `GP/Source/GPRuntime/Public/Command/GPUnitCommand.h`

```cpp
struct GPRUNTIME_API FGP_UnitCommand
{
	FGameplayTag CommandTag;
	FVector TargetLocation = FVector::ZeroVector;
	AActor* TargetActor = nullptr;
	bool bQueue = false;
};
```

| Item | Value |
| --- | --- |
| Kind | Plain C++ (not USTRUCT) |
| Export | `GPRUNTIME_API` — matches `FGP_CommandRequest` / `FGP_LobbyPlayer` |
| TargetActor | Raw pointer; sync-only; receiver must not store |

### Dispatch

```cpp
int32 UGP_CommandComponent::DispatchValidatedCommand(
	const FGP_CommandRequest& ValidatedRequest) const;
```

- Owner valid + `HasAuthority()` else Warning + return 0
- One shared immutable `FGP_UnitCommand` per request
- Iterate `IssuingUnits` order; `IsValid` + `HasAuthority` only (no TeamId/target re-validate)
- One `ReceiveCommand` per valid authority unit
- Returns delivered receiver-call count

### Receiver

```cpp
virtual void AGP_UnitBase::ReceiveCommand(const FGP_UnitCommand& Command);
```

- Authority guard → one `LogGPUnitCommand` Received line → return
- No state / execution / TargetActor method calls

### RPC integration

```text
ValidateAndNormalizeCommand
→ GP CommandServer Accepted
→ DispatchValidatedCommand  (per-unit Received logs)
→ GP CommandDispatch summary
→ return
```

`SkippedUnits = RequestedUnits - DeliveredUnits`. DeliveredUnits==0 → Warning (no retry).

---

## 2. Logging

| Category | Location | Format |
| --- | --- | --- |
| `LogGPCommandServer` | existing | `GP CommandDispatch: PC=… Team=… Tag=… RequestedUnits=… DeliveredUnits=… SkippedUnits=… TargetActor=… Queue=… NetMode=…` |
| `LogGPUnitCommand` | `GPUnitBase.cpp` STATIC | `GP UnitCommand Received: Unit=… Team=… Tag=… TargetActor=… Loc=… Queue=… Role=… NetMode=…` |

Actual order: CommandInput → Accepted → Received×N → CommandDispatch

---

## 3. Builds

| Target | Result |
| --- | --- |
| GPEditor Win64 Development | **PASSED** (UHT via compile path) |
| GP Win64 Development | **PASSED** |
| GP Win64 Shipping | **PASSED** |

---

## 4. Operator validation

| Case | Result |
| --- | --- |
| Standalone delivery | **NOT CAPTURED IN PROVIDED EXCERPT** |
| Remote client Team 2 Move | **PASS** — Client input LocalTeam=2 → server PC=`GP_PlayerController_1` Accepted → one Authority Received (Team=2) → Dispatch 1/1/0 |
| Host Team 1 multi Move | **PASS** — Units=2 → Accepted 2 → exactly two Received (Team=1, Authority) → Dispatch 2/2/0 |
| Host Team 1 Attack (2 issuers) | **PASS** — Attack + enemy TargetActor + authoritative Loc; two issuer Received; **target not** Received; Dispatch 2/2/0 |
| Team isolation | **PASS** — no cross-team delivery |
| Receiver server-only authority | **PASS** — Received Role=Authority / NetMode=ListenServer |
| Invocation count (one per unique issuer) | **PASS** |
| Delivered/Skipped aggregation | **PASS** |
| Duplicate receiver calls | **NONE** |
| Unexpected movement | **NONE** |
| AI / NavMesh / GAS execution | **NONE** |
| Unit state mutation | **NONE** |
| Runtime RPC/UHT warnings (supplied excerpt) | **NONE** |
| Queue=true receiver delivery | **NOT CAPTURED** |
| Mine delivery | **DEFERRED** |

---

## Stop condition
**CODE_DONE_NETWORK_VALIDATED.**
Phase E complete. GP-S17 closes as `DONE_WITH_EXECUTION_DEFERRED` (delivery boundary complete; no Move/Attack/Mine execution).
Next: GP-S18 unit layer; GP-S20–S22 Move via ReceiveCommand → MovementComponent (TDD/13).
