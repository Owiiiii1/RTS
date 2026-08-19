# Cursor Work Report — Building Procurement + Payload Ownership

## Status

**BUILDING_PROCUREMENT_PAYLOAD_OWNERSHIP_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED.**
**NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-building-procurement-payload-ownership`
- Base: `origin/main` @ `d2c1abcfcf4fe2f61ae00793294c0cc31919cd65`
- Head: this commit on the same branch (previous docs-only blocker was `50d65239630e48daa8a71553a38536124dd894df`)

## Repeated authored asset safety-gate result

Inspected the configured assets using Unreal’s Python commandlet without modifying assets.

### Logistics Hub — PASS

- Drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_LogisticsHUB`
- Cost: `100`
- BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB`
- Authored `SpawnedClass` path/class: `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_LogisticsHUB.BP_GP_LogisticsHUB_C`
- Slot validation: valid `AGP_LogisticsHub` subclass

### Defensive Turret — PASS

- Drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_DefensiveTurret`
- Cost: `150`
- BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_DefensiveTurret`
- Authored `SpawnedClass` path/class: `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_DefensiveTurret.BP_GP_DefensiveTurret_C`
- Slot validation: valid `AGP_DefensiveTurret` subclass

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

Removed the method and every call site (catalog `Get()`, native catalog construction, and native Hub `GetPurchaseCost`). Native Hub cost is no longer mutated from Project Settings.

## Canonical cost ownership

Canonical cost is `UGP_OrbitalDropDefinition::Cost`.

Native bootstrap values:

- Logistics Hub = 100
- Defensive Turret = 150
- Wall = 25
- Wall Turret = 75

Authored Hub/Turret Cost comes only from authored `UGP_OrbitalDropDefinition`.

## Canonical payload ownership

Canonical payload is `UGP_OrbitalDropDefinition` → `UGP_BuildingDefinition::SpawnedClass`.

Native bootstrap BuildingDefinitions now own payload classes explicitly:

- Hub: `AGP_LogisticsHub::StaticClass()`
- Turret: `AGP_DefensiveTurret::StaticClass()`

`UGP_BuildingDropCatalog::ResolvePayloadClass` no longer reads Project Settings payload classes.

## Slot validation rules

- Logistics Hub: valid `SpawnedClass` must derive from `AGP_LogisticsHub`
- Defensive Turret: valid `SpawnedClass` must derive from `AGP_DefensiveTurret`
- Arbitrary `AGP_BuildingBase` is not silently accepted for those two known products
- Wall / WallTurret / Wall Package behavior was preserved

## SpawnedClass async-readiness analysis / result

`UGP_BuildingDefinition::SpawnedClass` is `TSoftClassPtr<AGP_BuildingBase>` and can be unresolved while the drop + BuildingDefinition are already loaded. Hub/Turret Ready now requires:

1. top-level drop loaded
2. BuildingDefinition loaded
3. required `SpawnedClass` non-null
4. `SpawnedClass` resolved
5. `SpawnedClass` valid for slot

Async loading only. No `LoadSynchronous` was added to `BuildingDropCatalog`. Native Hub/Turret `StaticClass()` values are already loaded, so native bootstrap remains immediately Ready.

## Pending / Failed semantics

- Cold unresolved Hub/Turret `SpawnedClass` remains Pending → purchase `DefinitionNotReady`; no native substitution while the configured authored product is Pending
- Invalid or missing slot `SpawnedClass` transitions to Failed, logs a diagnostic, and uses the existing native fallback
- Never remains stuck Pending

## BuildingAuthority / DropPod flow

`GPBuildingDropAuthority` still obtains payload only via `UGP_BuildingDropCatalog::ResolvePayloadClass()`. DropPod receives the already-resolved payload class. Purchase, READY inventory, placement, grid reservation, snapping, deploy radius, altitude, cleanup, and delivery timing were not changed.

## Vitals / footprint unchanged

No changes to `BuildingDefinition.UnitDefinition`, `UnitDefinitionAsset`, MaxHealth fields, GAS initialization, `FootprintCells`, `PlacementFootprintBounds`, BuildGrid fallback tables, replicated footprint, `NavigationObstacle`, or placement offsets.

## Stale DefaultGame.ini keys

`GP/Config/DefaultGame.ini` was not modified. Stale keys `BuildingOrbitalPurchaseCost`, `BuildingPayloadClass`, and `DefensiveTurretPayloadClass` remain as inert leftover text. No GConfig reader, string compatibility lookup, or migration shim was added.

## Exact production files changed

- `GP/Source/GPRuntime/Public/Settings/GPOrbitalDeliverySettings.h`
- `GP/Source/GPRuntime/Private/Settings/GPOrbitalDeliverySettings.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropCatalog.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropCatalog.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPDropPod.h` (debug getter only)

## Exact tests / results

All Failures=0, Cancelled=false:

- `gp.Settings.RunOrbitalDeliveryVisibilityContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest`
- `gp.Building.RunMultiBuildingDataContractTest`
- `gp.Building.RunDefensiveTurretContractTest`
- `gp.Building.RunBuildGridContractTest`
- `gp.Resource.RunUnitCapLogisticsHubContractTest`
- `gp.Economy.RunEconomyLogisticsDataContractTest`

The building-drop contract now covers native Hub/Turret cost and payload, authored cost/payload wins, absent settings mutation/override, Pending=`DefinitionNotReady`, invalid/missing Failed+native fallback, cold unresolved `SpawnedClass` Pending, and DropPod receiving the catalog-resolved payload.

## Full-suite escalation decision

**Not escalated.** `Docs/Development/Risk_Based_Development_Workflow.md` reserves the full suite for cross-cutting architecture, shared authority/state infrastructure, replication/GAS, major refactors, milestone/RC, or unexpected targeted-regression breakage. This is a bounded ownership-cleanup slice. Directly affected Hub/Turret SpawnedClass cold-load/failure paths are covered by the building-drop contract.

## GPEditor / UHT result

`GPEditor Win64 Development` + UHT **PASS**. GP Development / Shipping were not run; those wait for operator PASS / finalization.

## Protected local DataAsset confirmation

Authored Hub/Turret DataAssets and Blueprints remain local/untracked. They were inspected only. They were not staged, committed, reverted, stashed, reset, restored, or cleaned. `GP/Config/DefaultGame.ini`, `GP/Config/DefaultEngine.ini`, `L_PrototypeArena.umap`, and other protected local content remain untouched by this commit.

## Stop

**NOT MERGED.**
**NOT FINALIZED.**
Operator PIE validation is next.
