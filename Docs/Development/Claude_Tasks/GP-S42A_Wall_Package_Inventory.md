# GP-S42A — Wall Package Data + MainBase Wall Inventory

## Status
**GP-S42A_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

**NOT FINALIZED.**

**Code Allowed: YES**

## Slice Group
Wall Package (GP-0305R canon) — first implementation half

## Branch
`feature/gp-s42a-wall-package-inventory`  
Base: `origin/main` @ `c00e95ed46fb4aa738a1747576ee2d6b84ffe593`

## Goal
Buy Wall Package → spend OrbitalFerronite once → one DropPod to owning MainBase → inventory becomes 5. No `AGP_Wall` placement.

## Implementation
- `UGP_WallPackageDefinition` (`PrimaryAssetType` `GPWallPackageDefinition`). Native bootstrap Cost **150** (catalog placeholder, not final balance), SegmentCount **5**, `GP.Drop.Type.WallPackage`.
- `UGP_WallPackageCatalog`: authored `UGP_OrbitalDeliverySettings::WallPackageDefinition` wins; async soft-load (GP-S39E); native fallback; pending rejects without spend.
- `UGP_WallSegmentInventoryComponent` on `AGP_MainBase` only: replicated stock 0..5 + pending; `CanPurchase` / `CanBuildWall`; authority begin/complete/cancel/clear; unused `AuthorityTryConsumeSegments`.
- `AGP_DropPod` payload kind `WallPackage` (not Building). Lands at `WallPackageDropZone`. No grid reservation. No `AGP_Wall` spawn.
- Purchase: `AGP_PlayerController::RequestWallPackagePurchase` → `GPWallPackageAuthority::AuthorityPurchaseWallPackage`. Validate → pending → spend once → spawn. Spawn fail refunds via `UGP_GE_AddOrbital`.
- TEMP HUD: Buy Wall Package + stock/pending + Build Wall availability text only (click logs deferred to GP-S42C). Event-driven.

## Deferred
`AGP_Wall`, WallConnection, drag placement, WallTurret.

## Validation (candidate)

| Check | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `gp.Orbital.RunWallPackageInventoryContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `GP Win64 Development` / Shipping | **not run** (after operator PASS) |

## Operator test (no Wall assets required)
1. PIE with MainBase and enough OrbitalFerronite.
2. Buy Wall Package → one pod to MainBase.
3. In flight: cannot buy again (pending).
4. Arrival: stock 5; depot event; cannot buy second package.
5. Build Wall shows available; no drag.
