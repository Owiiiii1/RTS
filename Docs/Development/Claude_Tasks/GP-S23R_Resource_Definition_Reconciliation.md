# GP-S23R — Resource Definition Reconciliation

## Status
**GP-S23R_CODE_AND_FERRONITE_DATA_ASSET_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `9b3ec9997c2544764d0bd10c6bc4cdfb659dcb2f`

Branch: `feature/gp-s23r-resource-definition`  
Implementation: `bed8fb3adbbcd0e7dcd9f0d3069616c522afcb81`

## Canonical roadmap position
Canonical **GP-S23** (`UGP_ResourceDefinition`).  
Next after operator validation / finalization: **GP-S24R** (ResourceNode Ferronite deposit contract).

Does **not** implement Deposit integration, Cargo, Mining SM, Worker, Storage, or orbital execution.

## Architecture
- `UGP_ResourceDefinition : UPrimaryDataAsset` in GPRuntime
- Immutable tunable definition data only (no tick, no runtime mutable economy state)
- Internal `EGP_ResourceType::Ore` + canonical Ferronite identity (`DisplayName`, `GP.Resource.Type.Ferronite`)
- MiningComponent (future S26) must read cycle/rate/range from this asset — no C++ hardcoded balance

## Fields and ownership

| Field | Owner | Notes |
| --- | --- | --- |
| ResourceType | Definition | Ore for Ferronite prototype |
| DisplayName / Description | Definition | Player-facing Ferronite |
| ResourceGameplayTag | Definition | `GP.Resource.Type.Ferronite` (existing native tag) |
| Icon | Definition | Soft `UTexture2D` (optional; unset in prototype) |
| AmountPerMiningCycle | Definition | Prototype 10 |
| MiningCycleDurationSeconds | Definition | Prototype 1.0 s |
| InteractionRangeCm | Definition | Prototype 200 cm |
| MineRatePerWorker | Definition | Prototype 10 u/s (aligned with Amount/Duration) |
| ScoreConversionRate | Definition | Metadata 1.0 — no launch execution |
| ThreatPerStoredUnit | Definition | Prototype 0.5 — no ThreatValue write |
| Tint | Definition | Presentation metadata only |

Not on this asset: Worker CarryCapacity, Storage capacity, miner queue.

## PrimaryAssetId policy
`GetPrimaryAssetId()` → `GPResourceDefinition:<AssetFName>`  
Example: `GPResourceDefinition:DA_GP_Resource_Ferronite`

## Asset Manager policy
`DefaultGame.ini`:
- `PrimaryAssetTypesToScan` type `GPResourceDefinition` under `/Game/GrimProtocol/DataAssets/Resources` (AlwaysCook)
- `DirectoriesToAlwaysCook` for the same path  
No synchronous gameplay load paths added.

## Prototype values (`DA_GP_Resource_Ferronite`)
| Property | Value | Source |
| --- | --- | --- |
| ResourceType | Ore | Current enum (not renamed) |
| DisplayName | Ferronite | TDD/10 |
| Tag | GP.Resource.Type.Ferronite | Native registry |
| AmountPerMiningCycle | 10 | Derived from MineRate 10/s × 1s cycle |
| MiningCycleDurationSeconds | 1.0 | Prototype |
| InteractionRangeCm | 200 | TDD Worker MiningRange |
| MineRatePerWorker | 10 | TDD/10 |
| ScoreConversionRate | 1.0 | TDD/10 |
| ThreatPerStoredUnit | 0.5 | TDD recommended starting TBD |
| Icon | None | Soft unset |

**These are prototype defaults — not final balance.**

## Validation
`ValidateDefinition` + editor `IsDataValid`:
- ResourceType ≠ None
- DisplayName non-empty
- Valid gameplay tag
- Positive finite Amount / Cycle / Range / MineRate
- Non-negative ScoreConversion / ThreatPerStoredUnit
- Warning if MineRate vs Amount/Duration diverge >5%

## Diagnostics
- `gp.ResourceDefinition.Inspect [SoftObjectPath]` (non-shipping)
- Seed/verify: `-run=GPResourceDefinitionSeed` / `-VerifyOnly`

## Networking / tick
- Definition is content data (not replicated gameplay mutation)
- No permanent tick
- Soft Icon never sync-forced in gameplay this stage

## In-scope / out-of-scope
**In:** class, Ferronite DA, validation, PrimaryAssetId, Asset Manager scan, seed/verify, Inspect, docs.  
**Out:** ResourceNode tags/integration, Mine command target, Cargo/Mining/Worker/Storage, ThreatValue writes, orbital conversion, map, visual profiles.

## Acceptance criteria
1. DA opens as `UGP_ResourceDefinition`
2. Fields editable; Save persists
3. PrimaryAssetId stable; Asset Manager sees asset
4. IsDataValid / VerifyOnly pass
5. Inspect logs expected fields
6. Game target has no GPEditor dependency
7. Map unchanged

## Operator validation
1. Open `/Game/GrimProtocol/DataAssets/Resources/DA_GP_Resource_Ferronite`
2. Confirm class `UGP_ResourceDefinition`; identity Ore + Ferronite + tag
3. Temporarily change `MiningCycleDurationSeconds`; Save
4. `gp.ResourceDefinition.Inspect`
5. Restore value; Save
6. `-run=GPResourceDefinitionSeed -VerifyOnly`
7. Confirm Asset Manager visibility
8. Do not change the map

## Known limitations
- Ore enum name retained (rename later)
- Icon unset; Tint unused until presentation pass
- ThreatPerStoredUnit / rates are placeholders
- No ResourceNode wiring until S24R

## Next canonical stage
**GP-S24R — Ferronite Deposit Contract on AGP_ResourceNode**
