# GP-S28P2 — Resource Depletion, Registry, Reassignment and FIFO Recovery

## Status
**GP-S28P2_READY_FOR_MERGE**

## Baseline
- Branch: `feature/gp-s28p2-depletion-resource-reassignment`
- Base main: `86bcc9740fde0f19ac40c70f2f49298680f5f7d6` (GP-S28P1 merged; main does not yet contain P2)
- Implementation tip (operator-validated): `64f8c8567dc1f004abcc3b9bc5917794f2132b08`

## Goal
Safe one-shot ResourceNode depletion, GameState registry, path-aware reassignment / WaitingForResource, FIFO stable wait, and partial-cargo depletion haul priority — without changing mining cadence, Cargo/Storage/Threat semantics, or combat.

## Operator validation — PASSED

| Scenario | Result |
| --- | --- |
| A. Depletion → haul → unload → Node B mining | **PASSED** |
| B. 5 Workers, Node A full, 5th → free Node B | **PASSED** |
| C. FIFO: 4 Mining / 5th WaitingForSlot / promote / no crash | **PASSED** |
| D. Partial cargo (10/50) → haul → unload → then WaitingForResource | **PASSED** |

## Corrected behaviors (retained)
- MineSearchAnchor + SearchCenter / PathStart split
- Approach-point navigation (not actor center)
- FIFO stable WaitingForSlot (no same-target retarget loop)
- Depletion + Cargo > 0 → haul first (`ReturnToDeposit=false`) → PostDropOff reassignment
- WaitingForResource normal flow ⇒ Cargo=0 (non-shipping Error + haul redirect if MainBase exists)

## Scope (branch vs main)
ResourceNode depletion lifecycle; GameState registry; candidate search; approach points; Worker reassignment; FIFO; WaitingForResource; settings; tests/docs. No combat/projectile/Blueprint authority/map content commits.

## Tests (headless `-game -NullRHI` on `L_PrototypeArena`)
| Command | Exact result |
| --- | --- |
| `gp.Resource.RunDepletionReassignmentContractTest` | `Complete Failures=1 Cancelled=None` — FAIL `AnchorSearchCenterFindsNodeB` (nav/approach under NullRHI; not claimed PASS) |
| `gp.Resource.RunS28RegressionSuite` | `Complete Failures=1` — stopped after `gp.Worker.RunHaulingContractTest` FAIL `HeldClearedAfterDepleteHaul` (headless harness; not claimed PASS) |

Operator runtime scenarios A–D remain authoritative PASSED. No gameplay changes made for automation harness.

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** (finalization re-run; C++ unchanged since `64f8c85…`) |
| GP Win64 Development | **PASSED** |
| GP Win64 Shipping | **PASSED** |

## Settings
`UGP_ResourceGameplaySettings` — Project Settings → Game → GP Resource Gameplay; defaults in `GP/Config/DefaultGame.ini`.

## Operator-local assets
Untouched / uncommitted: Blueprint/**, Materials/**, map, DefaultEngine.ini, Niagara, authored ResourceNode.

## Stop condition
READY_FOR_MERGE pending tech-lead review / operator merge approval. Do **not** start GP-S28P3 or merge in this close-out.
