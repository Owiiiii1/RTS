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
Implementation tip: `37a567db20b58457c6a92754393edf26839410a7`

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

## First-Move path start (operator FAIL correction)

Operator: first PIE/spawn Move still turned the long-looking way; later Moves were correct.

Proven: `FindPathSync` / projected-straight copies Recast `PathPoints[0] == ProjectedStart`. First Tick with `PathIndex=0` steered toward that query anchor. Later Moves project near the actor, so the defect disappeared.

Contract first-Move (arena): ActualStart XY `(80,-1400)` Z=`50`; ProjectedStart XY same Z=`10`; DistXY=`0`; raw Path0 == ProjectedStart; after strip Path0=`(1280,-1400)`; first tick `+X`, yaw `90→84` Applied=`-6`.

Fix: `StripProjectedStartAnchor` removes the first runtime point when it is the projected query start (tolerance `max(AcceptanceRadius, 25)`). Genuine first corners (not the query start) are kept. Destination projection, repath, serials, and straight-line fallback are unchanged.

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
