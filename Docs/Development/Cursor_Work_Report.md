# Cursor Work Report — Building Vitals Definition Ownership

## Status

**BUILDING_VITALS_DEFINITION_OWNERSHIP_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-building-vitals-definition-ownership`
- Base: `origin/main` @ `71a7c700a1f4b066d30c0490365099c82ce91a41`
- Head: this implementation/report commit

## Pre-change ownership and initialization order

`AGP_BuildingBase::BeginPlay()` called `AGP_UnitBase::BeginPlay()` first. UnitBase initialized ASC
actor info and immediately ran its UnitDefinition pipeline. Because no production path copied
`BuildingDefinition.UnitDefinition` into actor `UnitDefinitionAsset`, buildings could permanently
initialize GAS from a BP/CDO UnitDefinitionAsset or actor `Default*`. BuildingDefinition initialization
ran afterward and only notified storage/unit-cap consumers.

`ResolveCanonicalMaxHealth()` had no production caller. Health bars and combat read runtime GAS only.

## Authored asset safety gate

Read-only Unreal inspection passed:

- MainBase: BuildingDefinition
  `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_MainBase`;
  UnitDefinition `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_MainBase`;
  resolved; MaxHealth/InitialHealth 1000/1000, Damage 0, Armor 0, Resistance 0, Cooldown 1, Range 0
- Logistics Hub: BuildingDefinition
  `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB`;
  UnitDefinition `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_LogisticsHUB`;
  resolved; 500/500, Damage 0, Armor 0, Resistance 0, Cooldown 1, Range 0
- Defensive Turret: BuildingDefinition
  `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_DefensiveTurret`;
  UnitDefinition `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_DefensiveTurret`;
  resolved; 400/400, Damage 20, Armor 0, Resistance 0, Cooldown 1, Range 900

Matching Blueprint CDO BuildingDefinition/UnitDefinition refs also resolved. Native MainBase/Hub/Turret
BuildingDefinitions already link native UnitDefinitions. No authored migration required.

## Canonical chain and precedence

`UGP_BuildingDefinition`
→ `UnitDefinition`
→ `AGP_UnitBase::UnitDefinitionAsset`
→ existing async UnitDefinition pipeline
→ GAS.

For a valid BuildingDefinition, nested UnitDefinition now outranks:

- Blueprint/CDO or instance `UnitDefinitionAsset`
- `BuildingDefinition.MaxHealth`
- actor `DefaultMaxHealth`, `DefaultHealth`, `DefaultDamage`, `DefaultArmor`,
  `DefaultDamageResistance`, `DefaultAttackCooldown`, and `DefaultAttackRange`

`UGP_UnitDefinition` remains the source for vitals/combat, sight, movement, cargo, and retaliation
tuning supported by the existing UnitBase pipeline.

## Lifecycle implementation

- UnitBase initializes ASC actor info first.
- A virtual gate defaults false, preserving all non-building units.
- BuildingBase returns true while a non-empty BuildingDefinition is unresolved.
- On BuildingDefinition completion, a non-empty nested UnitDefinition overwrites actor
  `UnitDefinitionAsset`.
- BuildingBase then calls the existing UnitDefinition initializer.
- Existing one-shot guards (`bUnitDefinitionReady`, `bDefinitionTuningApplied`,
  `bCombatAttributesInitialized`) guarantee no temporary defaults and no second GAS initialization.
- `NotifyBuildingDefinitionReady()` runs afterward; MainBase storage and Hub unit-cap behavior remain
  on their existing one-shot/readiness paths.

## Compatibility, pending, and failure semantics

- Empty BuildingDefinition UnitDefinition: preserve explicit actor UnitDefinitionAsset; otherwise
  actor `Default*`; warning diagnostic.
- Unresolved BuildingDefinition: UnitDefinition/GAS initialization remains deferred.
- Unresolved nested UnitDefinition: existing async UnitDefinition load remains pending; GAS attributes
  remain uninitialized rather than using defaults early.
- Nested UnitDefinition load failure: existing UnitBase diagnostic and actor Default* fallback; no hang.
- Empty BuildingDefinitionAsset: existing actor UnitDefinitionAsset/default behavior.
- Non-building units: unchanged because their lifecycle gate returns false.

## BuildingDefinition.MaxHealth policy

Retained as explicit bootstrap/compatibility fallback for definition-level
`ResolveCanonicalMaxHealth()` queries. It is not a spawned-building GAS source when UnitDefinition is
valid. Removing it would require separate authored-data proof and offers no benefit in this slice.

## Spawn paths

- Orbital: BuildingDropAuthority still resolves only product/payload. DropPod assigns the catalog
  BuildingDefinition during deferred building spawn, before `FinishSpawning`; BuildingBase owns the
  UnitDefinition bridge and GAS sequencing.
- Pre-placed/MainBase: authored MainBase BP has BuildingDefinitionAsset configured, so the same bridge
  applies without orbital spawn.
- DropPod unit payload semantics are unchanged.

## Unchanged systems

- Runtime Health/MaxHealth/Damage/Armor/Resistance/Cooldown/Range remain GAS attributes.
- Damage application, death, retaliation, health bar, replication, effects architecture unchanged.
- Logistics Hub UnitCapBonus, MainBase storage/container capacity/count, building tags, and payload
  SpawnedClass remain on existing owners.
- FootprintCells, PlacementFootprintBounds, BuildGrid fallbacks/reservation, replicated footprint,
  NavigationObstacle, placement offsets, snapping, and collision explicitly unchanged.

## Production files changed

- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h` (comments/policy only)
- `GP/Source/GPRuntime/Private/Orbital/GPDropPod.cpp`

