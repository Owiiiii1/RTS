# GP-S28P2 — Resource Depletion, Registry, Reassignment and FIFO Recovery

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
- Branch: `feature/gp-s28p2-depletion-resource-reassignment`
- Base main: `86bcc9740fde0f19ac40c70f2f49298680f5f7d6`
- Approach-path / settings: `53e5ff944730180764731c75fb495a38adeb91ab`
- FIFO crash correction: `42c99f61e4a278cb024664c83dd1dcb504e2f7a6`

## Goal
Safe one-shot ResourceNode depletion, registry, path-aware reassignment / WaitingForResource, and stable FIFO WaitingForSlot — without changing mining cadence, Cargo, Storage, Threat, or combat.

## Operator failure #4 — partial cargo stranded on depletion
Scenario: Node Amount=10, CargoCapacity=50, one Worker, MainBase present, no alternate node.

Observed: Worker mines 10 → node depletes → Cargo=10 → immediate `PostDepletion` / `WaitingForResource` (no haul).

### Root cause
`ConsumeResource` depletion → `ClearOccupancyWithoutPromotion` → `StopMining(TargetEndPlay)` **before** `AddCargo`, so UnitCommand saw Cargo=0 and reassigned; cargo was credited afterward and stranded.

### Corrected terminal priority
`DepositDepleted` + Cargo > 0 → haul (`ReturnToDeposit=false`) → drop-off → `TryAutoReassignMine(PostDropOff)` → else `WaitingForResource`.

Zero cargo → immediate PostDepletion reassignment / WaitingForResource (no unnecessary haul).

### Invariant
Normal playable flow: `WaitingForResource` ⇒ Cargo=0 for current resource type.
Non-shipping: Error log + haul redirect if MainBase exists (no recursive transition).
MainBase missing/unreachable: existing failure behavior only (no GP-S28P3 recovery).

## Preserved PASSED operator cases
1. depletion → full/normal haul → Node B reassignment
2. 4 Workers on Node A + 5th → free Node B
3. single-node FIFO (4 Mining / 5th WaitingForSlot / promote / no crash)
4. approach-point navigation fix
5. WaitingForResource retry/log suppression
6. settings class

## Tests
`gp.Resource.RunDepletionReassignmentContractTest` extended:
- partial cargo depletion → haul → unload → Threat += accepted × ThreatPerStoredUnit → WaitingForResource (cargo 0)
- partial cargo + alternate Node B → haul → retarget B
- zero cargo depletion → no unnecessary haul
- prior FIFO / anchor / approach cases retained

PIE result **not claimed** (not run non-interactively).

## Builds
- GPEditor Win64 Development + UHT — **PASSED**
- GP Dev/Shipping — deferred

## Operator-local assets
Untouched / uncommitted: Blueprint/**, Materials/**, map, DefaultEngine.ini, Niagara.
