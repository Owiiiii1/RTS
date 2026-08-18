# Cursor Work Report — GP-S41M Mobile Nav Generation

## Status
**GP-S41M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head
- Branch: `feature/gp-s41m-movement-shortest-yaw`
- Base: `origin/main` @ `d9df23143f256b2b2143fe66f5a0444f727452ae`
- Head: recorded after commit

## Operator result
Second FAIL / residual first-Move sideways leg. Shortest yaw itself is correct (long arc gone).

Two factual first-Move projections (~90 cm to nav):

| Case | ActualStart XY | ProjectedStart XY | Dist | Runtime Path0 |
| --- | --- | --- | --- | --- |
| 1 | (-733.49, -2229.23) | (-817.84, -2265.38) | 91.8 | (-836, -2223) |
| 2 | (-1360.45, -1121.76) | (-1449.64, -1132.91) | 89.9 | (-1463, -1026) |

Later repaths on the same move drop to Dist 24.9 then 0.0.

## Hypothesis
**Confirmed.**

`APawn::bCanAffectNavigationGeneration` defaults false, but the engine documents that **components can still affect generation independently**. `UpdateNavigationRelevance()` is empty on `APawn`. Authored BP meshes on `BP_SalvageWalker` / `BP_Worker` were only warned (`AuthoredNavigationWarnings`), not disabled.

Native capsule is already `SetCanEverAffectNavigation(false)`. Blueprint/SCS collision meshes with Pawn-block remain nav-relevant and carve a hole at the authored location during **editor static bake**. First Move projects to the hole rim (~90 cm). After the unit leaves that footprint, later projections match ActualStart.

Runtime Recast in `-game` is `Static` (`SupportsRuntimeGeneration=false`), so already-baked tiles do not update from newly spawned actors. That matches “first Move only” after PIE.

## Exact production correction
On `AGP_MobileUnit` only (Worker / SalvageWalker / Unit). Not `AGP_UnitBase` / buildings.

- `ApplyMobileNavigationGenerationPolicy` → `SetCanAffectNavigationGeneration(false, true)`
- Override `UpdateNavigationRelevance` to force **every** primitive `SetCanEverAffectNavigation(false)` (native + Blueprint/SCS)
- Called from `OnConstruction`, `PostInitializeComponents`, `BeginPlay` so editor bake and PIE both see the policy

Buildings keep `NavigationObstacle` nav-relevant. Untouched.

**Operator:** rebuild NavMesh on playable maps once so old authored-unit holes leave the static Recast data. This slice does not modify maps.

## StripProjectedStartAnchor
**Removed.** It was compensation for the self-hole. Recast start == query start is skipped by `AcceptanceRadius` when Actual ≈ Projected. Stripping it would skip a real off-nav entry waypoint. Tick still advances past a coincident Path0.

## Shortest-yaw helper
**Retained.** `ComputeShortestYawStep` / `FindDeltaAngleDegrees` / `NormalizeAxis`.

## Tests
| Command | Result |
| --- | --- |
| `gp.Movement.RunShortestYawContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | `Complete Failures=0 Cancelled=false` |

## Candidate build
`GPEditor Win64 Development` + UHT **PASS**.  
`GP` Win64 Development / Shipping: **NOT RUN**.

## Protected assets
Untouched / not committed (config, maps, BPs, DAs, VFX).

## Unrelated error (recorded only)
`GP BuildingDefinitionLoadFailed` MainBase / `DA_GP_Buildings_MainBase` `ResolveFailedUsingFallback`. Not fixed.

## Merge
**NOT MERGED. NOT FINALIZED.**
