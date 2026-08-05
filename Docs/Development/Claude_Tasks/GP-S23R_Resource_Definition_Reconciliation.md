# GP-S23R — Resource Definition Reconciliation

## Status
**GP-S23R_CODE_AND_FERRONITE_DATA_ASSET_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `9b3ec9997c2544764d0bd10c6bc4cdfb659dcb2f`

Branch: `feature/gp-s23r-resource-definition`  
Candidate: `bed8fb3adbbcd0e7dcd9f0d3069616c522afcb81`  
Correction: `fef94837839ed25041fe5dc0256a1472231c0642`

## Canonical roadmap position
Canonical **GP-S23**. Next after finalization: **GP-S24R**.

## Correction — single-source mining balance
Removed editable `MineRatePerWorker` UPROPERTY. Dual balance sources are forbidden.

**Canonical stored mining fields:**
- `AmountPerMiningCycle`
- `MiningCycleDurationSeconds`
- `InteractionRangeCm`

**Derived only (not stored):**
- `GetEffectiveMineRatePerWorker()` = Amount / Duration

### Future MiningComponent contract
`UGP_MiningComponent` must use:
- `AmountPerMiningCycle` → ResourceNode consume / cargo delta
- `MiningCycleDurationSeconds` → cycle timer
- `InteractionRangeCm` → interaction distance
- `GetEffectiveMineRatePerWorker()` → UI / diagnostics / analytics only

## Architecture
`UGP_ResourceDefinition : UPrimaryDataAsset` — immutable tunables; no tick; no economy execution.  
Internal `EGP_ResourceType::Ore` + Ferronite identity (`DisplayName`, `GP.Resource.Type.Ferronite`).

## Final exact properties
| Field | Kind |
| --- | --- |
| ResourceType | Stored |
| DisplayName / Description | Stored |
| ResourceGameplayTag | Stored |
| Icon (soft) | Stored |
| AmountPerMiningCycle | Stored (mining SoT) |
| MiningCycleDurationSeconds | Stored (mining SoT) |
| InteractionRangeCm | Stored |
| ScoreConversionRate | Stored (metadata) |
| ThreatPerStoredUnit | Stored (metadata) |
| Tint | Stored (metadata) |
| GetEffectiveMineRatePerWorker | Derived |

## PrimaryAssetId / Asset Manager
`GPResourceDefinition:DA_GP_Resource_Ferronite`  
Scan + AlwaysCook: `/Game/GrimProtocol/DataAssets/Resources` (unchanged).

## Prototype values
| Property | Value |
| --- | --- |
| AmountPerMiningCycle | 10 |
| MiningCycleDurationSeconds | 1.0 |
| InteractionRangeCm | 200 |
| EffectiveMineRatePerWorker | **10 u/s derived** |
| ScoreConversionRate | 1.0 |
| ThreatPerStoredUnit | 0.5 (placeholder) |

## Validation
No MineRate field/mismatch checks. Retains ResourceType, DisplayName, tag, Amount>0, Cycle>0, Range>0, Score/Threat finite ≥0.

## Diagnostics
`gp.ResourceDefinition.Inspect` logs Amount, CycleDuration, **EffectiveMineRatePerWorker**, InteractionRangeCm — not a stored MineRate.

Seed: `-run=GPResourceDefinitionSeed` / `-VerifyOnly`

## Operator validation
Open DA → confirm no MineRate property → edit CycleDuration → Inspect shows derived rate → restore → VerifyOnly → Asset Manager → no map changes.

## Known limitations
Ore enum name retained; Icon unset; orbital metadata not executed; no ResourceNode wiring until S24R.

## Next stage
**GP-S24R — Ferronite Deposit Contract on AGP_ResourceNode**
