# GP-S17 Phase D — Server Submission & Validation
(Final analysis contract — RPC + authoritative normalize)

## Status
**Status: ANALYSIS_READY_IMPLEMENTATION_PENDING**

Docs-only checkpoint. **No** C++ / assets / RPC / execution in this pass.
Depends on: Phase C `CODE_DONE_OPERATOR_VALIDATED`.
Phase D = client→server submission + authoritative validation/normalization + server logs.
**Does not** dispatch, move units, call receivers, execute queue, or start full GP-S18 / GP-S19.

Analysis treats `FGP_CommandRequest` as RPC-suitable by structure. Serialization fitness is **not** runtime-proven until UHT/build + 2P test in the implementation checkpoint.

---

## 1. RPC owner and exact signature (locked)

RPC lives on `AGP_PlayerController` (owns the client connection).

```cpp
UFUNCTION(Server, Reliable)
void Server_RequestCommand(const FGP_CommandRequest& Request);

// Generated
void AGP_PlayerController::Server_RequestCommand_Implementation(
	const FGP_CommandRequest& Request);
```

| Item | Decision |
| --- | --- |
| Reliability | **Reliable** |
| WithValidation | **No** |
| Custom NetSerialize | **No** |
| RPC on CommandComponent | **No** |
| CommandComponent replication | Remains **non-replicated** |
| Delegation | RPC entry → owner `UGP_CommandComponent` validator |

---

## 2. Client submission flow (locked)

```text
local BuildSmartCommand succeeds
→ existing local diagnostic log (Phase C)
→ Server_RequestCommand(Request)
→ no local dispatch
→ no movement
```

Standalone / listen-host also go through the Server RPC path (UE semantics).
Do **not** call the validator from a client-side path as an RPC substitute.

---

## 3. Client trust boundary (locked)

Request may contain only candidate intent:

- `CommandTag`, `IssuingUnits`, `TargetLocation`, `TargetActor`, `bQueue`

**Do not add:** TeamId, PlayerId, PlayerController, PlayerState, claimed owner, authority flag, timestamp, sequence id.

Server trusts **no** client field without validation/normalization.
Requesting identity comes from RPC PC context / CommandComponent owner.

---

## 4. Authoritative normalization order (locked)

1. Reset `OutValidatedRequest` to default; `OutRejectReason = None`.
2. Verify requesting controller via `Cast<AGP_PlayerController>(GetOwner())`.
3. Obtain authoritative `AGP_PlayerState` from owner PC.
4. Obtain authoritative requesting `TeamId`.
5. Reject if requesting `TeamId < 1` (`0` and `-1` are not playable requesting teams).
6. Validate command tag: valid + **exact whitelist only**.
7. Normalize issuing units:
   - record ReceivedUnits for diagnostics;
   - prune null/stale/invalid refs;
   - dedupe first occurrence; preserve order;
   - authoritative TeamId must equal requesting TeamId;
   - apply ownership-spoof policy (Warning on foreign);
   - cap accepted unique commandable units at **24**.
8. Reject if accepted issuing list empty → `NoCommandableUnits`.
9. Validate target shape by command tag.
10. Normalize authoritative target actor/location.
11. Sanitize resulting location.
12. Preserve `bQueue` as intent.
13. Fill normalized request.
14. Log Accepted or Rejected.
15. **Stop** — no dispatch / execution.

---

## 5. Issuing-unit commandability (locked)

MVP: unit is commandable iff its authoritative `TeamId ==` requesting `TeamId`.

| Note | Detail |
| --- | --- |
| Semantics | **Team-commandability**, not per-player ownership |
| Same-team multi-player | Shared control (limitation) |
| Future | Per-player / allied rights deferred |
| Client selection | **Not** trusted as proof of commandability |

Foreign `TeamId`:

- remove from candidate list;
- security-oriented **Warning**;
- do not add to normalized request;
- do **not** necessarily reject the whole command.

