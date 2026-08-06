# GP-S28P2 — Resource Depletion, Registry, Reassignment and FIFO Recovery

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
- Branch: `feature/gp-s28p2-depletion-resource-reassignment`
- Base main: `86bcc9740fde0f19ac40c70f2f49298680f5f7d6` (Merge GP-S28P1)
- Implementation: `6c10937ffa3e1060e79ab1e8481e05c9f6aac6ed`
- Depends on: GP-S28P1 presentation hooks (merged)

## Goal
Safe one-shot ResourceNode depletion (vanish + deferred Destroy), authority ResourceNode registry, path-aware auto-reassignment / WaitingForResource, without changing mining cadence, Cargo, Storage, Threat, or combat.

## Delivered

### Depletion (`AGP_ResourceNode`)
- One-shot `bHasDepleted` (replicated) on PreviousAmount>0 → NewAmount≤0
- Occupancy clear **without** FIFO promotion
- `OnResourceDepleted(Node, PreviousAmount)` once (authority + client OnRep)
- Collision/nav interaction disabled; generated visual cleared (authored meshes left to actor Destroy)
- `DepletionDestroyDelaySeconds` default **0.25**; delay 0 → next-tick Destroy (never in Consume stack)
- New Mine/slot requests rejected after depletion

### Registry (`AGP_GameState`)
- Authority weak registry (multi-entry, not unique-per-team)
- Register/Unregister from Node BeginPlay/EndPlay
- `FindResourceCandidates` / `FindBestResourceCandidate` — path sync via NavigationSystem, no `GetAllActorsOfClass`
- Sort: free slot prefer → path length → direct distance → actor name
- `OnResourceNodeRegistered` / `OnResourceNodeUnregistered` for WaitingForResource wake

### Worker search tunables
- `ResourceSearchRadiusCm` default **3000**
- `MaxResourcePathLengthCm` default **6000**
- `bAllowManualTargetOutsideAutoSearchRadius` default **true**

### Reassignment (`UGP_UnitCommandComponent`)
- Prefer free reachable alternative when preferred node full
- Depleted/destroyed with empty cargo → auto reassign or `WaitingForResource`
- CargoFull still hauls first; post-haul retargets if deposit gone
- Command replacement unbinds registry wake + clears queue via existing StopMining
- `EGP_WorkerActivityState::WaitingForResource` + Mine executor state

### Tests
- `gp.Resource.RunDepletionReassignmentContractTest`
- Included in `gp.Resource.RunS28RegressionSuite`

### Builds
- GPEditor Win64 Development + UHT — **PASSED** (candidate)
- GP Dev/Shipping — deferred to finalization

## Operator validation setup
Level: 1× MainBase Team1; 2–3× `BP_GP_ResourceNode_Ferronite` (one low amount); 5–6 Workers; NavMesh covering candidates; one unreachable negative node.

Checks: 5 Workers on one node → ≤4 active + alt/FIFO; deplete → vanish + reassign; all depleted → WaitingForResource (no map-wide run); new node wake; Move cancels.

## Out of scope
HUD, multi drop-off, LogisticsHub, launch/Orbital, combat, DataAsset architecture, BP/Niagara/map commits.

## Operator-local assets
Left **untouched / uncommitted**: Blueprint/**, Materials/**, map/config/authored example edits.
