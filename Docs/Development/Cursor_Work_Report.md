# Cursor Work Report — Unit Payload Compatibility Cleanup

## Status

**UNIT_PAYLOAD_COMPAT_CLEANUP_FINALIZED_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `feature/gp-unit-payload-compat-cleanup`
- Base: `origin/main` @ `47a220b480e455f1cf5dfb6ca0613c13cf760a53`
- Head: (this commit)

## Operator PASS summary

- Worker authored BP/class arrives correctly
- Salvage Walker authored BP/class arrives correctly
- visuals/behavior unchanged
- prices/currency deduction unchanged
- transport behavior unchanged

## Removed settings fields

Completely absent from `UGP_OrbitalDeliverySettings`:

- `WorkerPayloadClass`
- `SalvageWalkerPayloadClass`

## Removed settings helper APIs

Completely absent from `UGP_OrbitalDeliverySettings`:

- `ResolveWorkerPayloadClass(...)`
- `ResolveSalvageWalkerPayloadClass(...)`
- `IsWorkerPayloadClassConfigInvalid()`
- `IsSalvageWalkerPayloadClassConfigInvalid()`

`TryLoadSoftSubclass` remains only for `UnitDropPodClass`, `BuildingPayloadClass`, and `DefensiveTurretPayloadClass`.

## Canonical authored payload ownership

`UGP_OrbitalUnitDropDefinition::PayloadClass` is canonical for configured products.

- Worker authored payload must be an `AGP_Worker` subclass
- Salvage Walker authored payload must be an `AGP_SalvageWalker` subclass

## Native Worker / Walker fallback ownership

`UGP_OrbitalUnitDropCatalog::EnsureNativeCatalog()` sets native product `PayloadClass`, and `ResolveFallbackPayloadClass` returns:

- Worker: `AGP_Worker::StaticClass()`
- Salvage Walker: `AGP_SalvageWalker::StaticClass()`

Not configurable through Project Settings.

## Pending / Failed readiness semantics

- Pending authored payload returns no native substitution
- purchase remains `DefinitionNotReady` while Pending
- null/invalid authored payload follows existing Failed → native fallback policy with diagnostics

## No unit payload LoadSynchronous path remains

Unit product payload resolution does not reach `UGP_OrbitalDeliverySettings` `TryLoadSoftSubclass`. Catalog payload readiness remains async. `GP/Source/.../Orbital` has no `LoadSynchronous`. No GConfig/string compatibility reader was added (`GP/Source` has no `GConfig`).

## DropPod source confirmation

`AGP_DropPod` resolves payload through `UGP_OrbitalUnitDropCatalog::ResolveWorkerPayloadClass` / `ResolveSalvageWalkerPayloadClass`. It does not read Project Settings.

## UnitDefinitionAsset semantics unchanged

When spawned actor `UnitDefinitionAsset` is empty, catalog product UnitDefinition may still be assigned. Explicit BP/CDO override remains unchanged.

## Stale DefaultGame.ini keys untouched

Committed `GP/Config/DefaultGame.ini` still contains:

```
WorkerPayloadClass=/Game/GrimProtocol/Blueprint/Units/BP_GP_Worker.BP_GP_Worker_C
SalvageWalkerPayloadClass=/Game/GrimProtocol/Blueprint/Units/BP_GP_SalvageWalker.BP_GP_SalvageWalker_C
```

Intentionally not edited. After C++ removal they cannot populate runtime fields.

## Final tests / results

| Check | Result |
| --- | --- |
| `gp.Settings.RunOrbitalDeliveryVisibilityContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunUnitCapLogisticsHubContractTest` | `Complete Failures=0 Cancelled=false` |

Full suite not run. Building / Wall Package contracts not run (shared production code outside unit payload ownership was not changed).

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `GP Win64 Development` | **PASS** |
| `GP Win64 Shipping` | **PASS** |

## Protected-files confirmation

Committed diff vs `origin/main` @ `47a220b…` is unit-payload C++ ownership, targeted contracts, and docs only:

- no numeric unit procurement ownership changes in this finalization
- no `UnitDropPodClass` changes
- no unit timing fallback field changes
- no `BuildingPayloadClass` / `DefensiveTurretPayloadClass` / `BuildingOrbitalPurchaseCost` / `BuildingDropCatalog` changes
- no Wall Package changes
- no maps
- no `DefaultGame.ini`
- no `DefaultEngine.ini`
- no Blueprint / DataAsset / material / content changes

No new functionality in this finalization.

## NOT MERGED
