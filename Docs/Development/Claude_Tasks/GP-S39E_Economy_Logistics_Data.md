# GP-S39E — Economy / Logistics Data Ownership

## Status
**GP-S39E_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Slice Group
Post-GP-S38D (Unit/Building Combat Data is on verified `main` @ `f841cdee19c97a0dfaacb8fc0bdd27623c543329`)

## Branch
`feature/gp-s39e-economy-logistics-data`  
Base: `origin/main` @ `f841cdee19c97a0dfaacb8fc0bdd27623c543329`

## Goal
Designer-facing data ownership for economy / logistics tunables. No GameBalance singleton. No rebalance. No binary DataAssets.

| Asset | Owns |
| --- | --- |
| `UGP_UnitDefinition` | intrinsic unit gameplay **+ unit-owned logistics** (`CargoCapacity`) |
| `UGP_BuildingDefinition` | identity/grid **+ storage** (`ContainerCapacity` / `ContainerCount`) **+ `UnitCapBonus`** |
| `UGP_ResourceDefinition` | Ferronite mining/orbital **+ deposit defaults** (`DepositMaxAmount`, `MaxConcurrentMiners`) |
| `UGP_OrbitalUnitDropDefinition` | unit purchase / slot / payload / delivery timing |
| `UGP_OrbitalDropDefinition` | building purchase + delivery timing |
| Global settings | only genuine transport / world-system tunables |

## Pillar 8

| Question | Answer |
| --- | --- |
| Strengthens core loop? | YES — resource extraction, shipping and orbital procurement become tunable. |
| Meaningful player decisions? | YES — cargo, storage, costs, slots and delivery timing define economy choices. |
| Testable now? | YES |
| Bounded? | YES — data ownership + initialization, no balance pass. |
| Avoid speculative framework? | YES — extends existing Unit/Building/Resource/Drop definitions only. |

Verdict **PASS**.

## Factual old ownership (pre-S39E, audited on main)

| Tunable | Owner before S39E |
| --- | --- |
| Worker cargo 50 | `UGP_CargoComponent::CargoCapacity` |
| MainBase storage 100×5 | `UGP_StorageComponent` CDO |
| Ready threshold | `CurrentAmount >= ContainerCapacity` (MVP == capacity; no separate LaunchReadyThreshold) |
| Ferronite mining / conversion / threat | already `UGP_ResourceDefinition` |
| Deposit MaxAmount 5000 / CurrentAmount / MaxConcurrentMiners 4 | `AGP_ResourceNode` |
| Worker/Walker cost 25/50, slots 1/2, payload classes | `UGP_OrbitalDeliverySettings` |
| Pod slot capacity 4, unit/building altitude/spacing/cleanup/radius/overlap | `UGP_OrbitalDeliverySettings` |
| Unit descent 2.5 / deploy 1.25 | `UGP_OrbitalDeliverySettings` |
| Building descent 2.5 / deploy 2.0 | `UGP_OrbitalDeliverySettings` |
| Building costs 100/150/25/75 | `UGP_OrbitalDropDefinition.Cost` (Hub may sync deprecated settings cost) |
| Container launch telegraph 2.5s | `UGP_ResourceGameplaySettings::ContainerLaunchDurationSeconds` |
| Initial MaxUnits +5 | `UGP_GE_UnitCap_Base5` on PlayerState (match/player) |
| Hub +5 | hardcoded `FScalableFloat(5)` on `UGP_GE_UnitCap_Plus5` |
| MainBase BuildingDefinition ref | **missing** (predeployed, not purchased) |

## Canonical ownership after S39E

