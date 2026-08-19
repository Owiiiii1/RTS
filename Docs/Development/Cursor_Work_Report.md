# Cursor Work Report — Unit Drop Nested Readiness

## Status

**UNIT_DROP_NESTED_READINESS_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-unit-drop-nested-readiness`
- Base: `origin/main` @ `9c4ef72e44fad28d9922d82e8cded1f5d00a473f`
- Head: (this commit)

## Factual root cause

`UGP_OrbitalUnitDropCatalog` marked an authored slot Ready as soon as the top-level `UGP_OrbitalUnitDropDefinition` resolved. Nested `UnitDefinition` and `PayloadClass` used already-loaded-only accessors. On a cold start the product could be Ready while those dependencies were still unloaded, so `ResolveWorkerPayloadClass` / `ResolveSalvageWalkerPayloadClass` could fall through to deprecated settings or native classes, and DropPod could spawn without the product UnitDefinition.

## Changed files

- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropCatalog.h`
- `GP/Source/GPRuntime/Private/Orbital/GPOrbitalUnitDropCatalog.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPOrbitalUnitDropContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPOrbitalUnitDropContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPEconomyLogisticsDataContractTest.cpp`
- `Docs/Development/Claude_Tasks/GP-UnitDrop-Nested-Readiness.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Cursor_Work_Report.md`

## Resulting readiness contract

For a configured authored unit product slot, Ready means all of:

1. Top-level `UGP_OrbitalUnitDropDefinition` loaded
2. `UnitDefinition` soft ref non-null
3. `UnitDefinition` loaded/resolved
4. `PayloadClass` soft ref non-null
5. `PayloadClass` loaded/resolved and a valid slot subclass (`AGP_Worker` or `AGP_SalvageWalker`)

Until then the slot stays Pending. Purchase is `DefinitionNotReady`. No Orbital Ferronite spend, manifest mutation, pod spawn, or cold-load class substitution.

Invalid/failed nested dependency: Failed, explicit log with slot/product/path/reason, native bootstrap, not stuck Pending.

Loads: `AssetManager` / `StreamableManager` async only. Teardown cancels top-level and nested handles. Callbacks ignored when the catalog is shutting down or the engine is exiting. `TryGetExisting()` does not create/resurrect during shutdown.

Unconfigured slots keep native bootstrap.

## Exact payload precedence

1. Canonical Ready authored `UGP_OrbitalUnitDropDefinition.PayloadClass`
2. If canonical product is native/empty-payload (unconfigured or failed authored): deprecated `WorkerPayloadClass` / `SalvageWalkerPayloadClass`
3. Native `AGP_Worker` / `AGP_SalvageWalker`

Deprecated settings classes are not removed. They do not outrank a valid authored product class because it was cold.

## Exact UnitDefinition behavior

Catalog Ready requires the product `UnitDefinition` to be loaded.

`AGP_DropPod` still assigns that product definition onto the spawned unit only when `UnitDefinitionAsset` is empty. If a BP/CDO already has `UnitDefinitionAsset` set, that explicit override is left unchanged (not audit slice H). Conflict: product UnitDefinition vs BP-authored `UnitDefinitionAsset` can still disagree when the actor reference is non-null.

## Tests

| Check | Result |
| --- | --- |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |

Building-drop / Wall Package contracts not run: those catalogs were not changed. Full project suite not run.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** |

GP Win64 Development / Shipping not run (finalization builds after operator PASS).

## Protected-files confirmation

Diff excludes maps, `DefaultGame.ini`, `DefaultEngine.ini`, Blueprints, DataAssets, materials, and other untracked Content.

## Operator test (not claimed PASS)

Cold editor start, do not open Worker/Salvage Walker DropDefinition, UnitDefinition, or payload BP. PIE immediately. Acquire Orbital Ferronite. Buy Worker, then Salvage Walker. Expect authored payload BP and authored UnitDefinition values. Close Editor normally.

## NOT MERGED

## NOT FINALIZED
