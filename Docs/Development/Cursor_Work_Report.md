# Cursor Work Report — GP-S42A nested BuildingDefinition load

## Status
**GP-S42A_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**

**NOT FINALIZED.**

## Branch / base / head
- Branch: `feature/gp-s42a-wall-package-inventory` (same branch, no new branch)
- Base: `origin/main` @ `c00e95ed46fb4aa738a1747576ee2d6b84ffe593`
- Head: (this commit)

## Observed failure
Operator purchase log:

```
GP BuildingPurchase Result:
Accepted=false
Reason=13
Cost=0.000
Ready=0
Drop=GPOrbitalDropDefinition:DA_GP_OrbitalDrop_LogisticsHUB
```

Reason=13 is `EGP_BuildingDropRejectReason::MissingBuildingDefinition`.

## Factual root cause
`AuthorityPurchaseBuilding` rejects `MissingBuildingDefinition` when `DropDefinition->ResolveLoadedBuildingDefinition()` is null. That resolve is already-loaded only (no async).

`UGP_BuildingDropCatalog` async-loaded only the top-level `UGP_OrbitalDropDefinition` settings soft ref and marked the authored slot Ready once that asset existed in memory. It did not load the nested `DropDefinition->BuildingDefinition` soft ref.

Cold Editor restart therefore depended on whether the nested building DA happened to already be in memory. That is the latent defect exposed during GP-S42A validation.

## Nested dependency loading design
An authored building drop is Ready only when:

1. top-level `UGP_OrbitalDropDefinition` is loaded
2. its `BuildingDefinition` soft ref is valid / non-null
3. referenced `UGP_BuildingDefinition` is loaded and resolved

Implementation:

- Per-slot top-level handle + nested BuildingDefinition handle (AssetManager StreamableManager).
- Top-level unloaded → async load → Pending.
- Top-level loaded, nested unloaded → async load nested → remains Pending.
- Purchase / deploy while Pending → `DefinitionNotReady`. No spend. No READY mutation.
- Both loaded → Ready. `CanonicalForSlot` returns authored only when the nested building is genuinely resolved.
- `IsDropDefinitionPending` / `IsDropDefinitionIdPending` stay true during nested load.
- No `LoadSynchronous` / `LoadObject`. `AuthorityPurchaseBuilding` is not patched with a sync load.
- Teardown cancels both handles. Nested/top callbacks ignore dead catalog / engine-exit.

Wall Package stays on `UGP_WallPackageDefinition` and is not routed through this path.

## Fallback behavior
S39E precedence preserved: authored configured → native bootstrap → deprecated fallback only where applicable.

- Null `BuildingDefinition` on an authored drop: explicit error log (`NullBuildingDefinitionUsingNativeFallback`). Slot Failed. Canonical is native. Not treated as Ready.
- Nested load failure: explicit error log with slot + Drop path + BuildingDefinition path. Slot Failed. Native fallback. Not stuck Pending. No crash / no spend.
- Configured-but-unresolved nested dependency is never Ready.

## Tests / builds
| Check | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Orbital.RunWallPackageInventoryContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunMultiBuildingDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `GP Win64 Development` / Shipping | **not run** |

## Protected files untouched
Maps, DefaultGame.ini / DefaultEngine.ini, authored BPs/DAs, VFX, Tools/ — not modified and not committed.

## NOT MERGED
## NOT FINALIZED
