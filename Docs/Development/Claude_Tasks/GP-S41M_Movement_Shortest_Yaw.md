# GP-S41M — Movement Shortest Yaw Path

## Status
**IMPLEMENTED / OPERATOR PASS / FINALIZED / READY FOR MERGE**

**NOT MERGED.**

**Code Allowed: YES**

## Slice Group
Post-GP-S40R movement facing defect

## Branch
`feature/gp-s41m-movement-shortest-yaw`  
Base: `origin/main` @ `d9df23143f256b2b2143fe66f5a0444f727452ae`  
Implementation tip: `3eff247cb155bd1faabb8466c50b1074a8315016`

## Operator PASS
After operator rebuilt NavMesh: first Move has no sideways excursion; facing uses shortest yaw; overall behavior is correct.

## Final root causes
1. `FMath::RInterpConstantTo` on `FRotator` movement-facing could choose the long yaw wrap (350→10 took the long path).
2. Blueprint-authored primitive components on mobile units could remain nav-relevant and carve static NavMesh holes at authored starting locations (~90 cm first-Move projection and a sideways initial leg).

Mobile units must never contribute to NavMesh generation. Buildings still may (`NavigationObstacle`).

## Implementation
`UGP_MovementComponent::ComputeShortestYawStep` — `FindDeltaAngleDegrees` + `NormalizeAxis`. Tick facing uses `RotationSpeed * DeltaTime`. Existing movement Tick only. No new Tick.

`AGP_MobileUnit::ApplyMobileNavigationGenerationPolicy` + `UpdateNavigationRelevance` force actor + all primitives (native and Blueprint/SCS) off generation.

`StripProjectedStartAnchor` **removed**. First-Move uses normal Recast semantics.

## Recorded out of scope
`GP BuildingDefinitionLoadFailed` MainBase / `DA_GP_Buildings_MainBase` (`ResolveFailedUsingFallback`). Not fixed here.

## Validation (final)

| Check | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `GP Win64 Development` | **PASS** |
| `GP Win64 Shipping` | **PASS** |
| `gp.Movement.RunShortestYawContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | `Complete Failures=0 Cancelled=false` |

## Out of scope

Attack facing in UnitCommand, retaliation, pathfinding, acceleration, speed tuning, operator assets, map/NavMesh commit.