Result: own units remain → accept; none remain → reject `NoCommandableUnits`.

---

## 6. Cap and duplicate policy (locked)

Iterate client order → invalid prune → unauthorized prune → duplicate prune (keep first) → stop after 24 accepted unique commandable units.

Diagnostic counters (server log only; **not** returned to client):

- ReceivedUnits, AcceptedUnits, InvalidCount, UnauthorizedCount, DuplicateCount, CappedCount (as needed)

---

## 7. Exact whitelist (locked)

Allow only exact native tags via `FGPGameplayTags` direct comparisons:

- `Command_Move` → `GP.Command.Move`
- `Command_Attack` → `GP.Command.Attack`
- `Command_Mine` → `GP.Command.Mine`

Reject: invalid tag; bare parent `GP.Command`; any other `GP.Command.*` descendant; unrelated tags.

**No** DataAsset / DataTable / dynamic registry / new gameplay tags.

---

## 8. Move validation (locked)

- Authoritative `TargetActor = nullptr` (client actor ignored/cleared)
- `TargetLocation` from client request after sanity checks
- No NavMesh / reachability / path / world trace

Reject invalid / non-finite / extreme location.

---

## 9. Attack validation (locked)

- `TargetActor` required, valid, `AGP_UnitBase`
- Authoritative target TeamId:
  - `==` requesting TeamId → reject `FriendlyAttackTarget`
  - `0` or `-1` → neutral candidate **allowed**
  - other playable TeamId → enemy **allowed**
- Keep authoritative `TargetActor`
- `TargetLocation =` current authoritative actor location (ignore client loc)
- Alive/attackable: **deferred** (no canonical contract)

---

## 10. Mine validation (locked)

- `TargetActor` required, valid, `AGP_UnitBase`
- Authoritative `HasCapabilityTag(Resource_Node) == true`
- Target team must **not** ban neutral resources
- Keep `TargetActor`; `TargetLocation =` authoritative actor location
- Non-resource → reject `InvalidResourceTarget`
- Amount / depletion / mineability: **deferred**

---

## 11. Capability policy (locked)

Phase D does **not** filter issuing units by Move/Attack capability (tags absent).

- No invented `CanMove` / `CanAttack` / new tags
- Mine stage validates **target** identity: `HasCapabilityTag(Resource_Node)`
- Issuer mining capability deferred

---

## 12. Location sanitization (locked)

Canonical checks (do **not** rely on unconfirmed `FVector::IsFinite()`):

- `TargetLocation.ContainsNaN()` must be false
- `FMath::IsFinite` on X, Y, Z
- `FMath::Abs` of each component `<= 10000000.0`

| Command | When applied |
| --- | --- |
| Move | On client-provided location |
| Attack / Mine | After server replaces location with actor location, then same checks |

No nav projection / world-bounds subsystem in Phase D.

---

## 13. Queue policy (locked)

- Normalized request **preserves** `bQueue`
- Validated **intent only** — no queue container / execution
- `bQueue == true` is **not** a reject reason
- Accepted log shows Queue
- Docs must **not** claim queue already works

---

## 14. Validator location and API (locked)

Lives on `UGP_CommandComponent`. Requesting PC is **owner-derived** (no controller argument).

```cpp
bool ValidateAndNormalizeCommand(
	const FGP_CommandRequest& ClientRequest,
	FGP_CommandRequest& OutValidatedRequest,
	EGP_CommandRejectReason& OutRejectReason) const;
```

| Item | Decision |
| --- | --- |
| Owner | `Cast<AGP_PlayerController>(GetOwner())` → PS / TeamId |
| Exposure | Public C++; **no** UFUNCTION; **no** Blueprint |
| Qualifiers | `const`; no persistent validation state |

---

## 15. Reject reason enum (locked)

