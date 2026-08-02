# GP-S17 — Command Request Prerequisite
(Canonical `FGP_CommandRequest` contract — analysis only)

## Status
**Status: IMPLEMENTATION_DONE**

Canonical header **implemented and operator-validated**:
`GP/Source/GPRuntime/Public/Command/GPCommandRequest.h`

- reflected `USTRUCT(BlueprintType)` `FGP_CommandRequest`
- exact five `UPROPERTY` fields (`CommandTag`, `IssuingUnits`, `TargetLocation`, `TargetActor`, `bQueue`)
- **no** NetSerialize / helpers / validation methods / `.cpp`
- GPEditor / GP Development / GP Shipping — **PASSED**
- UHT — **PASSED**
- Blueprint reflection validation — **PASSED** (`GP Command Request` variable type; all five fields visible; BP compiles; temp BP variable discarded; no assets saved)
- CommandComponent / PlayerController / Selection / input / RPC **not** integrated
- Invariants remain **documented** for future server validation (not coded here)
- Ready to be consumed by GP-S17 Phase B **after merge**

GP-S19 **full implementation not started**.
GP-S17 Phase A remains **PHASE_A_DONE**.
Phase B **not started**.

---

## 1. Canonical identity

| Item | Locked value |
| --- | --- |
| Header | `GP/Source/GPRuntime/Public/Command/GPCommandRequest.h` |
| Module | `GPRuntime` |
| Type | `USTRUCT(BlueprintType)` `struct GPRUNTIME_API FGP_CommandRequest` |
| NetSerialize | **None** on MVP checkpoint |
| `.cpp` | **None** unless later helpers require it |
| Role | Pass-by-value intent payload — **not** authoritative gameplay state |
| Server handling | Normalize + validate a **copy**; never trust the raw client payload as validated |

STYLE documents plural `Commands/`; Phase A already uses singular `Command/`. First implementation **colocates** here — do not create a parallel request home.

---

## 2. Exact MVP fields (five only)

All fields **must** be `UPROPERTY`.

| Field | Type | Default |
| --- | --- | --- |
| `CommandTag` | `FGameplayTag` | empty |
| `IssuingUnits` | `TArray<TObjectPtr<AGP_UnitBase>>` | empty |
| `TargetLocation` | `FVector` | `FVector::ZeroVector` |
| `TargetActor` | `TObjectPtr<AActor>` | `nullptr` |
| `bQueue` | `bool` | `false` |

Hard cap: `IssuingUnits` **≤ 24** (`static constexpr int32 MaxIssuingUnits = 24`; match selection cap).

Actor references are for **reflected RPC serialization** (object refs / NetGUID). Struct is not authoritative selection or match state.

**No additional fields.** TDD’s array name `Targets` is replaced by **`IssuingUnits`** (same meaning: units issuing the command). Do not also ship a `Targets` field.

### Documentation-level definition (not implemented here)

```cpp
// GP/Source/GPRuntime/Public/Command/GPCommandRequest.h

USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_CommandRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag CommandTag;

	UPROPERTY()
	TArray<TObjectPtr<AGP_UnitBase>> IssuingUnits;

	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	bool bQueue = false;

	static constexpr int32 MaxIssuingUnits = 24;
};
```

---

## 3. Invariants

Normative for Phase B+ / server validation (client may violate):

1. `CommandTag` must be **valid** and under **`GP.Command`** (reject empty / unknown / wrong hierarchy).
2. After server normalization, `IssuingUnits` contains **1–24** elements for executable unit commands.
3. Null / stale refs are **dropped**.
4. Duplicates are **removed**.
5. **First-occurrence order** is preserved after dedupe.
6. Client ordering is **not** a security boundary (server re-checks ownership/eligibility).
7. **TeamId / owner / capabilities** are **never** accepted from the request.
8. Target **shape** is determined by `CommandTag` — **no** target-kind enum.
9. Simultaneous `TargetActor` + `TargetLocation` is allowed only as **actor + fallback/impact location** for command types that need both; otherwise server rejects or canonicalizes per tag rules.
10. Location-only commands: `TargetActor == nullptr`.
11. No-target commands (e.g. Stop): target fields **ignored** by server.
12. Invalid mixed state is **rejected** or **canonicalized** by server rules for that command tag.

