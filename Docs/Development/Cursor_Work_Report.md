# Cursor Work Report

Status: **GP-S39E_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch
`feature/gp-s39e-economy-logistics-data`

## Base main SHA
`f841cdee19c97a0dfaacb8fc0bdd27623c543329`

## Feature head SHA
`8d001180fe4e7f1d24edc25b471f2b6e00877138`

## Old ownership matrix (factual, audited on main)

| Tunable | Owner before S39E |
| --- | --- |
| Worker cargo 50 | `UGP_CargoComponent::CargoCapacity` |
| MainBase storage 100×5 | `UGP_StorageComponent` CDO (`ContainerCapacity=100`, `ContainerCount=5`) |
| Ready threshold | `CurrentAmount >= ContainerCapacity` (MVP == capacity; no `LaunchReadyThreshold`) |
| Ferronite mining / conversion / threat | already `UGP_ResourceDefinition` (`AmountPerMiningCycle=10`, `MiningCycleDurationSeconds=1`, `InteractionRangeCm=200`, `ScoreConversionRate=1`, `OrbitalConversionRate=1`, `ThreatPerStoredUnit=0.5`) |
| Deposit MaxAmount 5000 / CurrentAmount / MaxConcurrentMiners 4 | `AGP_ResourceNode` (`CurrentAmount` EditInstanceOnly) |
| Worker/Walker cost 25/50, slots 1/2, payload classes | `UGP_OrbitalDeliverySettings` |
| Pod slot capacity 4 | `UGP_OrbitalDeliverySettings::PodTransportSlotCapacity` |
| Unit descent 2.5 / deploy 1.25 / cleanup 0.35 / altitude 2500 / spacing 180 | `UGP_OrbitalDeliverySettings` |
| Building descent 2.5 / deploy 2.0 / cleanup 0.5 / altitude 2500 / radius 5000 / overlap 25 | `UGP_OrbitalDeliverySettings` |
| Building costs Hub 100 / Turret 150 / Wall 25 / WallTurret 75 | `UGP_OrbitalDropDefinition.Cost` (Hub may sync deprecated settings cost) |
| Container launch telegraph 2.5s | `UGP_ResourceGameplaySettings::ContainerLaunchDurationSeconds` |
| Initial MaxUnits +5 | `UGP_GE_UnitCap_Base5` on PlayerState (match/player) |
| Hub +5 | hardcoded `FScalableFloat(5)` on `UGP_GE_UnitCap_Plus5` |
| MainBase BuildingDefinition ref | **missing** (predeployed, not purchased) |
| Manifest counts | `FGP_UnitDropManifest` `WorkerCount` / `SalvageWalkerCount` |

## New ownership matrix

| Concern | Canonical | Runtime / fallback |
| --- | --- | --- |
| Worker cargo capacity | `UGP_UnitDefinition.CargoCapacity` (50) | `UGP_CargoComponent` runtime state; empty def keeps component 50 |
| Walker / buildings cargo | 0 | no cargo / unused |
| MainBase storage | `UGP_BuildingDefinition` 100×5 | component 100×5 if BuildingDef empty |
| Hub / turret / wall / wallturret storage | 0 / 0 | no Ferronite containers |
| Ready threshold | == `ContainerCapacity` | no partial launch; no `LaunchReadyThreshold` |
| Ferronite mining / conversion / threat | `UGP_ResourceDefinition` (unchanged) | — |
| Deposit max / concurrent miners | `UGP_ResourceDefinition.DepositMaxAmount=5000` / `MaxConcurrentMiners=4` | node fields if def unresolved; authored instance override only if live value ≠ native `AGP_ResourceNode` CDO |
| Node `CurrentAmount` | runtime + `EditInstanceOnly` | starts at MaxAmount unless instance override / later replication |
| Unit cost / slots / payload / descent / deploy | `UGP_OrbitalUnitDropDefinition` | settings deprecated fields as fallback |
| Building cost | `UGP_OrbitalDropDefinition.Cost` (unchanged) | Hub settings cost bridge retained |
| Building descent / deploy | `UGP_OrbitalDropDefinition` 2.5 / 2.0 | settings fallback |
| Pod capacity, pod class, altitude, spacing, cleanup, radius, overlap | `UGP_OrbitalDeliverySettings` | global |
| Container launch duration | `UGP_ResourceGameplaySettings` | global (not moved) |
| Initial MaxUnits | `UGP_GE_UnitCap_Base5` (+5) | match/player — **not** UnitDefinition |
| Hub UnitCapBonus | `UGP_BuildingDefinition.UnitCapBonus` (5) | Hub class fallback 5 if def empty; GE is SetByCaller |
| BuildingDefinition on actors | `AGP_BuildingBase::BuildingDefinitionAsset` soft | empty / loaded / async / failure — same as UnitDefinition |

