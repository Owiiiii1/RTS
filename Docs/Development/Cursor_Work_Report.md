# Cursor Work Report — Unit Drop Nested Readiness

## Status

**UNIT_DROP_NESTED_READINESS_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch

`feature/gp-unit-drop-nested-readiness`

## Operator gameplay validation (already PASS — do not repeat unless unit-drop behavior changes)

- Worker cold-start authored payload / UnitDefinition: **PASS**
- Salvage Walker cold-start authored payload / UnitDefinition: **PASS**

## Operator editor-close validation: FAIL (original)

Fatal:

`Object is not packaged: GP_BuildingDropCatalog GP_BuildingDropCatalog`

Stack:

- `UGP_BuildingDropCatalog::Get()`
- `UGP_OrbitalBuildingDropContractTestRunner::RestoreSettings()`
- `UGP_OrbitalBuildingDropContractTestRunner::BeginDestroy()`

This is a factual teardown defect discovered during validation. **Do not mark operator PASS yet.**

Required operator re-test after this correction (editor close only):

- Open Editor
- Optionally PIE once
- Close Editor normally
- Expect: no fatal / no `Object is not packaged: GP_BuildingDropCatalog`

## Factual root cause

`UGP_BuildingDropCatalog::Get()` recovered/created the transient catalog via `FindObject` / `NewObject` whenever `GCatalog` was invalid. `ShutdownCatalog()` reset `GCatalog` with no engine-exit creation lock.

Contract runner `BeginDestroy()` → `RestoreSettings()` always called creating `Get()` for `OverrideDeliveryTiming` and `DebugEndContractIsolation`.

UObject destruction during editor shutdown therefore recreated `GP_BuildingDropCatalog` after package teardown had begun.

## BuildingDropCatalog lifecycle correction

Narrow production-safe lifecycle, matching `UGP_WallPackageCatalog` / `UGP_OrbitalUnitDropCatalog` principles. No change to building purchase, nested BuildingDefinition readiness, payload precedence, unit-drop readiness, or Wall Package behavior.

- `TryGetExisting()` returns the live catalog or `nullptr`. Never creates, refreshes, or syncs.
- `Get()` does not create a transient catalog while shutdown is active, engine pre-exit has locked creation, or `IsEngineExitRequested()`. If creation is blocked and no live catalog exists, `Get()` returns the packaged CDO (`GetMutableDefault`) and skips refresh/mutation of a transient object.
- `ShutdownCatalog()` is idempotent: marks shutdown first, cancels top-level and nested building-definition handles, resets strong ownership, does not recreate.
- Engine PreExit (`NotifyEngineShutdown`) locks future creation before shutdown.
- Async load callbacks ignore completion when the catalog is not the live instance or creation is blocked.

Contract runner teardown never calls creating `Get()`:

- Ordinary active-test cleanup may restore mutable settings.
- Catalog-specific cleanup uses `TryGetExisting()`.
- If the catalog no longer exists, do not create it merely to `OverrideDeliveryTiming` or `DebugEndContractIsolation`.
- `BeginDestroy` is non-creating and cleanup is idempotent.
- Normal completed-test `Finish()` still restores isolation/settings deterministically while the catalog exists.

Coordinator `Release()` also uses `TryGetExisting()` so contract finish during teardown cannot resurrect catalogs.

## Exact files changed

- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropCatalog.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropCatalog.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalBuildingDropContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalBuildingDropContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPContractTestCoordinator.cpp`
- `GP/Source/GPRuntime/Private/GPRuntime.cpp`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/Development/AI_Project_Log.md`

## Tests / results

| Check | Result |
| --- | --- |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |

Building-drop contract now includes teardown coverage:

1. `TryGetExisting()` with no catalog → `nullptr`, does not create
2. Normal `Get()` before shutdown works
3. Contract cleanup during normal execution restores settings/isolation
4. Shutdown cancels handles and releases the catalog
5. BeginDestroy-style cleanup after catalog shutdown does not call creating `Get()` and does not resurrect
6. Callback after shutdown is ignored safely

Wall Package contract not run: shared Wall Package lifecycle code was not changed. Full regression suite not run.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |

GP Win64 Development / Shipping not run.

## Protected-files confirmation

Diff excludes maps, `DefaultGame.ini`, `DefaultEngine.ini`, Blueprints, DataAssets, materials, and other untracked Content.

## NOT MERGED

## NOT FINALIZED
