# GP-S30 — Container Launch / Orbital Conversion

## Status
**GP-S30_FULL_STORAGE_WORKER_FIX_READY_FOR_OPERATOR_RETEST**

## Slice Group
Slice 8 — Buildings + Orbital Drops

## Branch
`feature/gp-s30-container-launch-orbital-conversion`  
Base: `main` @ `89ce3c50ebd05a4bf1e58a5b4e117544dc68cb8f`  
Prior finalization: `030efc55469153a8d1465ac81ae3996c1bd391cb` — **not merged**.

## Operator validation
Previous PIE PASS for launch economy + HUD.  
**Current:** operator retest pending for full-storage Worker FSM fix (not merge-ready).

## Full-storage Worker fix (this follow-up)

- Symptom: Workers mined/looped while Storage full
- Root: overflow `ClearCargo` + `ContinueMineAfterSuccessfulHaul`
- Semantics: full/partial unload with remainder → `WaitingForDropOff`; cargo-first; resume on `OnStorageChanged`
- Capacity unchanged (5×100=500)

## Contracts
| Command | Result |
| --- | --- |
| `gp.Resource.RunS28RegressionSuite` | Failures=0 |
| `gp.Resource.RunDropOffResilienceContractTest` | Failures=0 |
| `gp.Resource.RunContainerLaunchContractTest` | Failures=0 |
| `gp.Resource.RunContainerLaunchHUDContractTest` | Failures=0 |

## Builds
- GPEditor Win64 Development + UHT: **PASS**
- GP Win64 Development / Shipping: **NOT RUN**

## Report
`Docs/Development/Cursor_Work_Report.md`
