# Cursor Work Report — Dead Overlap Setting Removal

## Status

**DEAD_OVERLAP_SETTING_REMOVAL_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-remove-dead-overlap-setting`
- Base: `origin/main` @ `f38e803771261c60d865949c693a52a73fbcedb2`
- Head: (this commit)

## Repository-wide search / classification

`BuildingPlacementOverlapMarginCm` occurrences before removal:

| Kind | Path | Notes |
| --- | --- | --- |
| Declaration | `GPOrbitalDeliverySettings.h` | UPROPERTY + class comment |
| Config text | `GP/Config/DefaultGame.ini` | `BuildingPlacementOverlapMarginCm=25.000000` |
| Test | `GPOrbitalDeliveryVisibilityContractTest.cpp` | Slice A hidden/deprecated expectation |
| Docs | audit / slice A task / prior report | ownership notes |
| Runtime reader | **none** | |

No `GConfig` / `GetFloat` / `GetInt` access of this key exists in `GP/Source`. Zero production runtime readers.

## C++ removal

Removed `UGP_OrbitalDeliverySettings::BuildingPlacementOverlapMarginCm` (UPROPERTY, default, comments). No redirect or replacement property.

Placement, footprint, SAT/OBB, deploy radius, NavigationObstacle, and related systems were not touched.

## Stale DefaultGame.ini key

Intentionally **not** edited. Protected local config exists. After C++ removal the leftover INI key cannot populate a runtime field. Harmless legacy text for a later dedicated config-hygiene operation.

## Settings contract change

`gp.Settings.RunOrbitalDeliveryVisibilityContractTest`:

- OLD: property exists, Config, hidden/deprecated
- NEW: `FindPropertyByName("BuildingPlacementOverlapMarginCm") == nullptr`

Other Slice A visibility/metadata assertions unchanged.

## Tests / results

| Check | Result |
| --- | --- |
| `gp.Settings.RunOrbitalDeliveryVisibilityContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |

Unit-drop / Wall Package / full suite not run.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |

GP Win64 Development / Shipping not run (finalization after operator PASS).

## Operator test (not claimed PASS)

Project Settings → Game → GP Orbital Delivery: field gone. Normal building placement and Hub/Turret deploy still work.

## Protected-files confirmation

Committed diff excludes maps, `DefaultGame.ini`, `DefaultEngine.ini`, Blueprints, DataAssets, materials, and other untracked Content.

## Runtime placement behavior unchanged

## NOT MERGED

## NOT FINALIZED
