# GP-S28P1 — Blueprint-Ready Resource Actors + Cargo Visual Contract

## Status
**GP-S28P1_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
- Branch: `feature/gp-s28p1-blueprint-cargo-visual`
- Base audit: `audit/gp-s28p-resource-playable-pass` @ `377b9b8c28dc09929efbae061a05e351b0dbad3f`
- Implementation: `e196a43e124e4c9fb0b0fe7f56ae299ac61f459a`
- Main: `035c486758059032bb2551520834dd73f8667ef5` (untouched)

## Operator validation (haul loop)
Confirmed: Worker select → RMB ResourceNode Mine → cargo → delivery → unload → return; BP Worker/MainBase children work.

## Correction — UnitDefinition compile warning
Removed unconditional `WarnNoUnitDefinitionAsset` (`eea992a312af2a73400ad4f6d0bece2e82d73bf5`).

## Correction — MainBase Storage validation lifecycle
Lifecycle-aware `ValidateStorageContract` + removed BuildingDefinition warning (`70c8578aa70595f104732548862dc2f554b627c0`).

## Correction — Niagara Mining Effect + Generated Visual Override
Cancels unused MiningAnimationAnchor / primitive mining animation idea.

### Cargo presentation (API stable)
- `OnCargoVisualStateChanged` unchanged (bVisible, FillNormalized, Amount, Capacity).
- Operator: keep container mesh always visible; color via FillNormalized (0 white → partial white-yellow → full green).
- No gradual gameplay mining transfer; no material/BP assets in C++.

### Worker
- `MiningEffectAnchor` under PresentationRoot (with CargoVisualAnchor).
- `OnMiningEffectStateChanged` from `OnMiningStateChanged`; active only while `EGP_MiningState::Mining`.
- NiagaraComponent authored in BP under anchor (Auto Activate false); Activate/Deactivate from event.

### ResourceNode
- `bUseGeneratedPrototypeVisual` (default true) on `AGP_ResourceNode`.
- false → clear generated prototype shapes; authored meshes + CollisionBox remain.
- Operator: `BP_GP_ResourceNode_Ferronite` → Use Generated Prototype Visual = false.

### Tests / build
- Presentation contract extended (mining effect + visual toggle lifecycle).
- GPEditor Win64 Development + UHT — **PASSED**
- PIE suite — operator pending

## Goal
Expose stable Blueprint presentation attach points and cargo/mining visual signals so operator-authored BP children can look playable — without changing Mine command semantics, Storage/Threat, or resource reassignment.

## Delivered

### Worker (`AGP_Worker`)
- `PresentationRoot` → under Capsule
- `CargoVisualAnchor` → under PresentationRoot
- `MiningEffectAnchor` → under PresentationRoot
- Accessors: `GetPresentationRoot`, `GetCargoVisualAnchor`, `GetMiningEffectAnchor`, `GetCargoFillNormalized`, `HasCargoForVisual`
- `OnCargoVisualStateChanged` / `OnMiningEffectStateChanged`
- No C++ StaticMesh/Niagara asset; no replicated presentation bool; no Tick

### MainBase (`AGP_MainBase`)
- `PresentationRoot` / `DropOffVisualAnchor` (unchanged this correction)

### ResourceNode (`AGP_ResourceNode`)
- `GetPresentationRoot()` = CollisionBox
- `GetRemainingNormalized()`
- `bUseGeneratedPrototypeVisual` + setter; visual component respects flag + `VisualSourceMode`

### Tests
- `gp.Resource.RunPresentationContractTest` (coordinator token)
- Included in `gp.Resource.RunS28RegressionSuite` (after Cargo)

## Preserved
- RMB → Mine → haul → drop-off → remine path
- Mining cadence, Cargo amounts/replication, FIFO, depletion, Storage/Threat
- No BP/Niagara/material/map assets committed

## Operator next (PIE)
1. `BP_GP_Worker`: cargo mesh always visible; material from FillNormalized; Niagara under MiningEffectAnchor wired to `OnMiningEffectStateChanged`.
2. `BP_GP_ResourceNode_Ferronite`: **Use Generated Prototype Visual = false**; authored meshes only.
3. Run `gp.Resource.RunPresentationContractTest` → Failures=0; optionally `gp.Resource.RunS28RegressionSuite`.
4. Smoke haul loop still works; Niagara only while Mining (not WaitingForSlot / haul).

## Out of scope (P2+)
Depletion lifecycle, cross-node reassignment, drop-off wait, HUD, launch/Orbital/Score, combat, MiningAnimationAnchor, cycle pulse event.
