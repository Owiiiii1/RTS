# Cursor Work Report — Building Procurement + Payload Ownership

## Status

**BUILDING_PROCUREMENT_PAYLOAD_OWNERSHIP_FINALIZED_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `feature/gp-building-procurement-payload-ownership`
- Base: `origin/main` @ `d2c1abcfcf4fe2f61ae00793294c0cc31919cd65`
- Head: this finalization commit (implementation `12736422435b578ec9adda713d3208ec41c90dad`)

## Operator PASS summary

Cold/open Editor + PIE confirmed:

- Logistics Hub price unchanged at 100
- authored `BP_GP_LogisticsHUB` deploys correctly
- Hub placement / footprint unchanged
- Hub still grants +5 unit cap
- Defensive Turret price unchanged at 150
- authored `BP_GP_DefensiveTurret` deploys correctly
- turret combat still works
- Turret placement / footprint unchanged

## Authored asset safety-gate summary

Read-only Unreal inspection of configured authored chains:

- Hub drop `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_LogisticsHUB` → BuildingDefinition `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB` → `SpawnedClass` `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_LogisticsHUB.BP_GP_LogisticsHUB_C` (`BP_GP_LogisticsHUB_C`, valid `AGP_LogisticsHub` subclass)
- Turret drop `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_DefensiveTurret` → BuildingDefinition `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_DefensiveTurret` → `SpawnedClass` `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_DefensiveTurret.BP_GP_DefensiveTurret_C` (`BP_GP_DefensiveTurret_C`, valid `AGP_DefensiveTurret` subclass)

## Exact removed settings fields

- `BuildingOrbitalPurchaseCost`
- `BuildingPayloadClass`
- `DefensiveTurretPayloadClass`

## Exact removed helper APIs

- `ResolveBuildingPayloadClass(...)`
- `ResolveDefensiveTurretPayloadClass(...)`
- `IsBuildingPayloadClassConfigInvalid()`
- `IsDefensiveTurretPayloadClassConfigInvalid()`

`TryLoadSoftSubclass` remains only for `UnitDropPodClass`.

## `SyncLegacyLogisticsHubCompatibility` removal

Fully removed, including catalog create/access and native Hub cost-read call sites. No remaining production references.

## Canonical cost ownership

Canonical cost is authored `UGP_OrbitalDropDefinition::Cost`.

Native bootstrap:

- Logistics Hub = 100
- Defensive Turret = 150
- Wall = 25
- Wall Turret = 75

No Project Settings mutation path remains.

## Canonical payload ownership

`UGP_OrbitalDropDefinition` → `UGP_BuildingDefinition` → `SpawnedClass`.

Native ownership:

- Hub BuildingDefinition `SpawnedClass` = `AGP_LogisticsHub`
- Turret BuildingDefinition `SpawnedClass` = `AGP_DefensiveTurret`

Authored configured confirmation:

- Hub: `BP_GP_LogisticsHUB_C`, valid `AGP_LogisticsHub` subclass
- Turret: `BP_GP_DefensiveTurret_C`, valid `AGP_DefensiveTurret` subclass

## Slot validation rules

- Hub rejects arbitrary non-`AGP_LogisticsHub` subclasses
- Turret rejects arbitrary non-`AGP_DefensiveTurret` subclasses

## SpawnedClass async-readiness behavior

`SpawnedClass` is a soft class. Configured authored Hub/Turret cannot become Ready until `SpawnedClass` is loaded, resolved, and valid for the slot. Async loading only. No `LoadSynchronous` in `BuildingDropCatalog`.

## Pending / Failed semantics

- Unresolved class stays Pending
- Purchase remains `DefinitionNotReady`
- No native substitution while Pending
- Invalid/missing class transitions to Failed and uses native fallback
- Never stuck Pending

## Authority / DropPod flow

`GPBuildingDropAuthority` still obtains payload from `UGP_BuildingDropCatalog::ResolvePayloadClass()`. DropPod receives the already-resolved payload. No direct Project Settings payload read remains.

## Vitals unchanged

No changes to `BuildingDefinition.UnitDefinition`, `UnitDefinitionAsset`, `BuildingDefinition.MaxHealth`, `UnitDefinition.MaxHealth`, `DefaultMaxHealth`, or GAS initialization.

## Footprint unchanged

No changes to `FootprintCells`, `PlacementFootprintBounds`, BuildGrid fallback ownership, replicated footprint ownership, `NavigationObstacle`, or placement offsets.

Delivery timing, `UnitDropPodClass`, `PodTransportSlotCapacity`, building altitude, cleanup delay, deploy radius, and Wall Package ownership are unchanged.

## Stale INI keys untouched

`GP/Config/DefaultGame.ini` and `GP/Config/DefaultEngine.ini` were not modified. Stale keys `BuildingOrbitalPurchaseCost`, `BuildingPayloadClass`, and `DefensiveTurretPayloadClass` remain inert leftover text. No GConfig reader, string compatibility lookup, or migration shim.

Committed diff vs `origin/main` is source/tests/docs only. No maps, Blueprint, DataAsset, material, or other Content files.

## Final tests / results

All Failures=0, Cancelled=false:

- `gp.Settings.RunOrbitalDeliveryVisibilityContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest` (includes SpawnedClass cold-load/failure coverage; no distinct extra command)
- `gp.Building.RunMultiBuildingDataContractTest`
- `gp.Building.RunDefensiveTurretContractTest`
- `gp.Building.RunBuildGridContractTest`
- `gp.Resource.RunUnitCapLogisticsHubContractTest`
- `gp.Economy.RunEconomyLogisticsDataContractTest`

## Full-suite escalation decision

**Not escalated.** `Docs/Development/Risk_Based_Development_Workflow.md` reserves the full suite for cross-cutting architecture, shared authority/state infrastructure, replication/GAS, major refactors, milestone/RC, or unexpected targeted-regression breakage. Finalization reran the slice contract plus high-risk affected regressions. No new regression appeared.

## Builds

- GPEditor Win64 Development + UHT **PASS**
- GP Win64 Development **PASS**
- GP Win64 Shipping **PASS**

## Protected-files confirmation

- `GP/Config/DefaultGame.ini` untouched by this slice
- `GP/Config/DefaultEngine.ini` untouched by this slice
- Maps / Blueprints / materials / untracked Content left local

## Local authored Logistics Hub DataAsset confirmation

The locally edited Logistics Hub DataAsset remains local and was not staged, committed, reverted, stashed, reset, or restored.

## Stop

**NOT MERGED.**
