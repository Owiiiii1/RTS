# Cursor Work Report — GP-S41M Finalization

## Status
**GP-S41M_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head
- Branch: `feature/gp-s41m-movement-shortest-yaw`
- Base: `origin/main` @ `d9df23143f256b2b2143fe66f5a0444f727452ae`
- Head: `3eff247cb155bd1faabb8466c50b1074a8315016`

## Operator PASS
Confirmed after operator rebuilt NavMesh:

- first Move no longer makes the initial sideways excursion
- movement-facing rotates by the shortest yaw path
- overall behavior is correct

## Final root causes
1. `TickComponent` facing used `FMath::RInterpConstantTo` on `FRotator`. That interpolator does not wrap yaw, so 350→10 could take the long path.
2. Blueprint/SCS primitives on mobile units could stay nav-relevant and carve static NavMesh holes at authored start locations (~90 cm first-Move projection and a sideways first leg).

## Final implementation
- `ComputeShortestYawStep` via `FindDeltaAngleDegrees` + `NormalizeAxis`; Tick facing uses `RotationSpeed * DeltaTime`. Existing movement Tick only.
- `AGP_MobileUnit` forces actor + all primitives (including Blueprint/SCS) off NavMesh generation. Buildings keep `NavigationObstacle`.
- `StripProjectedStartAnchor` removed. First-Move uses normal Recast semantics; `AcceptanceRadius` advances past a coincident start point.

## Targeted tests
| Command | Result |
| --- | --- |
| `gp.Movement.RunShortestYawContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | `Complete Failures=0 Cancelled=false` |

Full regression: **NOT RUN**.

## Final builds
| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `GP Win64 Development` | **PASS** |
| `GP Win64 Shipping` | **PASS** |

## Final audit
- Shortest signed yaw: 350→10 and 10→350 take the short direction; ±180 boundary works
- Rotation clamped to `RotationSpeed * DeltaTime`
- No new permanent Tick
- Path / serial / result semantics unchanged
- Mobile units cannot affect NavMesh generation
- Blueprint/SCS primitives forced non-navigation-relevant
- Buildings remain navigation obstacles
- `StripProjectedStartAnchor` removed
- First-Move path uses normal Recast semantics
- No combat / retaliation changes
- Operator rebuilt NavMesh manually; map/NavMesh **not committed**

## Protected assets
Untouched / not committed (config, maps, BPs, DAs, VFX).

## Merge
**NOT MERGED.** READY FOR MERGE.
