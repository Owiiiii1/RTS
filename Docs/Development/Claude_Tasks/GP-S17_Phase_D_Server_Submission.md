# GP-S17 Phase D — Server Submission & Validation
(Implementation — RPC + authoritative normalize)

## Status
**Status: CODE_DONE_NETWORK_VALIDATED**

Phase D server submission + authoritative validate/normalize **complete** and network-validated in 2P Listen Server.
Local request crosses `Server_RequestCommand` boundary; host and remote client submissions accepted with correct PC/Team.
**No** dispatch / execution / movement / client ack / rate limiter.
Does **not** start full GP-S18 / GP-S19.

---

## 1. Compiled RPC

```cpp
UFUNCTION(Server, Reliable)
void Server_RequestCommand(const FGP_CommandRequest& Request);
```

| Item | Result |
| --- | --- |
| Owner | `AGP_PlayerController` |
| Reliable | Yes |
| WithValidation | No |
| Parameter | `const FGP_CommandRequest&` — **UHT accepted** (no value-param fallback) |
| Custom NetSerialize | No |
| Request struct changes | **NO** |
| Header include | `#include "Command/GPCommandRequest.h"` in `GPPlayerController.h` |

UHT/build serialization of current USTRUCT (`FGameplayTag`, `TArray<TObjectPtr<AGP_UnitBase>>`, `FVector`, `TObjectPtr<AActor>`, `bool`) — **compile PASSED**.
Runtime 2P proof: remote client → server Accepted log — **PASSED** (operator Output Log).

---

## 2. Client submission flow

```text
OnCommandInputStarted
→ BuildSmartCommand success
→ local LogGPCommandInput
→ Server_RequestCommand(Request)
→ no local dispatch / movement
```

Trace miss / build failure → no RPC.

---

## 3. Validator

```cpp
bool UGP_CommandComponent::ValidateAndNormalizeCommand(
	const FGP_CommandRequest& ClientRequest,
	FGP_CommandRequest& OutValidatedRequest,
	EGP_CommandRejectReason& OutRejectReason) const;
```

| Item | Value |
| --- | --- |
| Enum | `EGP_CommandRejectReason` in `GPCommandComponent.h` (plain C++, not UENUM) |
| Owner derivation | `Cast<AGP_PlayerController>(GetOwner())` → `GetPlayerState<AGP_PlayerState>()` → `GetTeamId()` |
| Component replication | Still non-replicated |
| Persistent state | None |

### Reject reasons
None, InvalidController, InvalidPlayerState, InvalidRequestingTeam, InvalidCommandTag, UnsupportedCommandTag, NoCommandableUnits, InvalidTarget, FriendlyAttackTarget, InvalidResourceTarget, InvalidTargetLocation

### Whitelist
Exact equality: `Command_Move`, `Command_Attack`, `Command_Mine` via `FGPGameplayTags`.

### Issuing units
Client order → invalid prune → dedupe first → TeamId match → cap 24.
Foreign TeamId → prune; **one** aggregate Warning (`UnauthorizedUnits`) if count > 0.
Empty accepted → `NoCommandableUnits`.
**Team-commandability** limitation: same-team shared control; no per-player ownership.

### Move
`TargetActor=nullptr`; client location + sanity.

### Attack
UnitBase required; friendly TeamId reject; 0/−1 + other playable allowed; location = actor location + sanity.

### Mine
UnitBase + `HasCapabilityTag(Resource_Node)`; location = actor location + sanity.
No issuer mining capability filter.

### Location sanity
`ContainsNaN` false; `FMath::IsFinite` XYZ; abs each ≤ `10000000.0`.

### Queue
`bQueue` preserved as intent only — **not** executed.

---

## 4. Logging

Category: `LogGPCommandServer` — `DECLARE` in `GPCommandComponent.h`, `DEFINE` in `GPCommandComponent.cpp`.

| Event | Format |
| --- | --- |
| Accepted | `GP CommandServer Accepted: PC=… Team=… Tag=… ReceivedUnits=… AcceptedUnits=… TargetActor=… Loc=… Queue=… NetMode=…` |
| Rejected | `GP CommandServer Rejected: PC=… Team=… Reason=… Tag=… ReceivedUnits=… NetMode=…` |
| Unauthorized | `GP CommandServer UnauthorizedUnits: PC=… ReceivedUnits=… UnauthorizedUnits=…` (**Warning**, aggregate) |

---

## 5. Builds

| Target | Result |
| --- | --- |
| GPEditor Win64 Development | **PASSED** (UHT via compile path) |
| GP Win64 Development | **PASSED** |
| GP Win64 Shipping | **PASSED** |

---

## 6. Operator network validation

| Case | Result |
| --- | --- |
| Listen host Move | **PASS** — local `CommandInput` + server `Accepted` (PC=`GP_PlayerController_0`, Team=1, TargetActor=None, Queue=false, NetMode=ListenServer) |
| Listen host Attack | **PASS** — server Accepted Attack; TargetActor preserved; TargetLocation normalized to authoritative actor location |
| Queue intent preserve | **PASS** — local Queue=true → server Queue=true; **no** queue execution |
| Remote client submission | **PASS** — client LocalTeam=2 / Client / AutonomousProxy → server PC=`GP_PlayerController_1` Team=2 Accepted Move on ListenServer |
| Host/client PC distinction | **PASS** — host `GP_PlayerController_0` vs client `GP_PlayerController_1` |
| Host/client Team distinction | **PASS** — Team=1 vs Team=2 |
| Request serialization (USTRUCT/refs/tags) | **PASS** |
| Unexpected movement / dispatch / AI / receiver / queue exec | **NONE** |
| Runtime RPC / validation errors-warnings in provided log | **NONE** |
| Resource Mine network | **DEFERRED** — local Phase C Mine mapping previously validated; Phase D Mine branch compiled; server Mine Accepted log pair **not** captured in supplied excerpt |
| Malicious-input matrix (foreign inject, invalid/unsupported tag, friendly Attack craft, Mine non-resource, NaN/extreme loc, duplicates, >24) | **DEFERRED** — normal RMB builder does not emit these; no permanent test hook added; branches build/UHT verified only |

---

## Stop condition
**CODE_DONE_NETWORK_VALIDATED.**
Phase D complete. Next stage = separate **command dispatch / receiver analysis** (not immediate movement).
Do **not** start dispatch / movement / AI / NavMesh / GP-S18 / GP-S19 from this close-out.
