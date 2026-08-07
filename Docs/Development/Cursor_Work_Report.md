# Cursor Work Report — GP-S28P2 Partial-Cargo Depletion Correction

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28p2-depletion-resource-reassignment` (no merge; main untouched)

## Fourth operator failure
Node Amount=10, CargoCapacity=50, one Worker, MainBase present, no alternate ResourceNode:

Worker mines 10 → deposit depletes → Cargo=10 → immediate `ResourceReassignmentNoCandidate Reason=PostDepletion` / `WaitingForResource` (no haul).

## Root cause
`ExecuteMiningCycle`: `ConsumeResource` depletion clears occupancy → `StopMining(TargetEndPlay)` before `AddCargo`, so UnitCommand reassigned with Cargo=0; cargo credited afterward and stranded.

## Corrected terminal priority
DepositDepleted + Cargo > 0 → haul (`ReturnToDeposit=false`) → unload → `PostDropOff` reassignment → else WaitingForResource.

Zero cargo → immediate reassignment / WaitingForResource (no unnecessary haul).

## Invariant
Normal flow: WaitingForResource ⇒ Cargo=0. Non-shipping Error + haul redirect if MainBase exists (no recursive transition). MainBase failure: existing behavior only.

## Test results
| Command | Result |
| --- | --- |
| `gp.Resource.RunDepletionReassignmentContractTest` | Extended (partial / alt / zero-cargo); **PIE not run non-interactively — operator pending** |
| `gp.Resource.RunS28RegressionSuite` | **Not run non-interactively — operator pending** |

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Dev / Shipping | Deferred |

## Operator-local assets — untouched
DefaultEngine.ini, map, Blueprint/**, Materials/**, authored ResourceNode, Niagara.

## Commit SHA
`64f8c8567dc1f004abcc3b9bc5917794f2132b08`
