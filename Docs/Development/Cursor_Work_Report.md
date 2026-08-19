# Cursor Work Report — Unit Numeric Compatibility Cleanup

## Status

**UNIT_NUMERIC_COMPAT_CLEANUP_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-unit-numeric-compat-cleanup`
- Base: `origin/main` @ `967e6ea3a5b81ddc1a2c19c4bfe292f5ef989507`
- Head: (this commit)

## Pre-change reference classification

Repository-wide search of the four names before C++ removal:

| Kind | Path | Notes |
| --- | --- | --- |
| Declaration | `GP/Source/GPRuntime/Public/Settings/GPOrbitalDeliverySettings.h` | Hidden Config UPROPERTYs: Worker slots=1, Walker slots=2, Worker cost=25, Walker cost=50 |
| Config text | `GP/Config/DefaultGame.ini` | Stale keys `WorkerTransportSlotCost=1`, `SalvageWalkerTransportSlotCost=2`, `WorkerOrbitalDropCost=25.000000`, `SalvageWalkerOrbitalDropCost=50.000000` |
| Catalog construction | `GPOrbitalUnitDropCatalog.cpp` `EnsureNativeCatalog()` | Already wrote Worker 25/1 and Walker 50/2 onto native drop objects |
| Catalog getters | `GetWorkerTransportSlotCost` / `GetSalvageWalkerTransportSlotCost` / `GetWorkerOrbitalDropCost` / `GetSalvageWalkerOrbitalDropCost` | Canonical Ready product first, else settings, else hardcoded 25/50/1/2 |
| Authority / purchase | `GPUnitDropAuthority.cpp` | Already used catalog getters; Pending → `DefinitionNotReady` before spend |
| UI reader | `GPTEMP_S28P_PlanetaryFerroniteHUD.cpp` | Already used catalog getters; no direct settings numeric read |
| Visibility contract | `GPOrbitalDeliveryVisibilityContractTest.cpp` | Expected the four properties hidden/deprecated |
| Economy contract | `GPEconomyLogisticsDataContractTest.cpp` | Asserted catalog getters 25/1 native and 17/3 authored Ready |
| Unit-drop contract | `GPOrbitalUnitDropContractTest.cpp` | Purchase/pending coverage; numeric ownership cases added in this slice |
| Docs | `Configuration_Data_Ownership_Audit.md` | Listed as DEPRECATED_ACTIVE catalog fallbacks |
| Production GConfig / string lookup | **none** in `GP/Source` | |

Numeric procurement 25 / 50 / 1 / 2 also appeared as native bootstrap literals in catalog construction and contracts. No broader dependency outside unit procurement / catalog / UI.

## Exact removed settings fields

Completely removed from `UGP_OrbitalDeliverySettings`:

- `WorkerTransportSlotCost`
- `SalvageWalkerTransportSlotCost`
- `WorkerOrbitalDropCost`
- `SalvageWalkerOrbitalDropCost`

No Unreal redirect added. No replacement DeveloperSettings field. No new DataAsset. `DefaultGame.ini` not used as authority.

## Native bootstrap ownership

Single production source: `UGP_OrbitalUnitDropCatalog` constants and `EnsureNativeCatalog()` assignment onto native drop products.

- `UGP_OrbitalUnitDropCatalog::NativeWorkerOrbitalDropCost = 25`
- `UGP_OrbitalUnitDropCatalog::NativeWorkerTransportSlotCost = 1`
- `UGP_OrbitalUnitDropCatalog::NativeSalvageWalkerOrbitalDropCost = 50`
- `UGP_OrbitalUnitDropCatalog::NativeSalvageWalkerTransportSlotCost = 2`

Catalog getters, authority, HUD, and tests all consume the resolved catalog/product. Authority and HUD do not duplicate the numbers.

## Native Worker 25/1 and Walker 50/2 preserved

Yes. Unconfigured / failed authored slots still resolve to those native product values.

## Authority purchase source after migration

`GPUnitDropAuthority` still reads `UGP_OrbitalUnitDropCatalog` getters for product Cost and TransportSlotCost after `AreManifestDefinitionsReady`. It does not read the removed settings properties.

- Authored Ready product values win
- Pending authored product remains `DefinitionNotReady`
- no spend / manifest mutation / pod spawn while Pending
- failed/unconfigured authored slot follows existing native bootstrap policy
- transport slot validation uses the same resolved product used for purchase
- Orbital Ferronite deduction remains product Cost
- nested-readiness async semantics unchanged

## HUD source after migration

TEMP HUD still reads the same catalog getters as gameplay. No HUD redesign. Pending nested authored products expose authored Cost/Slots rather than native substitution. Top-level pending without a loaded drop object still has no authored product and therefore uses native bootstrap numbers, matching the old settings-bridge values (25/1 and 50/2).

## Authored product precedence preserved

Authored configured Ready Worker / Salvage Walker Cost and TransportSlotCost win over native bootstrap.

## Pending readiness behavior preserved

Pending still blocks purchase (`DefinitionNotReady`). Nested pending uses the authored drop object's numerics when that object is present; it does not substitute native 25/1 merely because nested assets are cold.

## Stale DefaultGame.ini keys untouched

`GP/Config/DefaultGame.ini` still contains:

```
WorkerTransportSlotCost=1
SalvageWalkerTransportSlotCost=2
WorkerOrbitalDropCost=25.000000
SalvageWalkerOrbitalDropCost=50.000000
```

Intentionally not edited. After C++ property removal they cannot populate runtime fields. No production GConfig/string lookup was added. Documented for later config-hygiene cleanup.

## Exact production files changed

- `GP/Source/GPRuntime/Public/Settings/GPOrbitalDeliverySettings.h`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropCatalog.h`
- `GP/Source/GPRuntime/Private/Orbital/GPOrbitalUnitDropCatalog.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalDeliveryVisibilityContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalUnitDropContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPEconomyLogisticsDataContractTest.cpp`

Docs:

- `Docs/Development/Configuration_Data_Ownership_Audit.md` (narrow Slice C mark)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Claude_Tasks/GP-Unit-Numeric-Compat-Cleanup.md`
- `Docs/Development/Cursor_Work_Report.md`

## Tests / results

| Check | Result |
| --- | --- |
| `gp.Settings.RunOrbitalDeliveryVisibilityContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |

No distinct extra purchase-flow command. Unit-drop + economy already cover authority spend. Building / Wall Package contracts not run. Full suite not run.

## GPEditor / UHT result

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `GP Win64 Development` | not run (finalization after operator PASS) |
| `GP Win64 Shipping` | not run (finalization after operator PASS) |

## Protected-files confirmation

Committed diff vs `origin/main` @ `967e6ea…` is C++ unit-procurement ownership + targeted contracts + docs:

- no maps
- no `DefaultGame.ini`
- no `DefaultEngine.ini`
- no Blueprint / DataAsset / material / untracked Content changes

## Operator test (do not claim PASS)

Cold/open normal editor and PIE:

1. Buy Worker — price unchanged from current authored DA; purchase + pod/payload succeed
2. Buy Salvage Walker — same
3. Orbital currency deduction matches displayed/product cost
4. If transport slot capacity is observable in current TEMP UI, confirm same capacity behavior

No DataAsset or Project Settings edits required.

## NOT MERGED

## NOT FINALIZED
