# Cursor Work Report — Unit Payload Compatibility Cleanup

## Status

**UNIT_PAYLOAD_COMPAT_CLEANUP_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-unit-payload-compat-cleanup`
- Base: `origin/main` @ `47a220b480e455f1cf5dfb6ca0613c13cf760a53`
- Head: (this commit)

## Pre-change reference classification

Repository-wide search of `WorkerPayloadClass` / `SalvageWalkerPayloadClass` and the related resolvers before C++ removal:

| Kind | Path | Notes |
| --- | --- | --- |
| UPROPERTY declaration | `GPOrbitalDeliverySettings.h` | Hidden Config soft-class bridges |
| Settings helper / resolver | `GPOrbitalDeliverySettings.cpp` | `ResolveWorkerPayloadClass` / `ResolveSalvageWalkerPayloadClass` via `TryLoadSoftSubclass` (`LoadSynchronous`) |
| Invalid-config helpers | same | `IsWorkerPayloadClassConfigInvalid` / `IsSalvageWalkerPayloadClassConfigInvalid` |
| Catalog fallback | `GPOrbitalUnitDropCatalog::ResolveFallbackPayloadClass` | Delegated to settings resolvers, then native C++ class |
| Catalog public resolvers | `ResolveWorkerPayloadClass` / `ResolveSalvageWalkerPayloadClass` | Authored Ready drop PayloadClass, else settings fallback |
| DropPod / spawn reader | `GPDropPod.cpp` | Already used catalog resolvers, not settings |
| Visibility contract | `GPOrbitalDeliveryVisibilityContractTest.cpp` | Hidden/deprecated + type + smoke resolve |
| Unit-drop contract | `GPOrbitalUnitDropContractTest.cpp` | Saved/restored settings payload; stub spawn via settings bridges |
| UnitCap contract | `GPUnitCapLogisticsHubContractTest.cpp` | `WorkerPayloadClass.Reset()` / `SalvageWalkerPayloadClass.Reset()` isolation only |
| Economy contract | no settings payload reader | Authored drop `PayloadClass` assignment only |
| Config text | `GP/Config/DefaultGame.ini` | `WorkerPayloadClass=...BP_GP_Worker_C`, `SalvageWalkerPayloadClass=...` |
| Docs | audit / prior slice notes | DEPRECATED_ACTIVE sync-load bridge |
| Production GConfig / string lookup | **none** in `GP/Source` | |

No production dependency outside unit-drop payload / catalog / spawn. UnitCap used settings only as test isolation.

## Removed settings fields

Completely removed from `UGP_OrbitalDeliverySettings`:

- `WorkerPayloadClass`
- `SalvageWalkerPayloadClass`

## Removed settings helper APIs

- `ResolveWorkerPayloadClass(...)`
- `ResolveSalvageWalkerPayloadClass(...)`
- `IsWorkerPayloadClassConfigInvalid()`
- `IsSalvageWalkerPayloadClassConfigInvalid()`

`TryLoadSoftSubclass` remains for `UnitDropPodClass`, `BuildingPayloadClass`, and `DefensiveTurretPayloadClass`.

## Canonical authored payload ownership

`UGP_OrbitalUnitDropDefinition::PayloadClass`

- Worker slot: must resolve to `AGP_Worker` subclass
- Salvage Walker slot: must resolve to `AGP_SalvageWalker` subclass
- Nested readiness unchanged: top-level drop + UnitDefinition + PayloadClass loaded and slot-valid before Ready
- Pending nested PayloadClass: `DefinitionNotReady`, resolver returns empty (no native substitution for purchase)

## Native Worker / Walker fallback ownership

`UGP_OrbitalUnitDropCatalog::EnsureNativeCatalog()` sets native product `PayloadClass`, and `ResolveFallbackPayloadClass` returns:

- Worker: `AGP_Worker::StaticClass()`
- Salvage Walker: `AGP_SalvageWalker::StaticClass()`

Not configurable through Project Settings.

## Nested async readiness confirmation

Configured authored slot stays Pending while nested PayloadClass is cold. Purchase remains `DefinitionNotReady`. Invalid/null authored PayloadClass still fails through to native bootstrap with existing diagnostics.

## No unit payload LoadSynchronous path remains

Unit product payload resolution no longer reaches `UGP_OrbitalDeliverySettings` `TryLoadSoftSubclass`. Catalog payload readiness remains async. `GP/Source/.../Orbital` has no `LoadSynchronous`.

## DropPod source after migration

`AGP_DropPod` still resolves payload from `UGP_OrbitalUnitDropCatalog::ResolveWorkerPayloadClass` / `ResolveSalvageWalkerPayloadClass`. It does not read Project Settings.

## UnitDefinitionAsset semantics unchanged

When spawned actor `UnitDefinitionAsset` is empty, catalog product UnitDefinition may still be assigned. Explicit BP/CDO override remains unchanged.

## Stale DefaultGame.ini keys untouched

Local/committed INI may still contain `WorkerPayloadClass` / `SalvageWalkerPayloadClass`. Intentionally not edited. After C++ removal they cannot populate runtime fields. No GConfig/string reader was added.

## Exact production files changed

- `GP/Source/GPRuntime/Public/Settings/GPOrbitalDeliverySettings.h`
- `GP/Source/GPRuntime/Private/Settings/GPOrbitalDeliverySettings.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropCatalog.h`
- `GP/Source/GPRuntime/Private/Orbital/GPOrbitalUnitDropCatalog.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalUnitDropContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalDeliveryVisibilityContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPUnitCapLogisticsHubContractTest.cpp`

Docs:

- `Docs/Development/Configuration_Data_Ownership_Audit.md` (narrow Slice D mark)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Claude_Tasks/GP-Unit-Payload-Compat-Cleanup.md`
- `Docs/Development/Cursor_Work_Report.md`

## Tests / results

| Check | Result |
| --- | --- |
| `gp.Settings.RunOrbitalDeliveryVisibilityContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunUnitCapLogisticsHubContractTest` | `Complete Failures=0 Cancelled=false` |

Full suite not run. Building / Wall Package contracts not run (shared production code outside unit payload ownership was not changed).

## GPEditor / UHT result

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |
| `GP Win64 Development` | not run (finalization after operator PASS) |
| `GP Win64 Shipping` | not run (finalization after operator PASS) |

## Protected-files confirmation

Committed diff vs `origin/main` @ `47a220b…` is unit-payload C++ ownership, targeted contracts, and docs only:

- no maps
- no `DefaultGame.ini`
- no `DefaultEngine.ini`
- no Blueprint / DataAsset / material / untracked Content changes
- no numeric / timing / building procurement / Wall Package production changes

## Operator test (do not claim PASS)

Cold/open normal editor and PIE with existing authored DataAssets:

1. Buy Worker — expected authored Worker BP/class arrives
2. Buy Salvage Walker — expected authored Salvage Walker BP/class arrives
3. Visuals/behavior unchanged
4. Prices / currency / transport behavior unchanged

No Project Settings or DataAsset edits required.

## NOT MERGED

## NOT FINALIZED
