# GP-S28P2 — Resource Depletion, Registry, Reassignment and FIFO Recovery

## Status
**GP-S28P2_READY_FOR_MERGE**

## Baseline
- Branch: `feature/gp-s28p2-depletion-resource-reassignment`
- Base main: `86bcc9740fde0f19ac40c70f2f49298680f5f7d6` (GP-S28P1; main does not yet contain P2)
- Implementation tip (operator-validated): `64f8c8567dc1f004abcc3b9bc5917794f2132b08`
- Test correction commit: `aa405546f0267eb5f77c7bd9c282219426bdacb5`

## Operator validation — PASSED
| Scenario | Result |
| --- | --- |
| A. Depletion → haul → unload → Node B mining | **PASSED** |
| B. 5 Workers, Node A full, 5th → free Node B | **PASSED** |
| C. FIFO: 4 Mining / 5th WaitingForSlot / promote / no crash | **PASSED** |
| D. Partial cargo (10/50) → haul → unload → then WaitingForResource | **PASSED** |

## Finalization test correction
Initial headless Failures=1:
- `HeldClearedAfterDepleteHaul` — obsolete assertion; updated to P2 held-Mine persistence
- `AnchorSearchCenterFindsNodeB` — harness PathStart/NodeB spawn; navigable Base PathStart + approach-reachable alternate

Production gameplay unchanged.

## Tests (headless `-game -NullRHI`)
| Command | Exact result |
| --- | --- |
| `gp.Resource.RunDepletionReassignmentContractTest` | `Complete Failures=0 Cancelled=None` |
| `gp.Resource.RunS28RegressionSuite` | `Complete Failures=0` |

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** (after test correction) |
| GP Win64 Development | **PASSED** (finalization; production unchanged) |
| GP Win64 Shipping | **PASSED** (finalization; production unchanged) |

## Settings
`UGP_ResourceGameplaySettings` — Project Settings → Game → GP Resource Gameplay; `GP/Config/DefaultGame.ini`.

## Operator-local assets
Untouched / uncommitted: Blueprint/**, Materials/**, map, DefaultEngine.ini, Niagara, authored ResourceNode.

## Stop condition
READY_FOR_MERGE pending tech-lead / operator merge approval. Do **not** start GP-S28P3 or merge in this close-out.
