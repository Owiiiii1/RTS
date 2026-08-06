# GP-S28P2 — Resource Depletion, Registry, Reassignment and FIFO Recovery

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
- Branch: `feature/gp-s28p2-depletion-resource-reassignment`
- Base main: `86bcc9740fde0f19ac40c70f2f49298680f5f7d6` (Merge GP-S28P1)
- Implementation: `6c10937ffa3e1060e79ab1e8481e05c9f6aac6ed`
- Search-anchor correction: `563a025296fd5311ce8066259f22dba891063950`
- Approach-path / settings correction: see latest commit on this branch

## Goal
Safe one-shot ResourceNode depletion (vanish + deferred Destroy), authority ResourceNode registry, path-aware auto-reassignment / WaitingForResource, without changing mining cadence, Cargo, Storage, Threat, or combat.

## Operator failure #1 (resolved)
Post-drop-off search used Worker@MainBase for radius — fixed via persistent Mine search anchor (`SearchCenter` vs `PathStart`).

## Operator failure #2
Anchor/radius correct (`RegistryCount=1`, Node B beside Worker/base), but still `NoCandidate`. Per-candidate reject logs were Verbose-only (invisible). Confirmed cause: path queries targeted `ResourceNode::GetActorLocation()` (center inside CollisionBox / nav obstacle).

## Correction — approach-point search
- Shared `GPResourceApproach` helper (same 3D Mining Range budget as UnitCommand)
- Multi-direction projected approach samples around node; shortest valid non-partial path wins
- Candidate carries `BestApproachLocation`; Move may still recompute its own approach
- Authority free-slot uses live `ActiveMiners` counts, not stale replicated fields alone

## Settings — `UGP_ResourceGameplaySettings`
- Project Settings → Game → GP Resource Gameplay
- Config: `GP/Config/DefaultGame.ini` (`[/Script/GPRuntime.GP_ResourceGameplaySettings]`)
- Defaults: SearchRadius 3000, MaxPath 6000, WaitingRetry **3.0s**, DepletionDestroyDelay 0.25, ApproachSafety 25, ApproachDirections 8
- Worker/ResourceNode read these globals (no duplicate hardcoded search/delay defaults)

## WaitingForResource / logs
- Safety retry interval from settings (3s); event wake remains primary
- Single search pass (prefer free via sort; no double RequireFreeSlot sweep)
- Per-candidate `GP ResourceCandidate Accepted|Rejected` at Log with exact reason
- Identical WaitingWake no-candidate summaries suppressed

## Tests
`gp.Resource.RunDepletionReassignmentContractTest` — depletion, anchor/post-drop-off, approach acceptance, free-slot counts, settings, Move clears anchor, wake radius, no permanent Tick.

## Builds
- GPEditor Win64 Development + UHT — **PASSED**
- GP Dev/Shipping — deferred to finalization

## Operator re-validation
Same layout (Node A Amount=20, Node B beside cluster/base). Expect after unload: `ResourceCandidate Accepted` → `ResourceReassignmentSelected` → `MineApproachRequested`. Not `WaitingForResource`.

## Operator-local assets
Left **untouched / uncommitted**: Blueprint/**, Materials/**, map, DefaultEngine.ini, Niagara.
