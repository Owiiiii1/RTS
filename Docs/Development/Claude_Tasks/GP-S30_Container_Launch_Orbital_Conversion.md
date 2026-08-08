# GP-S30 — Container Launch / Orbital Conversion

## Status
**GP-S30_HUD_CONTAINER_BREAKDOWN_READY_FOR_OPERATOR_VALIDATION**

## Slice Group
Slice 8 — Buildings + Orbital Drops

## Branch
`feature/gp-s30-container-launch-orbital-conversion`  
Base: `main` @ `89ce3c50ebd05a4bf1e58a5b4e117544dc68cb8f`  
Prior finalization: `030efc55469153a8d1465ac81ae3996c1bd391cb` — **not merged**; operator requested HUD UX follow-up.

## Operator validation
Previous PIE PASS for launch economy + HUD lifecycle.  
**Current:** awaiting retest of container breakdown TEMP HUD (not merge-ready).

## TEMP HUD (this follow-up)

```
База: <GetTotalStored> / <GetTotalCapacity>
Контейнер 1 — <amount>
…
Контейнер N — <amount>
Orbital: <OrbitalFerronite>
[Launch Container]
```

- No hardcoded 5 / 500
- Stable container indices
- RebuildWidget lifecycle preserved
- Launch button / RPC / Storage / GAS unchanged

## Contracts
| Command | Result |
| --- | --- |
| `gp.Resource.RunContainerLaunchHUDContractTest` | Failures=0 |
| `gp.Resource.RunContainerLaunchContractTest` | Failures=0 |
| `gp.Resource.RunS28RegressionSuite` | Failures=0 |

## Builds
- GPEditor Win64 Development + UHT — **PASS**
- GP Development / Shipping — **NOT RUN** (after this C++ HUD change)

## Stop Condition
Operator retests HUD breakdown. Do **not** merge until PASS. Do **not** auto-start GP-S31.
