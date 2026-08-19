# Cursor Work Report — Building Procurement + Payload Ownership

## Status

**BUILDING_PROCUREMENT_PAYLOAD_OWNERSHIP_BLOCKED_BY_AUTHORED_ASSET_MIGRATION**

**NOT MERGED.**

## Branch / base / head

- Branch: `feature/gp-building-procurement-payload-ownership`
- Base: `origin/main` @ `d2c1abcfcf4fe2f61ae00793294c0cc31919cd65`
- Head: (this commit)

## Pre-change reference classification

- Settings UPROPERTYs: `BuildingOrbitalPurchaseCost`, `BuildingPayloadClass`, `DefensiveTurretPayloadClass`
- Settings helpers: `ResolveBuildingPayloadClass`, `ResolveDefensiveTurretPayloadClass`, `IsBuildingPayloadClassConfigInvalid`, `IsDefensiveTurretPayloadClassConfigInvalid`
- Native catalog construction: Hub cost 100; Turret cost 150; native Turret `SpawnedClass` is set; native Hub `SpawnedClass` is currently empty
- Legacy cost mutation: `SyncLegacyLogisticsHubCompatibility` copies `BuildingOrbitalPurchaseCost` onto the native Hub drop and is called during catalog creation/access and native Hub cost reads
- Payload precedence: Turret settings override → `BuildingDefinition.SpawnedClass` → native Turret; Hub `BuildingDefinition.SpawnedClass` → settings fallback → native Hub
- Building authority: obtains cost and payload only through `UGP_BuildingDropCatalog`
- DropPod: receives and stores the already-resolved payload class; no settings reader
- TEMP HUD / player controller: reads catalog `GetPurchaseCost` / `ResolvePayloadClass`
- Contracts: visibility, building-drop, economy, UnitCap, BuildGrid, MultiBuilding, and Turret contain bridge assertions or isolation
- Config text: `DefaultGame.ini` contains all three stale keys and configured BP classes
- Docs: audit and prior task history describe the active compatibility bridges
- Manual GConfig/string reader: none in `GP/Source`
- Other production readers: none outside the settings → catalog → authority/HUD chain

## Authored asset migration safety gate

Inspected the configured assets using Unreal’s Python commandlet without modifying assets.

### Logistics Hub — BLOCKING

- Drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_LogisticsHUB`
- Cost: `100`
- BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB`
- Current `SpawnedClass`: **None**
- Legacy bridge dependency: `BuildingPayloadClass=/Game/GrimProtocol/Blueprint/Buildings/BP_GP_LogisticsHUB.BP_GP_LogisticsHUB_C`

Required operator edit:

1. Open `DA_GP_Buildings_LogisticsHUB`
2. Set `SpawnedClass` to `BP_GP_LogisticsHUB_C`
3. Save the asset
4. Rerun this same cleanup slice

### Defensive Turret — READY

- Drop: `/Game/GrimProtocol/DataAssets/Game/DA_GP_OrbitalDrop_DefensiveTurret`
- Cost: `150`
- BuildingDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_DefensiveTurret`
- Current `SpawnedClass`: `/Game/GrimProtocol/Blueprint/Buildings/BP_GP_DefensiveTurret.BP_GP_DefensiveTurret_C`
- Slot validation: valid `AGP_DefensiveTurret` subclass

## Runtime change result

Implementation stopped at the required safety gate.

- No settings fields removed
- No settings helper APIs removed
- `SyncLegacyLogisticsHubCompatibility` remains
- No cost ownership change
- No payload ownership change
- No async-readiness change
- No authority, DropPod, vitals, footprint, grid, Wall Package, config, or content change

## Validation

- Authored asset inspection: completed
- Unreal contracts: not run because implementation was blocked before C++ changes
- Full suite: not run; no runtime blast radius
- GPEditor/UHT: not run; docs-only blocked result

## Protected-files confirmation

- `GP/Config/DefaultGame.ini` untouched
- `GP/Config/DefaultEngine.ini` untouched
- maps untouched
- Blueprints untouched
- DataAssets untouched
- materials/content untouched
- existing protected local changes remain unstaged

## NOT MERGED
