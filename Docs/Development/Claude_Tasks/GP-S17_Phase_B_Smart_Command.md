# GP-S17 Phase B — BuildSmartCommand
(Local request construction only — analysis finalize)

## Status
**Status: ANALYSIS_READY_IMPLEMENTATION_PENDING**

Docs-only. **No** C++ in this pass.
Depends on: Phase A CommandComponent shell (**DONE**), canonical `FGP_CommandRequest` (**IMPLEMENTATION_DONE**).
Does **not** start Phase C (RMB), Phase D (RPC), execution, full GP-S18 / GP-S19.

---

## 1. Exact API (locked)

```cpp
bool BuildSmartCommand(
	AActor* TargetActor,
	const FVector& TargetLocation,
	bool bQueue,
	FGP_CommandRequest& OutRequest) const;
```

| Decision | Value |
| --- | --- |
| Visibility | **Public** C++ API |
| `const` | **YES** |
| `UFUNCTION` | **NO** |
| Blueprint exposure | **NO** |
| Target-context USTRUCT | **NO** |
| Overloads | **NO** |
| Optional / variant types | **NO** |

### OutRequest / failure
1. Always start with `OutRequest = FGP_CommandRequest{}`.
2. Any failure → `return false` with OutRequest left **default-constructed**.
3. **No** partial request returned on failure.
4. Success → `return true` only with a fully populated request.

---

## 2. Selection read boundary

```text
UGP_CommandComponent::GetOwner()
  → Cast<AGP_PlayerController>
  → GetSelectionComponent()
  → GetSelectedUnits()   // const TArray<TWeakObjectPtr<AGP_UnitBase>>&
```

| Rule | Value |
| --- | --- |
| Persistent selection cache on CommandComponent | **NO** |
| Delegate subscription | **NO** |
| Cached unit array member | **NO** |
| Temporary local normalized array **inside one call** | **Allowed** |
| SelectionComponent API changes | **None** |
| InspectedTarget | **Not** read by BuildSmartCommand |

---

## 3. Exact smart mapping

Before implementation: re-confirm native tag names in `FGPGameplayTags` / `.cpp`. **Do not create new tags.**

| Target context | Local speculative command |
| --- | --- |
| `TargetActor == nullptr` | `GP.Command.Move` |
| Enemy `AGP_UnitBase` (assigned TeamId ≠ issuing) | `GP.Command.Attack` |
| Neutral `AGP_UnitBase` (TeamId == 0 / `IsNeutral`) | `GP.Command.Attack` **speculative only** |
| Unassigned `AGP_UnitBase` (TeamId &lt; 0) | Treat as **neutral** → speculative `GP.Command.Attack` |
| Friendly `AGP_UnitBase` (same assigned TeamId ≥ 1) | `GP.Command.Move`, `TargetActor = nullptr`, use `TargetLocation` |
| Actor with canonical `GP.Resource.Node` identity | `GP.Command.Mine` (see §5) |
| Unknown non-unit actor | `GP.Command.Move`, `TargetActor = nullptr` |
| No valid selected units after normalize | **failure** |
| Missing / unassigned local team (TeamId &lt; 1) | **failure** |

| Deferred / forbidden | Rule |
| --- | --- |
| Neutral Attack | Speculative intent only — **not** authoritative attack permission |
| `GP.Command.Interact` | **Do not use** — no canonical native tag / consumer |
| Repair | Deferred |
| Follow | Deferred |
| Ability | Deferred |

Expected native command tags (confirm at impl):  
`Command_Move`, `Command_Attack`, `Command_Mine` → `GP.Command.Move` / `Attack` / `Mine`.

---

## 4. Team relation policy

| Topic | Rule |
| --- | --- |
| Issuing team | Local PlayerState / controller contract (`GetTeamId()`) |
| Target team | Only from `AGP_UnitBase` |
| Friendly | Same **assigned** TeamId (≥ 1) |
| Enemy | Different **assigned** TeamIds (≥ 1) |
| Neutral | Target TeamId == 0 / `IsNeutral()` |
| Unassigned target | Treated as **neutral** (speculative Attack) |
| Unknown non-unit | **No** Attack — Move fallback |
| Local unassigned / missing team | **Failure** |
| TeamId in request | **Never** |
| Security | Local relation is **not** a security boundary |

