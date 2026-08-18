# GP-S42A — Wall Package Data + MainBase Wall Inventory

## Status
**GP-S42A_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

**NOT FINALIZED.**

**Code Allowed: YES**

## Slice Group
Wall Package (GP-0305R canon) — first implementation half + operator-feedback correction

## Branch
`feature/gp-s42a-wall-package-inventory`  
Base: `origin/main` @ `c00e95ed46fb4aa738a1747576ee2d6b84ffe593`

## Goal
Buy Wall Package (stock 0..4, full price) → one DropPod to MainBase **UnitDropZone** → arrival fills to at most 5. No `AGP_Wall` placement.

## Operator-feedback corrections
- Shutdown: `BeginDestroy` uses `TryGetExisting()` only. `Get()` must not create during engine pre-exit / module shutdown.
- Landing: Wall Package uses `AGP_MainBase::UnitDropZone`. `WallPackageDropZone` removed.
- Top-up: purchase at stock 0..4 for full `PackageDefinition.Cost`. Arrival `Accepted = min(SegmentCount, free capacity)`. Excess wasted. No refund.

## Deferred
`AGP_Wall`, WallConnection, drag placement, WallTurret.

## Validation (candidate)

| Check | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `gp.Orbital.RunWallPackageInventoryContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| GUI editor close smoke | **NOT AUTOMATED** |

## Operator test
1. PIE with MainBase and enough OrbitalFerronite.
2. Buy Wall Package at stock 0 and at stock 1..4 — always full price, one pod to **UnitDropZone**.
3. In flight: cannot buy again.
4. Arrival fills to 5; excess wasted; cannot buy at 5.
5. Build Wall shows available; no drag.
6. Close editor: no `GP_WallPackageCatalog` unpackaged fatal.