No GameBalance singleton. No duplicated canonical values.

## CargoCapacity ownership / fallback

1. Loaded `UnitDefinition.CargoCapacity` configures `UGP_CargoComponent` (Worker 50).
2. Empty UnitDefinition preserves existing component field (legacy 50).
3. Component still owns runtime cargo state. Definition configures capacity only.
4. Legacy `UGP_CargoComponent::CargoCapacity` is **not** deleted (authored BP compatibility).

## MainBase storage ownership / fallback

1. Loaded BuildingDefinition 100×5 configures `UGP_StorageComponent` before drop-off is accepted.
2. Empty BuildingDefinition preserves component 100×5.
3. Pending async BuildingDefinition does **not** lock 100×5. Storage stays unconfigured until load success or explicit failure fallback.
4. ReadyThreshold == ContainerCapacity. Full 100 → Ready; 99 does not. Total 5×100 = 500.
5. `AGP_BuildingBase::BuildingDefinitionAsset` is the designer path for predeployed MainBase (and all buildings). Same empty/loaded/async/failure semantics as UnitDefinition. No `LoadSynchronous`.

## Ferronite deposit ownership

Moved onto `UGP_ResourceDefinition`:

- `DepositMaxAmount = 5000`
- `MaxConcurrentMiners = 4`

Unchanged on ResourceDefinition:

- `AmountPerMiningCycle = 10`
- `MiningCycleDurationSeconds = 1`
- `InteractionRangeCm = 200`
- `ScoreConversionRate = 1`
- `OrbitalConversionRate = 1`
- `ThreatPerStoredUnit = 0.5`

`AGP_ResourceNode::CurrentAmount` remains runtime / `EditInstanceOnly`. Init precedence: definition default → explicit authored override (value ≠ native CDO) → runtime replicated CurrentAmount. Legacy node fields remain compatibility fallback.

## Unit acquisition definition schema

`UGP_OrbitalUnitDropDefinition : UPrimaryDataAsset`  
`PrimaryAssetType = GPOrbitalUnitDropDefinition`

| Field | Worker | Salvage Walker |
| --- | --- | --- |
| Cost | 25 | 50 |
| TransportSlotCost | 1 | 2 |
| DeliveryDescentSeconds | 2.5 | 2.5 |
| PayloadDeployDelaySeconds | 1.25 | 1.25 |
| UnitDefinition | Worker def | Walker def |
| PayloadClass | empty in native catalog (settings BP bridge → native class) | same |

Also: DisplayName, Icon.

`FGP_UnitDropManifest` still has `WorkerCount` / `SalvageWalkerCount`. `ComputeManifestCosts` and payload resolve read the unit drop catalog first.

## Building acquisition timing

`UGP_OrbitalDropDefinition` now also owns:

- `DeliveryDescentSeconds = 2.5`
- `PayloadDeployDelaySeconds = 2.0`

Cost unchanged: Hub 100 / Turret 150 / Wall 25 / WallTurret 75.

Not moved: spawn altitude, cleanup, placement radius, overlap margin.

## Global settings that remain global

