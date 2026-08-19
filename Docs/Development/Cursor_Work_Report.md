# Cursor Work Report — Delivery Timing Ownership Cleanup

## Status

**DELIVERY_TIMING_OWNERSHIP_CLEANUP_FINALIZED_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `feature/gp-delivery-timing-ownership-cleanup`
- Base: `origin/main` @ `75b13fc193531170eb3d4c1eaf9ee3f736d1d160`
- Head: (this commit)

## Operator PASS summary

- Worker descent/deploy timing looks unchanged
- Salvage Walker descent/deploy timing looks unchanged
- Logistics Hub pod descent and payload deploy timing look unchanged
- Defensive Turret timing looks unchanged
- prices/currency deduction unchanged
- payload class unchanged
- placement unchanged
- READY/inventory flow unchanged

## Exact four removed settings fields

Completely absent from `UGP_OrbitalDeliverySettings`:

- `UnitDropDescentDurationSeconds`
- `UnitDropPayloadDeployDelaySeconds`
- `BuildingDropDescentDurationSeconds`
- `BuildingDropPayloadDeployDelaySeconds`

No replacement DeveloperSettings timing fields. No GConfig/string compatibility reader was added (`GP/Source` has no `GConfig`).

## Unit timing ownership

- Authored Ready: `UGP_OrbitalUnitDropDefinition::DeliveryDescentSeconds` and `PayloadDeployDelaySeconds` are canonical
- Native bootstrap owned by `UGP_OrbitalUnitDropCatalog` construction:
  - Worker: **2.5 / 1.25**
  - Salvage Walker: **2.5 / 1.25**
- Mixed manifest still uses max `DeliveryDescentSeconds` and max `PayloadDeployDelaySeconds`
- Configured authored Pending remains `DefinitionNotReady`
- Blocked Pending purchase does not use native timing substitution
- `ResolveManifestDeliveryTiming` and `GPUnitDropAuthority` no longer read Project Settings timing

## Building timing ownership

- Authored Ready: `UGP_OrbitalDropDefinition::DeliveryDescentSeconds` and `PayloadDeployDelaySeconds` are canonical
- Native building bootstrap: **2.5 / 2.0** on `UGP_BuildingDropCatalog` construction
- Configured authored Pending remains `DefinitionNotReady`
- `GPBuildingDropAuthority` no longer seeds timing from Project Settings; it uses `UGP_BuildingDropCatalog::ResolveDeliveryTiming`

## Native unit timing

Worker and Salvage Walker native products: **2.5 / 1.25**

## Native building timing

Native building products: **2.5 / 2.0**

## Authored precedence

Authored Ready product timing wins for both unit and building catalogs.

## Mixed manifest max semantics

Preserved: max descent and max deploy delay across products in the unit manifest.

## Pending behavior

Configured authored product Pending remains `DefinitionNotReady` (no spend / spawn).

## Wall Package unchanged

Wall Package timing remains independently owned by `UGP_WallPackageDefinition` / `UGP_WallPackageCatalog::ResolveDeliveryTiming`. It does not consume the four removed settings fields.

## Stale DefaultGame.ini keys untouched

Committed `GP/Config/DefaultGame.ini` still contains:

```
UnitDropDescentDurationSeconds=10.000000
UnitDropPayloadDeployDelaySeconds=5.000000
BuildingDropDescentDurationSeconds=10.000000
BuildingDropPayloadDeployDelaySeconds=5.000000
```

Intentionally not edited. After C++ removal they cannot populate runtime fields.

## Final tests / results

| Check | Result |
| --- | --- |
| `gp.Settings.RunOrbitalDeliveryVisibilityContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunUnitCapLogisticsHubContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunMultiBuildingDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunDefensiveTurretContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunBuildGridContractTest` | `Complete Failures=0 Cancelled=false` |

Full suite not run.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** (up to date) |
| `GP Win64 Development` | **PASS** |
| `GP Win64 Shipping` | **PASS** |

## Protected-files confirmation

Committed diff vs `origin/main` @ `75b13fc…` is delivery-timing C++ ownership, targeted contracts, and docs only:

- no unit numeric ownership changes
- no unit payload ownership changes
- no `UnitDropPodClass` / `BuildingPayloadClass` / `DefensiveTurretPayloadClass` / `BuildingOrbitalPurchaseCost` changes
- no `BuildingMaxDeployRadiusFromMainBaseCm` / altitude / spacing / cleanup delay changes
- no footprint/grid ownership changes
- no `UnitDefinitionAsset` semantics changes
- no Wall Package ownership changes
- no maps
- no `DefaultGame.ini`
- no `DefaultEngine.ini`
- no Blueprint / DataAsset / material / content changes

No new functionality in this finalization.

## NOT MERGED
