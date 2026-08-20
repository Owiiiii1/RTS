# GP — Building Vitals Definition Ownership

Status: **BUILDING_VITALS_DEFINITION_OWNERSHIP_READY_FOR_OPERATOR_VALIDATION**

Branch: `feature/gp-building-vitals-definition-ownership`  
Base: `origin/main` @ `71a7c700a1f4b066d30c0490365099c82ce91a41`

## Goal

Make `UGP_BuildingDefinition::UnitDefinition` the canonical bridge into the existing
`AGP_UnitBase::UnitDefinitionAsset` async initialization and GAS attribute path for buildings.

## Factual baseline

Before this slice, `AGP_BuildingBase::BeginPlay()` called `AGP_UnitBase::BeginPlay()` first.
Unit initialization could therefore complete from an actor/BP `UnitDefinitionAsset` or `Default*`
before the BuildingDefinition pipeline ran. `CompleteBuildingDefinitionInitialization()` ignored
its resolved definition for vitals.

`ResolveCanonicalMaxHealth()` had no production caller. `BuildingDefinition.MaxHealth` was catalog
bootstrap/compatibility metadata and test coverage, not a live GAS initialization source.

## Authored asset safety gate

Read-only Unreal inspection:

- MainBase BuildingDefinition:
  `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_MainBase`
  - UnitDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_MainBase`
  - MaxHealth/InitialHealth 1000/1000, Damage 0, Armor 0, Resistance 0, Cooldown 1, Range 0
- Logistics Hub BuildingDefinition:
  `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_LogisticsHUB`
  - UnitDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_LogisticsHUB`
  - MaxHealth/InitialHealth 500/500, Damage 0, Armor 0, Resistance 0, Cooldown 1, Range 0
- Defensive Turret BuildingDefinition:
  `/Game/GrimProtocol/DataAssets/Buildings/DA_Buildings/DA_GP_Buildings_DefensiveTurret`
  - UnitDefinition: `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_DefensiveTurret`
  - MaxHealth/InitialHealth 400/400, Damage 20, Armor 0, Resistance 0, Cooldown 1, Range 900

All three references resolved. Their Blueprint CDOs also point at the matching BuildingDefinition
and UnitDefinition. Native MainBase/Hub/Turret BuildingDefinitions already link native UnitDefinitions.
No authored asset migration was required.

## Implemented contract

Canonical chain:

`UGP_BuildingDefinition`
→ `UnitDefinition`
→ `AGP_UnitBase::UnitDefinitionAsset`
→ existing async UnitDefinition initialization
→ GAS attributes.

For a non-empty BuildingDefinition:

1. `AGP_UnitBase::BeginPlay()` initializes ASC actor info but asks a virtual lifecycle gate before
   starting UnitDefinition initialization.
2. `AGP_BuildingBase` defers while its non-empty BuildingDefinition is unresolved.
3. BuildingDefinition completion copies a valid nested UnitDefinition over any actor/BP
   `UnitDefinitionAsset`.
4. The existing UnitDefinition async pipeline runs once and initializes GAS/tuning.
5. Existing `NotifyBuildingDefinitionReady()` consumers run afterward.

Orbital DropPod assigns the catalog BuildingDefinition during deferred spawn before `FinishSpawning`.
It does not initialize GAS. BuildingDropAuthority does not read UnitDefinition.

## Compatibility and failure policy

- Empty BuildingDefinition UnitDefinition: retain an explicitly authored actor `UnitDefinitionAsset`;
  otherwise use actor `Default*`; emit a diagnostic.
- Unresolved nested UnitDefinition: use the existing async load; remain pending and leave GAS
  attributes uninitialized.
- Nested load failure: existing UnitBase error/fallback policy initializes actor `Default*`; never hangs.
- Empty BuildingDefinitionAsset: preserve existing UnitBase behavior.
- `BuildingDefinition.MaxHealth`: retained as definition-level bootstrap/compatibility fallback only.
- Non-building units: lifecycle gate defaults false, so their behavior is unchanged.

## Preserved behavior

- Logistics Hub unit-cap bonus remains one-shot guarded.
- MainBase storage and registration remain on existing readiness/team paths.
- Building tags and payload `SpawnedClass` are unchanged.
- Damage, death, retaliation, health-bar, replication, and effects architecture are unchanged.
- Footprint, placement bounds, BuildGrid, grid reservation, navigation, snapping, collision, and
  placement offsets are strictly unchanged.

## Validation

Failures=0:

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

The broader `gp.Resource.RunS28RegressionSuite` was attempted because shared UnitBase lifecycle code
changed. It stopped in the hauling child with 15 failures after a hostile authored
`BP_GP_SalvageWalkerLONGRAGE` in the locally modified prototype map attacked and killed the test
worker. Isolated hauling reproduced the same map contamination. This is not evidence of a vitals
ownership regression; all directly affected and independently runnable shared-path contracts passed.

Build: GPEditor Win64 Development + UHT **PASS**.

## Operator test

1. Cold-open Editor and enter PIE.
2. MainBase: verify normal health/max health with no reset/pop.
3. Deploy Logistics Hub: correct BP, normal 500/500 health, +5 cap remains.
4. Deploy Defensive Turret: correct BP, normal 400/400 health, combat/cooldown/range unchanged.
5. Damage Hub and Turret; verify health bar and death behavior.
6. Verify placement/footprint behavior is unchanged.
7. Optional console evidence: review `GP UnitCombatAttributesInitialized` log lines for resolved values.

No asset edits are required.

**NOT MERGED. NOT FINALIZED.**
