# GP-S28P3 — Drop-Off Resilience (MainBase Haul Recovery)

## Status
**GP-S28P3_READY_FOR_MERGE**

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
`feature/gp-s28p3-dropoff-resilience` (no merge until tech-lead approval)

## Implementation summary
- Rename unused `WaitingForStorage` → `WaitingForDropOff` (haul + Worker activity)
- `AGP_GameState` authority multicasts `OnMainBaseRegistered` / `OnMainBaseUnregistered`
- UnitCommand: `EnterWaitingForDropOff`, active-haul unregister (current target), waiting register wake, deferred resume, `DropOffRetrySeconds` safety retry
- Missing / destroyed / unreachable MainBase + Cargo > 0 → wait; preserve Cargo + held Mine / search anchor
- Explicit command replacement clears subscriptions/timer — no stale haul resurrect
- Threat only after Accepted storage add; storage overflow LOST unchanged
- MainBase remains sole MVP Ferronite drop-off
- Non-shipping helpers: `gp.Resource.SpawnTestMainBase` / `DestroyTestMainBase`
- Contract: `gp.Resource.RunDropOffResilienceContractTest` + S28 suite entry

## Operator validation (final)
| Scenario | Result |
| --- | --- |
| **A** — Missing MainBase + registration wake | **PASS** |
| **B** — MainBase destroyed during active haul | **PASS** |
| **C** — existing MainBase unreachable → path restored | **DEFERRED** (not failed) |
| **D** — explicit Move replaces WaitingForDropOff | **PASS** |

Scenario C deferred to **future canonical navigation/path-following movement stage** — see [`../DEFERRED_VALIDATION_GP-S28P3_Scenario_C.md`](../DEFERRED_VALIDATION_GP-S28P3_Scenario_C.md). Accepted deferred validation; not a P3 blocker (deterministic contract unreachable coverage PASS).

## Automated tests (finalization re-run)
| Command | Result |
| --- | --- |
| `gp.Resource.RunDropOffResilienceContractTest` | `Complete Failures=0 Cancelled=None` |
| `gp.Resource.RunDepletionReassignmentContractTest` | `Complete Failures=0 Cancelled=None` |
| `gp.Resource.RunS28RegressionSuite` | `Complete Failures=0` |

## Builds (finalization)
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Win64 Development | **PASSED** |
| GP Win64 Shipping | **PASSED** |

## Scope audit (vs `3b1ae705…`)
Allowed only: WaitingForDropOff rename; MainBase registry delegates; haul wait/wake/retry; `DropOffRetrySeconds`; P3 contracts/suite; non-shipping helpers; docs.  
Absent: P4 HUD, LogisticsHub drop-off, storage-full redesign, orbital/Score, combat, construction, path-following redesign, Blueprint/map/content commits.

## Invariants confirmed
- Cargo preserved while WaitingForDropOff
- Held Mine / search anchor preserved for recovery
- Command replacement prevents stale resurrection
- Active-haul unregister filters exact current target
- Waiting register wake: same-team valid MainBase + Storage + Cargo
- No permanent Tick for waiting (timer + events only)
- At most one wake bind / unregister bind / retry timer
- Threat only after Accepted storage add
- Storage overflow LOST unchanged
- MainBase-only Ferronite drop-off; LogisticsHub not a drop-off path

## Stop condition
**READY_FOR_MERGE.** Merge only after tech-lead approval. Do **not** start P4 until explicitly assigned.
