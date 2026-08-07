# GP-S28P3 — Drop-Off Resilience (MainBase Haul Recovery)

## Status
**GP-S28P3_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Slice Group
Slice 6 follow-on — Resource Playable Pass (P3)

## Code Allowed
**YES**

## Depends On
- GP-S28 merged (haul / Storage / Threat)
- GP-S28P1 merged @ `86bcc9740fde0f19ac40c70f2f49298680f5f7d6`
- GP-S28P2 merged @ `e90b7bd48fb9080a881e6dda7be889eaa99a3161`
- Spec base main: `3b1ae705d8b15fd54daf06553337885d0420dc57`
- Canonical: GDD/02, GDD/06, TDD/06, TDD/07, TDD/13, ADR-0009, `Resource_Playable_Pass_Audit.md`

## Branch
`feature/gp-s28p3-dropoff-resilience` (no merge)

## Candidate commit
`fa98a64175b25c16244fe234aadff627896ad213`

## Goal
MainBase missing / destroyed / unreachable → preserve Cargo + Mine intent/search anchor → `WaitingForDropOff` → wake/retry → haul → unload → existing P2 PostDropOff. No permanent Tick. MainBase remains the only MVP Ferronite drop-off.

## Implementation (actual)

### State rename
- `EGP_HaulExecutionState::WaitingForStorage` → `WaitingForDropOff`
- `EGP_WorkerActivityState::WaitingForStorage` → `WaitingForDropOff`
- Audit: unused production-wise before P3; no duplicate parallel state.

### GameState registry events
`AGP_GameState` authority-only native multicasts:
- `OnMainBaseRegistered` — after successful registry add (not AlreadyRegistered / rejected)
- `OnMainBaseUnregistered` — only when a concrete MainBase was removed (`Removed > 0`)

Not replicated RPCs.

### UnitCommand haul recovery (`UGP_UnitCommandComponent`)
- `EnterWaitingForDropOff(Reason)` central helper
- Active haul (`ReturningToBase` / `DroppingOff`): bind `OnMainBaseUnregistered` filtered to current `HaulMainBase`
- Waiting: bind `OnMainBaseRegistered` wake once; safety retry timer; no permanent Tick
- Resume deferred next-tick (`ExecuteScheduledDropOffHaulResume`) to avoid sync recursion
- Missing / PathRejected / MoveFailed / InvalidBeforeDropOff / MainBaseDestroyed + Cargo > 0 → wait (not `FinishHaulChain(true)`)
- Cargo ≤ 0 keeps prior terminal fail semantics
- Storage-full overflow LOST unchanged (not WaitingForDropOff)
- Threat only after Accepted storage add

### Settings
`UGP_ResourceGameplaySettings::DropOffRetrySeconds` default `3.0` (clamp ≥ 0.1)  
`GP/Config/DefaultGame.ini` → `[/Script/GPRuntime.GP_ResourceGameplaySettings]`  
Project Settings → Game → GP Resource Gameplay

### Diagnostics
Transition logs only:
- `GP DropOffWait Enter Reason=… Cargo=… Team=…`
- `GP DropOffWait Wake Reason=MainBaseRegistered`
- `GP DropOffWait Retry Result=StillMissing|AttemptResume|StillUnreachable` (spam-suppressed)

## State machine
```
CargoFull / DepositDepleted(+Cargo>0)
  → ReturningToBase
      → DroppingOff → ContinueMineAfterSuccessfulHaul (P2)
      → WaitingForDropOff (missing / destroyed / unreachable)
          → ReturningToBase (register wake / safety retry)
          → Idle (explicit command replacement; Cargo preserved)
```

## Registry / subscription model
| Phase | Bind | Filter / action |
| --- | --- | --- |
| Active haul | `OnMainBaseUnregistered` | exact current haul target → cancel move → WaitingForDropOff |
| WaitingForDropOff | `OnMainBaseRegistered` | same team + Storage + Cargo > 0 → wake once → resume haul |
| Waiting | unregister | no-op |
| Replacement / EndPlay / drop-off success | clear both binds + retry timer + pending resume |

At most one unregister bind, one register bind, one retry timer per UnitCommand.

## Retry
`DropOffRetrySeconds` from settings (not hardcoded in UnitCommand). Armed only while WaitingForDropOff.

## Tests
| Command | Result |
| --- | --- |
| `gp.Resource.RunDropOffResilienceContractTest` | `Complete Failures=0 Cancelled=None` |
| `gp.Resource.RunDepletionReassignmentContractTest` | `Complete Failures=0 Cancelled=None` |
| `gp.Resource.RunS28RegressionSuite` | `Complete Failures=0` |

Suite includes P3 contract after P2. Hauling ownership case updated for WaitingForDropOff (held retained).

## Build
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Win64 Development | deferred (finalization) |
| GP Win64 Shipping | deferred (finalization) |

## Operator validation (PIE)

| Scenario | Result | Notes |
| --- | --- | --- |
| **A** — Missing MainBase + registration wake | **PASS** | WaitingForDropOff; Cargo preserved; runtime MainBase register wakes haul; deliver; Accepted/Threat OK; P2 PostDropOff continues |
| **B** — MainBase destroyed mid-haul | **PASS** | Movement cancelled; `Reason=MainBaseDestroyed`; Cargo preserved; WaitingForDropOff; replacement wake → deliver; `ReturnToDeposit=true`; remine after drop-off |
| **C** — Existing MainBase unreachable → path restored → retry | **DEFERRED** | Not a P3 failure — see below / [`../DEFERRED_VALIDATION_GP-S28P3_Scenario_C.md`](../DEFERRED_VALIDATION_GP-S28P3_Scenario_C.md) |
| **D** — Explicit Move replaces WaitingForDropOff | **PASS** | Haul cancelled `CommandReplaced`; Held Mine → Move; Cargo preserved; later MainBase register does **not** stale-wake; Move completes; no haul resurrect |

### Scenario C — deferred (architecture), not failed
Current `UGP_MovementComponent` is **not** full runtime NavMesh/path-following movement:
- UnitCommand runs pre-accept nav/path/approach query;
- after accept, MovementComponent tick moves the actor toward the destination;
- a runtime wall / BlockingVolume / NavModifier between Worker and destination is **not** a valid way to force MoveFailed — the Worker can still physically cross after destination accept.

Manual C via wall/BlockingVolume/NavModifier is therefore **invalid** for P3 recovery validation. Automated contract unreachable coverage (forced approach reject / wait / resume) remains **PASS** and is retained.

**Abandoned (not implemented):** proposed `gp.Resource.MakeTestMainBaseUnreachable` / `MakeTestMainBaseReachable`. Kept helpers: `gp.Resource.SpawnTestMainBase`, `gp.Resource.DestroyTestMainBase`.

## Out of scope (unchanged)
P4 HUD, client MainBase registry, LogisticsHub drop-off, storage-full redesign, orbital/Score/combat/construction, queued commands, save/load, Blueprint gameplay authority, map/content authoring.

## Stop condition
Status remains **GP-S28P3_CODE_READY_OPERATOR_VALIDATION_PENDING** pending finalization workflow. Manual C is an accepted deferred dependency on future canonical navigation/path-following movement — **not** a P3 gameplay blocker (deterministic contract unreachable coverage already exists). Do **not** merge in this note. Do **not** start P4.