```cpp
enum class EGP_CommandRejectReason : uint8
{
	None = 0,
	InvalidController,
	InvalidPlayerState,
	InvalidRequestingTeam,
	InvalidCommandTag,
	UnsupportedCommandTag,
	NoCommandableUnits,
	InvalidTarget,
	FriendlyAttackTarget,
	InvalidResourceTarget,
	InvalidTargetLocation,
};
```

- Prefer plain C++ enum (not `UENUM`) unless reflection is required
- Placement: CommandComponent header or small optional validation header (include hygiene)
- No client-facing text strings

---

## 16. Output reset semantics (locked)

On entry:

- `OutValidatedRequest = FGP_CommandRequest{}`
- `OutRejectReason = EGP_CommandRejectReason::None`

On failure: `false`; Out stays default; reason set; **no** partial normalized leak.  
On success: `true`; reason `None`; Out fully normalized.

---

## 17. RPC implementation behavior (locked)

```text
Server_RequestCommand_Implementation
→ require CommandComponent
→ local OutValidated + Reason
→ ValidateAndNormalizeCommand(...)
→ reject: structured server log; return
→ accept: structured server log; return
```

Do **not** store request after the function.
**No** `LastValidatedRequest` / delegate / replicated state / dispatch / unit receiver.

---

## 18. Logging policy (locked)

Category: `LogGPCommandServer` — define once (canonical: `GPCommandComponent.cpp` next to validator, or PC `.cpp` if RPC-only; prefer **CommandComponent.cpp**).

**Accepted (Log):**
```text
GP CommandServer Accepted: PC=... Team=... Tag=... ReceivedUnits=... AcceptedUnits=... TargetActor=... Loc=... Queue=... NetMode=...
```

**Rejected:**
```text
GP CommandServer Rejected: PC=... Team=... Reason=... Tag=... ReceivedUnits=... NetMode=...
```

| Case | Verbosity |
| --- | --- |
| Normal validation reject | Log / Verbose |
| Unauthorized foreign-unit injection | **Warning** |
| On-screen / Client RPC / client strings | **No** |

---

## 19. Rate-limit policy (locked)

Phase D: **no** timer / token bucket / persistent counters / cooldown.

Document: Reliable RPC flood protection required before production; separate future checkpoint. One-shot RMB is **not** security protection.

---

## 20. Network validation matrix (future operator)

| Case | Expected |
| --- | --- |
| Standalone Move | Local command log + server Accepted; no movement |
| Listen host | Server Accepted; correct PC/team |
| Remote client | RPC on server; Accepted; correct client PC/team |
| Host/client isolation | Each submits only its team units |
| Foreign unit injection | Pruned + Warning; remaining own units accepted if any |
| Foreign-only request | Reject `NoCommandableUnits` |
| Duplicates | Deduped |
| >24 units | Cap 24 |
| Invalid tag | Reject |
| Unsupported `GP.Command.*` | Reject |
| Move with actor | Actor cleared |
| Attack enemy | Accepted |
| Attack friendly | Rejected |
| Attack neutral (0 / −1) | Accepted candidate |
| Mine Resource.Node | Accepted |
| Mine non-resource | Rejected |
| NaN/Inf/extreme Move loc | Rejected |
| Queue true | Accepted/preserved in log; no execution |
| Every accepted case | No movement / AI / receiver / dispatch |

---

## 21. Exact next implementation checkpoint

**GP-S17 Phase D — server submission and normalization only**

| Expected C++ | Path |
| --- | --- |
| PC | `GPPlayerController.h` / `.cpp` |
| Command | `GPCommandComponent.h` / `.cpp` |
| Optional | One small validation header if enum hygiene needs it |

| Item | Decision |
| --- | --- |
| Request struct changes | **NO** |
| Assets / maps / config / Build.cs | **NO** |
| Dispatch / movement / AI / Nav / ack / rate limiter | **NO** |

---

## Stop condition
**ANALYSIS_READY_IMPLEMENTATION_PENDING.**
Await Phase D implementation assignment.
Do **not** implement RPC / validator / execution from this analysis.
Do **not** merge to main from this docs-only pass alone.