---

## 5. Resource / Mine policy

`GP.Command.Mine` only if **both**:

1. Native `GP.Command.Mine` exists (`FGPGameplayTags::Command_Mine`), and  
2. Target has canonical `GP.Resource.Node` identity via an **existing** project accessor.

**Known candidate path (must re-confirm at implementation):**  
`AGP_UnitBase::HasCapabilityTag(FGPGameplayTags::Get().Resource_Node)` — `GP.Resource.Node` is registered; `HasCapabilityTag` uses exact match on interim `CapabilityTags`.

| If… | Then… |
| --- | --- |
| Path confirmed for `AGP_UnitBase` | Mine when cast + `HasCapabilityTag(Resource_Node)` |
| Non-UnitBase resource actor with **no** clear tag accessor | Implementation **BLOCKED** for that case — do **not** invent a new interface |
| Tag path ambiguous | **BLOCKED**, not speculative invention |

Mine branch does not invent Interact or new tags.

---

## 6. Capability policy

- BuildSmartCommand **does not** filter selected units by CapabilityTags / AllowedCommands.
- All normalized selected candidates go into `IssuingUnits`.
- Missing capability is **not** a local failure.
- Server checks each unit later.
- CommandComponent **does not** mutate selection.

---

## 7. Request construction rules

1. `OutRequest = FGP_CommandRequest{}`
2. Resolve owner PC + SelectionComponent; fail if missing / local TeamId &lt; 1
3. Walk `GetSelectedUnits()` in selection order
4. Drop invalid / null weaks
5. Dedupe by identity; keep **first** occurrence
6. Clamp to first **24** valid unique units
7. Fail if normalized list empty
8. Deterministic tag mapping (§3)
9. Fill: `CommandTag`, `IssuingUnits`, `TargetLocation`, `TargetActor`, `bQueue`
10. `return true` only when fully assembled

| Shape | TargetActor | TargetLocation |
| --- | --- | --- |
| Move / Move fallback | `nullptr` | input `TargetLocation` |
| Attack / Mine | preserved | input location as impact/fallback candidate |

### 24 policy
Use first 24 valid unique refs; preserve order. Local clamp is **not** server protection — server re-applies cap.

---

## 8. Server-only responsibilities

Ownership · team authority · unit alive/state · capability · target legality · attackability · resource mineability · FoW · range · nav · queue support · final dispatch · malicious/stale actors.

**BuildSmartCommand produces an intent candidate, not a validated command.**

---

## 9. Validation strategy (implementation)

Preferred: debugger breakpoint on public C++ method.  
Temporary development-only invocation only if otherwise unreachable — **must be removed before commit**.  
No permanent hooks. No asset/map changes.

| Case | Expected |
| --- | --- |
| No selection | `false` / default request |
| One unit + ground | Move |
| Multiple + ground | Move, order preserved |
| Enemy UnitBase | Attack |
| Friendly UnitBase | Move, TargetActor null |
| Neutral UnitBase | Speculative Attack |
| Resource node | Mine **only if** tag path confirmed |
| Unknown actor | Move fallback |
| Invalid/duplicate refs | Normalized |
| &gt;24 | First 24 |
| 2P | Each CommandComponent reads **own** PC selection only |

---

## 10. Exact next implementation checkpoint

**GP-S17 Phase B — BuildSmartCommand local construction only**

| Allowed | Forbidden |
| --- | --- |
| `GPCommandComponent.h/.cpp` | PC input / Enhanced Input / RMB |
| Include/use `GPCommandRequest.h` | RPC / server validation / dispatch |
| One public C++ method | Movement / AI / Nav / Unit receiver |
| Selection read + normalize + map + fill | Permanent test hook |
| Docs + builds/UHT | Assets / maps / config / new tags |

---

## Stop condition
**ANALYSIS_READY_IMPLEMENTATION_PENDING.**
Contract locked. Await Phase B implementation assignment.
Do **not** write BuildSmartCommand / input / RPC from this finalize.
