# Cursor Work Report — Settings Visibility Truth

## Status

**SETTINGS_VISIBILITY_TRUTH_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-settings-visibility-truth`
- Base: `origin/main` @ `283297012c1cefe162028a7ba4166c02a81230cc`
- Head: (this commit)

## What changed

Editor exposure and labels on `UGP_OrbitalDeliverySettings` only. No runtime readers, precedence, defaults, INI, or authored content.

Approach: keep `Config` serialization; drop `EditAnywhere` on hidden compatibility fields so they are not Project Settings controls. No custom details panel.

## Editor exposure removed (still Config + same names/types/defaults/readers)

- `WorkerTransportSlotCost`
- `SalvageWalkerTransportSlotCost`
- `WorkerOrbitalDropCost`
- `SalvageWalkerOrbitalDropCost`
- `WorkerPayloadClass`
- `SalvageWalkerPayloadClass`
- `BuildingOrbitalPurchaseCost`
- `BuildingPayloadClass`
- `BuildingPlacementOverlapMarginCm` (unused; hidden; not wired)

## Relabeled as fallback seeds (still editable)

- `UnitDropDescentDurationSeconds` — Fallback Defaults | Unit Product Timing
- `UnitDropPayloadDeployDelaySeconds` — Fallback Defaults | Unit Product Timing
- `BuildingDropDescentDurationSeconds` — Fallback Defaults | Building Product Timing
- `BuildingDropPayloadDeployDelaySeconds` — Fallback Defaults | Building Product Timing

Display names include `(Fallback Seed)`. Tooltips state canonical product definitions normally overwrite these. Wall Package still owns its own timing.

## DefensiveTurretPayloadClass

Remains `EditAnywhere`. Category/display/tooltip mark it as **LEGACY compatibility override**. `DeprecatedProperty` not applied (must stay operator-editable until slice E). Precedence unchanged: still outranks `BuildingDefinition.SpawnedClass`.

## BuildingPlacementOverlapMarginCm

Config retained. `EditAnywhere` removed. `DeprecatedProperty` + comment: unused, does not affect placement. No gameplay wiring.

## Unchanged

- Canonical DataAsset refs and true global transport/world fields remain normal editable settings
- Property names, types, C++ in-class defaults
- Runtime readers and fallback precedence
- No INI migration

## Tests / results

| Check | Result |
| --- | --- |
| `gp.Settings.RunOrbitalDeliveryVisibilityContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Orbital.RunWallPackageInventoryContractTest` | `Complete Failures=0 Cancelled=false` |

Full suite not run.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |

GP Win64 Development / Shipping not run (finalization after operator PASS).

## Operator test (not claimed PASS)

Project Settings → Game → GP Orbital Delivery:

- DataAsset refs obvious
- true globals still available
- old Worker/Walker cost/slot/payload bridges no longer look like normal settings
- fallback unit/building timing explicitly labeled
- dead overlap margin no longer looks functional
- Defensive Turret LEGACY override clearly identified
- existing gameplay still works

## Protected-files confirmation

Committed diff excludes maps, `DefaultGame.ini`, `DefaultEngine.ini`, Blueprints, DataAssets, materials, and other untracked Content.

## NOT MERGED

## NOT FINALIZED