## Test files changed/added

- added `GP/Source/GPRuntime/Public/Buildings/GPBuildingVitalsOwnershipContractTest.h`
- added `GP/Source/GPRuntime/Private/Debug/GPBuildingVitalsOwnershipContractTest.cpp`
- updated `GP/Source/GPRuntime/Private/Debug/GPOrbitalBuildingDropContractTest.cpp`

## Tests

Failures=0, Cancelled=false:

- `gp.Building.RunBuildingVitalsOwnershipContractTest`
- `gp.Building.RunOrbitalBuildingDropContractTest`
- `gp.Building.RunMultiBuildingDataContractTest`
- `gp.Building.RunDefensiveTurretContractTest`
- `gp.Resource.RunUnitCapLogisticsHubContractTest`
- `gp.Units.RunUnitDefinitionContractTest`
- `gp.Economy.RunEconomyLogisticsDataContractTest`
- `gp.Combat.RunHealthBarContractTest`
- `gp.Combat.RunAutoAcquireContractTest`
- `gp.Combat.RunAttackMoveContractTest`
- `gp.Resource.RunOrbitalUnitDropContractTest`

## Regression/full-suite escalation decision

Escalated because shared `AGP_UnitBase::BeginPlay` received a virtual gate. Direct building,
UnitDefinition, combat, health-bar, economy, and non-building orbital-unit contracts all passed.

`gp.Resource.RunS28RegressionSuite` was additionally attempted. It stopped at Worker hauling with 15
failures after a hostile authored `BP_GP_SalvageWalkerLONGRAGE` in the locally modified prototype map
attacked and killed the hauling fixture. Running the hauling contract alone reproduced the same 15
failures and hostile-map interference. The result is environment/content contamination, not a
vitals-ownership failure; protected map/content was not changed to force the suite.

No further full-suite escalation was useful without modifying protected local content.

## Build

- GPEditor Win64 Development + UHT: **PASS**
- GP Development / Shipping: not run; reserved for finalization after operator PASS

## Protected files

Untouched and not staged:

- `GP/Config/DefaultGame.ini`
- `GP/Config/DefaultEngine.ini`
- maps, Blueprints, DataAssets, materials, and all untracked Content
- locally edited Logistics Hub BuildingDefinition/DataAsset content

The temporary read-only asset-inspection script was deleted after use.

## Operator validation required

Cold-open Editor → PIE. Verify MainBase has stable normal health; deploy Hub (500/500, +5 cap) and
Turret (400/400, combat/cooldown/range); damage both and verify health bar/death; verify no placement
or footprint regression. `GP UnitCombatAttributesInitialized` log lines expose initialized GAS values.

**NOT MERGED. NOT FINALIZED.**
