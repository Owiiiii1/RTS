# GP-S41M — Movement Shortest Yaw Path

## Status
**GP-S41M_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

**Code Allowed: YES** (assigned implementation candidate)

## Slice Group
Post-GP-S40R movement facing defect

## Branch
`feature/gp-s41m-movement-shortest-yaw`  
Base: `origin/main` @ `d9df23143f256b2b2143fe66f5a0444f727452ae`  
Implementation tip: `5de48e900fc7ceef75bc3bc983949b94537b7ab7`

## Reproduced root cause

`UGP_MovementComponent::TickComponent` used `FMath::RInterpConstantTo` on `FRotator`.

That helper interpolates yaw as a raw component (`Target.Yaw - Current.Yaw`) and does **not** wrap to `[-180°, +180°]`.

Repro in `gp.Movement.RunShortestYawContractTest`:
`RInterpConstantTo(350° → 10°, dt=1/60, speed=360)` applies a **negative** ~6° step (long path toward 10° the long way), not +6°.

## Implementation

`UGP_MovementComponent::ComputeShortestYawStep(CurrentYaw, TargetYaw, MaxAbsDeltaDegrees)`

- `Delta = FMath::FindDeltaAngleDegrees(Current, Target)` → `[-180, +180]`
- if `|Delta| <= MaxAbsDelta` → snap to `NormalizeAxis(Target)`
- else apply `Clamp(Delta, -Max, +Max)` and `NormalizeAxis(Current + Applied)`

Tick facing uses this helper with `MaxAbsDelta = RotationSpeed * DeltaTime`. Existing movement Tick only. No new Tick.

## First-Move sideways leg (second operator FAIL)

Shortest yaw is correct. Residual: first Move after PIE drove ~90 cm to a nav rim, then toward the click.

Confirmed: mobile BP primitives can still affect Recast generation (`APawn` actor flag is not enough). Authored meshes carve a static hole at the placed location. After the unit leaves that hole, DistActualProjected → 0.

Fix: `AGP_MobileUnit::ApplyMobileNavigationGenerationPolicy` + `UpdateNavigationRelevance` force all primitives off nav generation (including Blueprint/SCS). Buildings unchanged.

`StripProjectedStartAnchor` **removed** — it skipped legitimate off-nav entry once the hole is gone. `AcceptanceRadius` already advances past a coincident Recast start.

**Rebuild NavMesh in editor** after this C++ change so old baked holes disappear. Maps are not modified in this slice.

## Recorded out of scope

`LogGPBuildGridRegister` `GP BuildingDefinitionLoadFailed` for `BP_GP_MainBase` / `DA_GP_Buildings_MainBase` (`ResolveFailedUsingFallback`). Separate DataAsset/load issue. **Not fixed here.**

## Shortest-yaw invariant

Rotation always follows the shortest signed yaw delta. Examples: 350→10 is +20; 10→350 is −20; +179→−179 is +2.

## Validation (candidate)

| Check | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `gp.Movement.RunShortestYawContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | `Complete Failures=0 Cancelled=false` |
| `GP` Win64 Development / Shipping | **NOT RUN** |

## Out of scope

Attack facing in UnitCommand, retaliation, pathfinding, acceleration, speed tuning, operator assets.
