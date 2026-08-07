# DEFERRED VALIDATION — GP-S28P3 Operator Scenario C

## Status
**DEFERRED** — not failed; not a P3 production blocker.

## Depends on
**future canonical navigation/path-following movement stage**
(no specific roadmap stage number assigned yet)

## Why deferred
Current `UGP_MovementComponent` accepts a destination after UnitCommand pre-query, then tick-moves the actor. Runtime physical walls / BlockingVolume / NavModifierVolume do **not** correctly produce haul MoveFailed / path invalidation. Manual “block the path” PIE is therefore invalid for P3 Scenario C today.

Automated unreachable coverage in `gp.Resource.RunDropOffResilienceContractTest` remains PASS and must not be removed.

## Re-run when
Canonical NavMesh/path-following movement with runtime route/path invalidation and approach pathfinding is implemented.

## Future operator checklist
1. Worker has Cargo > 0.
2. MainBase is registered and exists.
3. Real navigation route to MainBase is absent.
4. Haul enters `WaitingForDropOff` with Cargo intact.
5. No retry/jitter spam.
6. Navigation connectivity/path is then really restored.
7. Without MainBase unregister/register, safety retry discovers the new path.
8. Worker automatically leaves `WaitingForDropOff`.
9. Worker delivers Cargo.
10. Threat changes only after Accepted storage transaction.

## Related
- Task: [`Claude_Tasks/GP-S28P3_DropOff_Resilience.md`](Claude_Tasks/GP-S28P3_DropOff_Resilience.md)
- Branch tip at note: `feature/gp-s28p3-dropoff-resilience` / helper `422bc70454bf51a9cdd31dc2ab4f490f20f018a0`
- Abandoned proposals: `gp.Resource.MakeTestMainBaseUnreachable`, `gp.Resource.MakeTestMainBaseReachable` (not implemented)
