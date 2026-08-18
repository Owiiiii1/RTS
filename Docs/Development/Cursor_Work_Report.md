# Cursor Work Report — GP-S42A operator-feedback + shutdown-crash correction

## Status
**GP-S42A_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

**NOT FINALIZED.**

## Branch / base / head
- Branch: `feature/gp-s42a-wall-package-inventory` (same branch, no new branch)
- Base: `origin/main` @ `c00e95ed46fb4aa738a1747576ee2d6b84ffe593`
- Head: (this commit)

## Shutdown crash root cause
`UGP_WallPackageInventoryContractTestRunner::BeginDestroy()` called `UGP_WallPackageCatalog::Get()`, which can `NewObject` a transient `GP_WallPackageCatalog` during UObject/engine teardown. Fatal: `Object is not packaged: GP_WallPackageCatalog`.

## Lifecycle correction
- `TryGetExisting()`: live pointer or nullptr; never creates; never refreshes.
- `Get()`: creates only when not shutting down. During/after engine pre-exit / `NotifyEngineShutdown` returns existing or nullptr.
- `ShutdownCatalog()` is idempotent. Engine lock prevents resurrection after pre-exit / module shutdown. Test-only reset still allows a later normal `Get()`.
- `BeginDestroy` / `Finish` use `TryGetExisting` only.
- Async `HandleLoaded` ignores callbacks when shutdown/exit is requested.
- `DebugEndContractIsolation` does not refresh bindings while creation is blocked.

## Editor close smoke
**GUI editor close NOT AUTOMATED.** UnrealEditor-Cmd received `quit` without a recorded `LogExit` (session stayed up). Operator must close the Editor normally and confirm no unpackaged-catalog fatal.

## UnitDropZone reuse
`GPWallPackageAuthority` lands on `AGP_MainBase::GetUnitDropZone()`. Authored UnitDropZone transform unchanged. Building READY placement unchanged.

## WallPackageDropZone removed
Native component, getter, and MainBase validation removed.

## Top-up / full-price / excess-wasted
- `CanPurchase` / begin: stock `< 5` and `!pending`.
- Cost always `PackageDefinition.Cost`.
- Arrival: `Accepted = min(SegmentCount, Capacity - current stock)` using stock at arrival. Success even if Accepted == 0. Never above 5. No refund.

## Tests / builds
| Check | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `gp.Orbital.RunWallPackageInventoryContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `GP Win64 Development` / Shipping | **not run** |

## Protected files untouched
Maps, DefaultGame.ini / DefaultEngine.ini, authored BPs/DAs, VFX, Tools/ — not committed.

## NOT MERGED
## NOT FINALIZED
