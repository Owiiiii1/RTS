# Cursor Work Report — Unit Numeric Compatibility Cleanup

## Status

**UNIT_NUMERIC_COMPAT_CLEANUP_FINALIZED_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `feature/gp-unit-numeric-compat-cleanup`
- Base: `origin/main` @ `967e6ea3a5b81ddc1a2c19c4bfe292f5ef989507`
- Head: (this commit)

## Operator PASS summary

- Worker price/display is correct
- Worker purchase succeeds
- Worker delivery succeeds
- Orbital Ferronite deduction matches displayed/product cost
- Salvage Walker price/display is correct
- Salvage Walker purchase succeeds
- Salvage Walker delivery succeeds
- transport-slot behavior shows no regression

## Four removed settings fields

Completely absent from `UGP_OrbitalDeliverySettings`:

- `WorkerTransportSlotCost`
- `SalvageWalkerTransportSlotCost`
- `WorkerOrbitalDropCost`
- `SalvageWalkerOrbitalDropCost`

No production reader remains. No GConfig/string compatibility reader was added.

## Canonical native bootstrap ownership

Native bootstrap numerics are owned by `UGP_OrbitalUnitDropCatalog` constants and applied in `EnsureNativeCatalog()` onto the native drop products:

- Worker: Cost **25**, TransportSlotCost **1**
- Salvage Walker: Cost **50**, TransportSlotCost **2**

Authority and TEMP HUD do not duplicate these numbers. They read catalog getters that resolve the same product snapshot.

## Authored precedence / Pending readiness

- Authored Ready product Cost and TransportSlotCost win
- Pending authored products remain `DefinitionNotReady` (no spend / spawn)
- Nested pending uses the authored drop object's numerics when that object is present; native 25/1 is not substituted merely because nested assets are cold

## Authority + HUD source

- `GPUnitDropAuthority` reads resolved catalog/product Cost and TransportSlotCost
- TEMP HUD reads the same catalog getters
- Neither reads the removed Project Settings properties

## Stale INI keys untouched

Committed `GP/Config/DefaultGame.ini` still contains:

```
WorkerTransportSlotCost=1
SalvageWalkerTransportSlotCost=2
WorkerOrbitalDropCost=25.000000
SalvageWalkerOrbitalDropCost=50.000000
```

Intentionally not edited. After C++ removal they cannot populate runtime fields.

## Final tests / results

| Check | Result |
| --- | --- |
| `gp.Settings.RunOrbitalDeliveryVisibilityContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |

Full suite not run.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** (up to date) |
| `GP Win64 Development` | **PASS** |
| `GP Win64 Shipping` | **PASS** |

## Protected-files confirmation

Committed diff vs `origin/main` @ `967e6ea…` is unit-procurement C++ ownership, targeted contracts, and docs only:

- no `WorkerPayloadClass` / `SalvageWalkerPayloadClass` changes
- no timing fallback field changes
- no building procurement/payload changes
- no `BuildingDropCatalog` / Wall Package changes
- no maps
- no `DefaultGame.ini`
- no `DefaultEngine.ini`
- no Blueprint / DataAsset / material / content changes

No new functionality in this finalization.

## NOT MERGED