| Concern | Canonical | Runtime / fallback |
| --- | --- | --- |
| Worker cargo capacity | `UGP_UnitDefinition.CargoCapacity` (50) | `UGP_CargoComponent` state; empty def keeps component 50 |
| Walker / buildings cargo | 0 | no cargo / unused |
| MainBase storage | `UGP_BuildingDefinition` 100×5 | component 100×5 if BuildingDef empty |
| Hub / turret / wall storage | 0 / 0 | no Ferronite containers |
| Ready threshold | == `ContainerCapacity` | no partial launch |
| Ferronite mining / conversion / threat | `UGP_ResourceDefinition` (unchanged) | — |
| Deposit max / concurrent miners | `UGP_ResourceDefinition.DepositMaxAmount` / `MaxConcurrentMiners` | node fields if def unresolved; authored instance override if value ≠ native CDO |
| Node `CurrentAmount` | runtime + `EditInstanceOnly` | starts at MaxAmount unless instance override |
| Unit cost / slots / payload / descent / deploy | `UGP_OrbitalUnitDropDefinition` | settings deprecated fields as fallback |
| Building cost | `UGP_OrbitalDropDefinition.Cost` (unchanged) | Hub settings cost bridge retained |
| Building descent / deploy | `UGP_OrbitalDropDefinition` 2.5 / 2.0 | settings fallback |
| Pod capacity, altitude, spacing, cleanup, radius, overlap | `UGP_OrbitalDeliverySettings` | global |
| Container launch duration | `UGP_ResourceGameplaySettings` | global (not moved) |
| Initial MaxUnits | `UGP_GE_UnitCap_Base5` (+5) | match/player — **not** UnitDefinition |
| Hub UnitCapBonus | `UGP_BuildingDefinition.UnitCapBonus` (5) | Hub class fallback 5 if def empty; GE is SetByCaller |
| BuildingDefinition on actors | `AGP_BuildingBase::BuildingDefinitionAsset` soft | empty / loaded / async / failure — same as UnitDefinition |

## Delivery timing names

- `DeliveryDescentSeconds` = pod falling / telegraph
- `PayloadDeployDelaySeconds` = delay after impact before payload appears
- Cleanup stays global on settings
- Perceived usable time ≈ Descent + PayloadDeployDelay (units ~3.75s, buildings ~4.5s)
- No third duplicate `DeliveryTime`

## UnitCap decision

- **Initial MaxUnits:** stay on PlayerState / `UGP_GE_UnitCap_Base5` (match/player economy). Not UnitDefinition.
- **Hub bonus:** `BuildingDefinition.UnitCapBonus`. `UGP_GE_UnitCap_Plus5` is now SetByCaller (`GP.UnitCap.BonusMagnitude`). No hidden hardcoded +5 in the GE.

## Preserved baseline (no rebalance)

Worker cargo 50; MainBase 100×5 (total 500); Ferronite deposit 5000 / 4 miners / mine 10 per 1s / range 200 / conversions 1/1 / threat 0.5; Worker 25/1; Walker 50/2; pod slots 4; Hub bonus +5; building costs 100 / 150 / 25 / 75; unit delivery 2.5+1.25; building delivery 2.5+2.0.

## Operator assets (do not commit binaries)

`DA_GP_Unit_Worker` / `SalvageWalker` / `MainBase` / `LogisticsHub` / `DefensiveTurret`  
`DA_GP_Building_MainBase` / `LogisticsHub` / `DefensiveTurret`  
`DA_GP_Resource_Ferronite`  
`DA_GP_OrbitalUnitDrop_Worker` / `SalvageWalker`  
`DA_GP_OrbitalDrop_LogisticsHub` / `DefensiveTurret` (+ future Wall / WallTurret)

Native catalogs permit contracts without `.uasset` files.

## Contracts

`gp.Economy.RunEconomyLogisticsDataContractTest` — cases A–R.

Regressions listed in `Cursor_Work_Report.md`. All Failures=0.

## Follow-up naming

Timed Retaliation Pursuit is **GP-S40R** (was GP-S39R). Not started.

## Out of scope

Retaliation, balance redesign, partial container launch, upgrades, research, save/load, runtime tuning UI, Shop rewrite, manifest UI rewrite, Wall gameplay, FoW, binary DataAssets.
