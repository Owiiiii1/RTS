# Cursor Work Report — GP-S28P2 Approach-Path + Settings Correction

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28p2-depletion-resource-reassignment` (no merge; main untouched)

## Second operator failure
After search-anchor fix: `HasAnchor=true`, `Radius=3000`, `RegistryCount=1`, but both `RequireFreeSlot` passes → `NoCandidate`, then WaitingWake log spam. Radius was not the cause.

## Exact confirmed rejection reason
Pathfinding to `ResourceNode::GetActorLocation()` (node center inside CollisionBox / nav-affecting obstacle) → path invalid / unprojectable. Prior reject diagnostics used `Verbose` and did not appear in the default Output Log.

## Approach-point path correction
- `GPResourceApproach` shared geometry (InteractionRange / DeltaZ / AcceptanceRadius / safety / CollisionBox extent)
- 8-direction projected approach samples; reject partial; pick shortest valid path
- `FGP_ResourceNodeCandidate.BestApproachLocation` populated
- Authority free-slot: live Active/Waiting array counts (`Active=0 Waiting=0 Max=4`)

## Settings class
- `UGP_ResourceGameplaySettings` (`UDeveloperSettings`, Config=Game)
- `GP/Config/DefaultGame.ini` section `[/Script/GPRuntime.GP_ResourceGameplaySettings]`
- Project Settings → Game → GP Resource Gameplay
- Waiting retry default **3.0s**; search/path/approach/depletion delay centralized

## Retry / log suppression
- One search pass with free-slot prefer sort
- WaitingWake identical no-candidate suppressed
- Per-candidate Accepted/Rejected at Log with exact enum reason
- Move/command replace still clears timer/subscriptions

## Tests
Extended `gp.Resource.RunDepletionReassignmentContractTest` (approach acceptance, free-slot, settings, prior anchor cases).

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Dev / Shipping | Deferred |

## Operator-local assets — untouched
DefaultEngine.ini, map, Blueprint/**, Materials/**, authored ResourceNode, Niagara.

## Commit SHA
Filled after commit on this branch tip.
