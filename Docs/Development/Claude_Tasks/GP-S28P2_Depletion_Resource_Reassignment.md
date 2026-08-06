# GP-S28P2 — Resource Depletion, Registry, Reassignment and FIFO Recovery

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
- Branch: `feature/gp-s28p2-depletion-resource-reassignment`
- Base main: `86bcc9740fde0f19ac40c70f2f49298680f5f7d6` (Merge GP-S28P1)
- Implementation: `6c10937ffa3e1060e79ab1e8481e05c9f6aac6ed`
- Correction (search-anchor): see latest commit on this branch
- Depends on: GP-S28P1 presentation hooks (merged)

## Goal
Safe one-shot ResourceNode depletion (vanish + deferred Destroy), authority ResourceNode registry, path-aware auto-reassignment / WaitingForResource, without changing mining cadence, Cargo, Storage, Threat, or combat.

## Operator failure (Part 1)
After Node A depletion + haul drop-off (`HaulDropOffComplete … Accepted=20`, `ReturnToDeposit=false`), Worker entered `WaitingForResource` despite Node B remaining available near the depleted cluster.

**Root cause:** `FindAutoResourceCandidate` used Worker location at MainBase as both search-radius origin and nav path start. Node B was outside `ResourceSearchRadiusCm` from the base even though it was inside the original mining cluster.

## Correction — persistent Mine search anchor
- Server-local `MineSearchAnchorLocation` + `bHasMineSearchAnchor` on Mine executor
- Set on manual Mine accept to original ResourceNode location; survives target Destroy / haul
- Cleared on Move / Attack / Stop / command replace / Mine cancel / EndPlay (`ResetMineExecutor`)
- Kept across `WaitingForResource`
- Auto-retarget does **not** move anchor to MainBase (original manual target remains SearchCenter)

## Search API semantics
`FGP_ResourceNodeSearchQuery`:
- `SearchCenter` — `ResourceSearchRadiusCm` filter (Mine search anchor)
- `PathStart` — current Worker location for nav reachability / `MaxPathLengthCm`
- Manual Mine outside auto radius still allowed

## Diagnostics (non-shipping, event-only)
On reassignment search: reason (`PostDepletion` / `PostDropOff` / `WaitingWake` / `SlotFullAlternative`), SearchCenter, PathStart, radius, max path, registry count, per-candidate rejects, selected candidate, or `ResourceReassignmentNoCandidate`. No Tick spam; WaitingForResource safety retry remains ≤1 Hz.

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
- `FindResourceCandidates` / `FindBestResourceCandidate` — SearchCenter + PathStart split
- Sort: free slot prefer → path length → direct distance → actor name
- `OnResourceNodeRegistered` / `OnResourceNodeUnregistered` for WaitingForResource wake

### Worker search tunables
- `ResourceSearchRadiusCm` default **3000**
- `MaxResourcePathLengthCm` default **6000**
- `bAllowManualTargetOutsideAutoSearchRadius` default **true**

### Reassignment (`UGP_UnitCommandComponent`)
- Prefer free reachable alternative when preferred node full
- Depleted/destroyed with empty cargo → auto reassign or `WaitingForResource`
- CargoFull still hauls first; post-haul retargets if deposit gone (SearchCenter = Mine anchor)
- Command replacement unbinds registry wake + clears queue via existing StopMining
- `EGP_WorkerActivityState::WaitingForResource` + Mine executor state

### Tests
- `gp.Resource.RunDepletionReassignmentContractTest` — includes post-drop-off anchor regression, Move clears anchor, Waiting wake radius, CargoFull haul first, no permanent Tick
- Included in `gp.Resource.RunS28RegressionSuite`

### Builds
- GPEditor Win64 Development + UHT — **PASSED** (correction candidate)
- GP Dev/Shipping — deferred to finalization

## Operator validation setup
Level: 1× MainBase Team1; 2–3× `BP_GP_ResourceNode_Ferronite` (one low amount); 5–6 Workers; NavMesh covering candidates; one unreachable negative node.

**Critical re-check:** MainBase farther than `ResourceSearchRadiusCm` from resource cluster; after Node A depletes and Worker unloads at base, Worker must retarget Node B (not `WaitingForResource`).

Checks: 5 Workers on one node → ≤4 active + alt/FIFO; deplete → vanish + reassign; all depleted → WaitingForResource (no map-wide run); new node wake; Move cancels.

## Out of scope
HUD, multi drop-off, LogisticsHub, launch/Orbital, combat, DataAsset architecture, BP/Niagara/map commits.

## Operator-local assets
Left **untouched / uncommitted**: Blueprint/**, Materials/**, map/config/authored example edits.