---

## 4. Target representation

No `EGP_CommandTargetKind`.

| Shape | Rule |
| --- | --- |
| Location-only | Meaningful `TargetLocation`; `TargetActor == nullptr` |
| Actor-primary | Required `TargetActor`; location optional fallback/impact |
| No-target | Target fields ignored |
| Actor + location | Only when the command type needs both; else reject/canonicalize |

---

## 5. Queue semantics

| Value | Meaning |
| --- | --- |
| `bQueue == false` | Replace / current-order policy |
| `bQueue == true` | Reserved **queued intent** |

MVP executor may temporarily support **replace only**.

If queue execution is not implemented yet, the server **must not** silently treat `true` as a fully supported queue. Future documented policy must either:

- **reject** queued requests, or
- **explicitly downgrade** to replace with a documented rule.

No execution-policy enum in this checkpoint.

---

## 6. Unit reference representation

| Choice | Verdict |
| --- | --- |
| `TObjectPtr<AGP_UnitBase>` in `IssuingUnits` | **Canonical** |
| `TWeakObjectPtr` inside network payload | **Forbidden** |
| Raw `AActor*` | Reflectable, but contract uses **`TObjectPtr`** |
| Selection local storage | May remain `TWeakObjectPtr`; convert to hard refs at build time |

---

## 7. RPC compatibility

Future signature (not implemented now):

```cpp
UFUNCTION(Server, Reliable, WithValidation)
void Server_RequestCommand(FGP_CommandRequest Request);
```

| Topic | Contract |
| --- | --- |
| Pass form | By value |
| Actor serialization | Unreal reflected object references / NetGUID |
| Custom NetSerialize | **Not** required until profiling/compatibility proves otherwise |
| Unresolved / destroyed / stale | Server must handle (prune / reject) |
| Ownership | Remote client is **not** unit owner merely because it sent a reference |

---

## 8. Trust boundary

### Client supplies (intent only)

- command intent tag
- candidate unit references
- candidate target actor / location
- queue intent (`bQueue`)

### Server derives or validates

- issuing PlayerController identity
- ownership / team
- unit alive / valid state
- command capability
- target legality
- friendly / enemy / neutral relation
- FoW visibility when FoW exists
- distance / range
- navigation reachability
- duplicate / null / cap normalization
- final dispatch eligibility

**Client-provided request is never a validated command.**

---

## 9. Relationship to GP-S19

| Item | Decision |
| --- | --- |
| This work | Pull-forward of **canonical request type contract** only |
| Full GP-S19 | **Not started** |
| Future S19 | **Must reuse** this type — no second request struct |
| Remains future | Tag-to-command mapping helpers, richer validation, other S19 logic |
| After struct lands | GP-S17 Phase B may start `BuildSmartCommand` |

---

## 10. Exact next implementation checkpoint

### `FGP_CommandRequest` struct only

| In scope | Out of scope |
| --- | --- |
| One public header | `.cpp` (unless trivial need — prefer none) |
| Reflected `USTRUCT` + five fields + defaults | Helper / validation methods |
| `MaxIssuingUnits` constexpr | NetSerialize |
| Compile + **UHT** validation (Editor/Dev/Shipping) | PlayerController / CommandComponent changes |
| | RPC / input / execution |

---

## 11. Strict exclusions

- no `FGP_CommandRequest` code in this docs pass
- no NetSerialize / BuildSmartCommand / RMB / RPC
- no CommandComponent behavior changes
- no full GP-S19 / Move / Attack / Ability

---

## Operator validation (passed)

| Check | Result |
| --- | --- |
| Blueprint type `GP Command Request` | **PASS** |
| Blueprint compiles with struct variable | **PASS** |
| Fields visible (Command Tag, Issuing Units, Target Location, Target Actor, Queue) | **PASS** |
| Reflection / UHT warnings | **NONE** |
| PIE / camera / selection regression | **NONE** |
| Temp Blueprint variable / assets saved | **NO** |

## Stop condition
**IMPLEMENTATION_DONE.**
Canonical request type ready for merge. Next: GP-S17 Phase B analysis/implementation planning after merge.
Do **not** start Phase B / RPC / input / full GP-S19 / command execution from this finalize.
