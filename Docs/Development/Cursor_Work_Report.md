# Cursor Work Report — Unit Drop Nested Readiness

## Status

**UNIT_DROP_NESTED_READINESS_FINALIZED_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `feature/gp-unit-drop-nested-readiness`
- Base: `origin/main` @ `9c4ef72e44fad28d9922d82e8cded1f5d00a473f`
- Head: (this commit)

Implementation commits: `7aba363` (nested unit-drop readiness), `5c92fdd` (BuildingDropCatalog teardown). This commit is docs/status only.

## Operator PASS summary

- Worker cold-start authored payload / UnitDefinition: **PASS**
- Salvage Walker cold-start authored payload / UnitDefinition: **PASS**
- Editor close after BuildingDropCatalog lifecycle correction: **PASS**
- No fatal
- No `Object is not packaged: GP_BuildingDropCatalog`

## Final readiness contract

For a configured authored unit product slot, Ready requires all of:

1. Top-level `UGP_OrbitalUnitDropDefinition` loaded
2. Nested `UnitDefinition` non-null and loaded
3. Nested `PayloadClass` non-null, loaded, and a valid slot subclass (`AGP_Worker` / `AGP_SalvageWalker`)

Until then the slot stays Pending. Purchase is `DefinitionNotReady`. No Orbital Ferronite spend, manifest mutation, pod spawn, or cold-load class substitution.

Authored product cold state cannot fall through to deprecated `WorkerPayloadClass` / `SalvageWalkerPayloadClass` merely because nested assets are unloaded. Canonical slot is `nullptr` while Pending; payload resolve uses the canonical product only when Ready.

Unconfigured or failed authored slots keep native bootstrap. Deprecated settings payload classes remain the fallback only for native/empty-payload products. Deprecated settings were not removed.

`AGP_DropPod` still assigns product `UnitDefinition` only when `UnitDefinitionAsset` is empty.

## BuildingDropCatalog shutdown correction

`Get()` no longer recreates a transient `GP_BuildingDropCatalog` during engine exit or catalog shutdown.

- `TryGetExisting()` never creates
- PreExit locks creation, then shuts down
- `ShutdownCatalog()` is idempotent and cancels top-level and nested handles
- Contract runner `BeginDestroy` / `RestoreSettings` uses `TryGetExisting()` only (non-creating)
- Coordinator `Release()` also uses `TryGetExisting()`
- If `Get()` must return while creation is blocked, it uses the packaged CDO, not a transient resurrection

Wall Package behavior was not changed. Wall Package inventory contract was run only as a shutdown/catalog regression cross-check.

## Final contract tests / results

| Check | Result |
| --- | --- |
| `gp.Resource.RunOrbitalUnitDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Economy.RunEconomyLogisticsDataContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Building.RunOrbitalBuildingDropContractTest` | `Complete Failures=0 Cancelled=false` |
| `gp.Orbital.RunWallPackageInventoryContractTest` | `Complete Failures=0 Cancelled=false` |

Full project suite not run. No new shared regression appeared.

## Builds

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **PASS** (up to date) |
| `GP Win64 Development` | **PASS** |
| `GP Win64 Shipping` | **PASS** |

No Shipping stub fixes required.

## Factual protected-files check

Committed diff vs `origin/main` @ `9c4ef72e44fad28d9922d82e8cded1f5d00a473f` is C++ + docs only:

- no maps
- no `DefaultGame.ini`
- no `DefaultEngine.ini`
- no Blueprint / DataAsset / material / content changes
- no unrelated ownership cleanup
- no deprecated settings removed
- Wall Package runtime behavior unchanged (catalog/authority/inventory files not in this branch diff)

Local untracked Content and local config/map dirt were left unstaged.

## Finalization scope

No new functionality in this finalization. Docs/status only after operator PASS and the validation above.

## NOT MERGED
