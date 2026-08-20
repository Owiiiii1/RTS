# GP — Building Procurement + Payload Ownership Cleanup

Status: **BUILDING_PROCUREMENT_PAYLOAD_OWNERSHIP_FINALIZED_READY_FOR_MERGE**

Branch: `feature/gp-building-procurement-payload-ownership` from `origin/main` @ `d2c1abcfcf4fe2f61ae00793294c0cc31919cd65`

Source audit: [`Configuration_Data_Ownership_Audit.md`](../Configuration_Data_Ownership_Audit.md)

## Repeated authored asset safety gate

Unreal inspection of the configured authored chains (read-only):

- Logistics Hub drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_LogisticsHUB`
  - Cost: `100`
  - BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB`
  - `SpawnedClass`: `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_LogisticsHUB.BP_GP_LogisticsHUB_C`
  - Valid `AGP_LogisticsHub` subclass
- Defensive Turret drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_DefensiveTurret`
  - Cost: `150`
  - BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_DefensiveTurret`
  - `SpawnedClass`: `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_DefensiveTurret.BP_GP_DefensiveTurret_C`
  - Valid `AGP_DefensiveTurret` subclass

Protected local DataAssets were inspected only. They were not modified, staged, or committed.

## Implementation

Removed from `UGP_OrbitalDeliverySettings`:

- `BuildingOrbitalPurchaseCost`
- `BuildingPayloadClass`
- `DefensiveTurretPayloadClass`

Removed helper APIs:

- `ResolveBuildingPayloadClass`
- `ResolveDefensiveTurretPayloadClass`
- `IsBuildingPayloadClassConfigInvalid`
- `IsDefensiveTurretPayloadClassConfigInvalid`

Removed `UGP_BuildingDropCatalog::SyncLegacyLogisticsHubCompatibility` and all call sites.

Canonical cost is `UGP_OrbitalDropDefinition::Cost`. Native bootstrap: Hub 100, Turret 150, Wall 25, Wall Turret 75.

Canonical payload is `UGP_OrbitalDropDefinition` → `UGP_BuildingDefinition::SpawnedClass`. Native Hub/Turret BuildingDefinitions own `AGP_LogisticsHub` / `AGP_DefensiveTurret`. Known Hub/Turret slots reject arbitrary `AGP_BuildingBase` subclasses.

`SpawnedClass` is a soft class. Hub/Turret Ready requires async-resolved slot-valid class. Pending stays `DefinitionNotReady`. Invalid/missing transitions to Failed and uses native fallback. No `LoadSynchronous` in the building catalog.

Authority still obtains payload only through `UGP_BuildingDropCatalog::ResolvePayloadClass()`. DropPod receives the already-resolved class.

Stale `DefaultGame.ini` keys were left inert. Operator PASS. **NOT MERGED.**