`UGP_OrbitalDeliverySettings`:

- `PodTransportSlotCapacity`
- `UnitDropPodClass`
- `UnitDropSpawnAltitudeCm`
- `UnitDropSpawnSpacingCm`
- `UnitDropCleanupDelaySeconds`
- `BuildingDropSpawnAltitudeCm`
- `BuildingDropCleanupDelaySeconds`
- `BuildingMaxDeployRadiusFromMainBaseCm`
- `BuildingPlacementOverlapMarginCm`

Deprecated (kept, not deleted): Worker/Walker slot costs, orbital costs, payload classes. Global default descent/deploy remain compatibility fallback.

`UGP_ResourceGameplaySettings::ContainerLaunchDurationSeconds` stays global launch presentation timing.

## UnitCap ownership decision

- **Initial MaxUnits:** stay on PlayerState / `UGP_GE_UnitCap_Base5` (+5). Match/player economy. Not UnitDefinition.
- **Logistics Hub bonus:** `UGP_BuildingDefinition.UnitCapBonus` (5). `UGP_GE_UnitCap_Plus5` is SetByCaller (`GP.UnitCap.BonusMagnitude`). Empty BuildingDef → Hub class fallback 5. Pending BuildingDef → do not apply yet. No hidden hardcoded +5.

## Delivery timing names

- `DeliveryDescentSeconds` = pod falling / telegraph
- `PayloadDeployDelaySeconds` = delay after impact before payload appears
- Cleanup = global pod presentation cleanup

Perceived usable time ≈ Descent + PayloadDeployDelay (units ~3.75s, buildings ~4.5s). No third duplicate `DeliveryTime`.

## Preserved baseline (no rebalance)

| Value | Amount |
| --- | --- |
| Worker cargo | 50 |
| MainBase storage | 100 × 5 (total 500) |
| Ferronite deposit | 5000 / 4 miners |
| Mining | 10 / 1s |
| Interaction range | 200 |
| Orbital / score conversion | 1 / 1 |
| Threat per stored unit | 0.5 |
| Worker acquisition | 25 cost / 1 slot |
| Walker acquisition | 50 cost / 2 slots |
| Pod slot capacity | 4 |
| Hub UnitCapBonus | +5 |
| Building costs | 100 / 150 / 25 / 75 |
| Unit delivery | 2.5 + 1.25 |
| Building delivery | 2.5 + 2.0 |
| Container launch telegraph | 2.5s (global settings) |

## Exact tests

| Command | Result |
| --- | --- |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | Complete Failures=0 |
| `gp.Units.RunUnitDefinitionContractTest` | Complete Failures=0 |
| `gp.Resource.RunS28RegressionSuite` | GP-S28 RegressionSuite Complete Failures=0 |
| `gp.Resource.RunContainerLaunchContractTest` | Complete Failures=0 |
| `gp.Resource.RunContainerLaunchHUDContractTest` | Complete Failures=0 Cancelled=None |
| `gp.Resource.RunOrbitalUnitDropContractTest` | Complete Failures=0 |
| `gp.Resource.RunUnitCapLogisticsHubContractTest` | Complete Failures=0 |
| `gp.Building.RunOrbitalBuildingDropContractTest` | Complete Failures=0 |
| `gp.Building.RunMultiBuildingDataContractTest` | Complete Failures=0 |
| `gp.Building.RunDefensiveTurretContractTest` | Complete Failures=0 |
| `gp.Building.RunBuildGridContractTest` | Complete Failures=0 |
| `gp.Combat.RunAutoAcquireContractTest` | Complete Failures=0 |
| `gp.Combat.RunAttackMoveContractTest` | Complete Failures=0 |
| `gp.Match.RunWinLoseContractTest` | Complete Failures=0 |
| `gp.Movement.RunRTSMovementReconciliationContractTest` | Complete Failures=0 |

All: **Failures=0**.

## Builds

| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASS** |
| GP Win64 Development | not run (candidate build only) |
| GP Win64 Shipping | not run (candidate build only) |

## Exact changed files

- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Claude_Tasks/GP-S39E_Economy_Logistics_Data.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/TDD/05_Unit_Architecture.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/07_Resource_Architecture.md`
- `Docs/TDD/10_Data_Assets.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `GP/Source/GPGASRuntime/Private/Effects/GPGE_UnitCap_Plus5.cpp`
- `GP/Source/GPGASRuntime/Public/Effects/GPGE_UnitCap_Plus5.h`
- `GP/Source/GPRuntime/Private/Buildings/GPBuildingBase.cpp`
- `GP/Source/GPRuntime/Private/Buildings/GPLogisticsHub.cpp`
- `GP/Source/GPRuntime/Private/Buildings/GPMainBase.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPDefensiveTurretContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPEconomyLogisticsDataContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPMultiBuildingDataContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalBuildingDropContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalUnitDropContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPUnitCapLogisticsHubContractTest.cpp`
- `GP/Source/GPRuntime/Private/GPRuntime.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropCatalog.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPDropPod.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPOrbitalDropDefinition.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPOrbitalUnitDropCatalog.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPOrbitalUnitDropDefinition.cpp`
- `GP/Source/GPRuntime/Private/Orbital/GPUnitDropAuthority.cpp`
- `GP/Source/GPRuntime/Private/Resources/GPCargoComponent.cpp`
- `GP/Source/GPRuntime/Private/Resources/GPResourceDefinition.cpp`
- `GP/Source/GPRuntime/Private/Resources/GPResourceNode.cpp`
- `GP/Source/GPRuntime/Private/Resources/GPStorageComponent.cpp`
- `GP/Source/GPRuntime/Private/UI/GPTEMP_S28P_PlanetaryFerroniteHUD.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitDefinitionCatalog.cpp`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingBase.h`
- `GP/Source/GPRuntime/Public/Buildings/GPBuildingDefinition.h`
- `GP/Source/GPRuntime/Public/Buildings/GPLogisticsHub.h`
- `GP/Source/GPRuntime/Public/Buildings/GPMainBase.h`
- `GP/Source/GPRuntime/Public/Economy/GPEconomyLogisticsDataContractTest.h`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropCatalog.h`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalDropDefinition.h`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropCatalog.h`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropDefinition.h`
- `GP/Source/GPRuntime/Public/Orbital/GPUnitDropAuthority.h`
- `GP/Source/GPRuntime/Public/Resources/GPCargoComponent.h`
- `GP/Source/GPRuntime/Public/Resources/GPResourceDefinition.h`
- `GP/Source/GPRuntime/Public/Resources/GPResourceNode.h`
- `GP/Source/GPRuntime/Public/Resources/GPStorageComponent.h`
- `GP/Source/GPRuntime/Public/Settings/GPOrbitalDeliverySettings.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinition.h`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinitionCatalog.h`

## Protected / operator assets untouched

Not committed:

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- operator-created UnitDefinition / BuildingDefinition / ResourceDefinition / OrbitalDrop `.uasset` files
- `BP_SalvageWalker` / `BP_GP_SalvageWalker`
- `BP_MainBase` / `BP_GP_MainBase`
- `BP_LogisticsHub` / `BP_GP_LogisticsHUB`
- `BP_GP_DefensiveTurret`
- untracked `GP/Content/Basic_VFX/`, `GrimProtocol/Blueprint/`, `Materials/`, `Mixed_Magic_VFX_Pack/`, `RocketThrusterExhaustFX/`, `Tools/`

## Operator acceptance (later, incremental)

Example: Worker `CargoCapacity` 50→20; MainBase 100×5→40×2; Worker Cost 25→17. Not required in this candidate.

## Follow-up naming

Timed Retaliation Pursuit is **GP-S40R** (was GP-S39R). Not started.

**NOT MERGED.**
**NOT FINALIZED.**
