# GP-S17 Phase B — BuildSmartCommand
(Local request construction only)

## Status
**Status: CODE_DONE_FUNCTIONAL_VALIDATION_DEFERRED**

Phase B **implementation complete**. Regression validation **passed**.
Functional request-content validation (Move / Attack / Mine / dedupe / cap 24) **deferred** until a real caller exists.

| Item | Value |
| --- | --- |
| API | `bool BuildSmartCommand(AActor*, const FVector&, bool, FGP_CommandRequest&) const` |
| Exposure | Public C++; **no** UFUNCTION / Blueprint |
| Selection | `GetSelectedUnits()` → `const TArray<TWeakObjectPtr<AGP_UnitBase>>&` |
| TeamId | `int32`; unassigned **-1**; neutral **0**; playable **1+** |
| Local TeamId | `AGP_PlayerState::GetTeamId()` |
| Target TeamId | `AGP_UnitBase::GetTeamId()` |
| Move | `FGPGameplayTags::Command_Move` → `GP.Command.Move` |
| Attack | `FGPGameplayTags::Command_Attack` → `GP.Command.Attack` |
| Mine | `FGPGameplayTags::Command_Mine` → `GP.Command.Mine` |
| Resource | `FGPGameplayTags::Resource_Node` → `GP.Resource.Node` |
| Resource accessor | `AGP_UnitBase::HasCapabilityTag(Resource_Node)` |
| Mine (UnitBase) | **IMPLEMENTED** |
| Mine (non-UnitBase) | **DEFERRED** — no canonical accessor |
| Cached selection | **NO** |
| Capability filter on issuers | **NO** |
| Input / RPC / execution | **NO** |
| Permanent test hook | **NO** |

---

## Mapping (preserved)

| Context | Result |
| --- | --- |
| `TargetActor == nullptr` | Move |
| Enemy UnitBase (TeamId ≥ 1, ≠ local) | Attack |
| UnitBase + Resource.Node CapabilityTag | Mine |
| Friendly UnitBase (TeamId ≥ 1, == local) | Move, TargetActor cleared |
| Neutral / unassigned UnitBase | Speculative Attack |
| Unknown non-unit | Move fallback |

---

## Normalization (preserved)

- Reset `OutRequest` first
- Prune invalid; dedupe first occurrence; preserve order; cap **24**
- Failure → default request only
- Temporary locals only; no member cache

---

## Validation

### Regression (passed)

| Check | Result |
| --- | --- |
| Standalone PIE | **PASS** |
| 2P Listen Server | **PASS** |
| Camera / click / marquee / control groups / debug boxes | **NONE** |
| Unexpected RMB / unit movement | **NONE** |
| Log errors/warnings | **NONE** |
| Assets / maps saved | **NO** |

### Functional request content (deferred)

Move / Attack / Mine / dedupe / cap-24 runtime checks require a real caller (e.g. Phase C RMB). **Not** a code-checkpoint blocker.

Builds / UHT retained **PASSED** from implementation (C++ unchanged at finalize).

---

## Next stage

Caller integration analysis (still **no** execution / RPC / movement). Do **not** claim functional BuildSmartCommand validation until a real caller exercises the branches.

---

## Stop condition
**CODE_DONE_FUNCTIONAL_VALIDATION_DEFERRED.**
Do **not** start RMB/input / RPC / execution / permanent hooks / full GP-S18/S19 from this finalize.
