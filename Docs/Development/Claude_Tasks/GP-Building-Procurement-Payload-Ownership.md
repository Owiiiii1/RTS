# GP — Building Procurement + Payload Ownership Cleanup

Status: **BUILDING_PROCUREMENT_PAYLOAD_OWNERSHIP_BLOCKED_BY_AUTHORED_ASSET_MIGRATION**

Branch: `feature/gp-building-procurement-payload-ownership` from `origin/main` @ `d2c1abcfcf4fe2f61ae00793294c0cc31919cd65`

Source audit: [`Configuration_Data_Ownership_Audit.md`](../Configuration_Data_Ownership_Audit.md)

## Safety-gate result

Unreal inspection of the configured authored chains:

- Logistics Hub drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_LogisticsHUB`
  - Cost: `100`
  - BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB`
  - `SpawnedClass`: **empty / invalid**
  - Current dependency: `BuildingPayloadClass=/Game/GrimProtocol/Blueprint/Buildings/BP_GP_LogisticsHUB.BP_GP_LogisticsHUB_C`
- Defensive Turret drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_DefensiveTurret`
  - Cost: `150`
  - BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_DefensiveTurret`
  - `SpawnedClass`: `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_DefensiveTurret.BP_GP_DefensiveTurret_C`
  - Valid `AGP_DefensiveTurret` subclass

## Required operator edit

Open `DA_GP_Buildings_LogisticsHUB`, set `SpawnedClass` to `BP_GP_LogisticsHUB_C`, and save the asset. Then rerun this same cleanup slice.

## Stop

No Project Settings fields, helper APIs, catalog compatibility code, tests, config, or authored assets were changed. No build or contracts were required for this docs-only blocked result.

**NOT MERGED.**
