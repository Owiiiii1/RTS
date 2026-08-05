# GP-S23R — Resource Definition Reconciliation

## Status
**GP-S23R_FINALIZED_READY_FOR_MERGE**

## Baseline
`main` @ `9b3ec9997c2544764d0bd10c6bc4cdfb659dcb2f`

Branch: `feature/gp-s23r-resource-definition`  
Candidate: `bed8fb3adbbcd0e7dcd9f0d3069616c522afcb81`  
Correction: `fef94837839ed25041fe5dc0256a1472231c0642`

## Canonical position
GP-S23 (`UGP_ResourceDefinition`). Next: **GP-S24R**.

## Shipped
- `UGP_ResourceDefinition` PrimaryDataAsset; PrimaryAssetId type `GPResourceDefinition`
- `DA_GP_Resource_Ferronite` (LFS)
- Mining SoT: AmountPerMiningCycle + MiningCycleDurationSeconds + InteractionRangeCm
- `GetEffectiveMineRatePerWorker()` derived only
- Asset Manager scan + AlwaysCook (single registration)
- Seed/VerifyOnly commandlet; `gp.ResourceDefinition.Inspect`
- No ResourceNode / Cargo / Mining / Worker / map changes

## Operator validation matrix (accepted)

| Item | Result |
| --- | --- |
| DA opens / class correct | **PASS** |
| Identity Ore + Ferronite + tag | **PASS** |
| Mining values 10 / 1 / 200 / Effective 10 | **PASS** |
| No stored MineRatePerWorker | **PASS** |
| Inspect Valid=true Errors=0 Warnings=0 AssetManager path | **PASS** |
| Data Validation valid | **PASS** |
| VerifyOnly Success 0 error 0 warning | **PASS** |
| AssetManagerSees=true | **PASS** |
| Map unchanged | **PASS** |

## Final DataAsset values
| Field | Value |
| --- | --- |
| ResourceType | Ore |
| DisplayName | Ferronite |
| GameplayTag | GP.Resource.Type.Ferronite |
| PrimaryAssetId | GPResourceDefinition:DA_GP_Resource_Ferronite |
| AmountPerMiningCycle | 10 |
| MiningCycleDurationSeconds | 1 |
| InteractionRangeCm | 200 |
| EffectiveMineRatePerWorker | 10 (derived) |

## Builds (finalization)
- GPEditor Dev+UHT: retained from correction `fef9483…` (C++ unchanged at finalization)
- GP Win64 Development — **PASSED**
- GP Win64 Shipping — **PASSED**

## Known limitations
- Ore enum name retained until rename stage
- Icon unset; ThreatPerStoredUnit / rates are prototypes
- No deposit integration until S24R

## Next stage
**GP-S24R — Ferronite Deposit Contract on AGP_ResourceNode**

No known blockers. Ready for main merge when requested.
