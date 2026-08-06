# GP-S28P1 — Blueprint-Ready Resource Actors + Cargo Visual Contract

## Status
**GP-S28P1_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
- Branch: `feature/gp-s28p1-blueprint-cargo-visual`
- Base audit: `audit/gp-s28p-resource-playable-pass` @ `377b9b8c28dc09929efbae061a05e351b0dbad3f`
- Implementation: `e196a43e124e4c9fb0b0fe7f56ae299ac61f459a`
- Main: `035c486758059032bb2551520834dd73f8667ef5` (untouched)

## Correction — UnitDefinition compile warning
Operator blocker: BP_GP_Worker Compile logged unconditional `WarnNoUnitDefinitionAsset` (“No UGP_UnitDefinition Worker asset…”).

Fix: removed that warning from `ValidateWorkerContract` (`eea992a312af2a73400ad4f6d0bece2e82d73bf5`). Worker UnitDefinition remains deferred — **no** placeholder asset / property. Presentation contract now asserts absence of UnitDefinition warnings. DataValidation on local BP_GP_Worker: 0 errors, 0 warnings.

## Correction — MainBase Storage validation lifecycle
Operator blocker: BP_GP_MainBase Compile — `Containers array size must equal ContainerCount` plus BuildingDefinition warning.

Root cause: `EnsureContainerArray()` is authority-only in `BeginPlay`, but DataValidation runs on templates with empty `Containers`.

Fix:
- `ValidateStorageContract` treats empty Containers on template/CDO/pre-BeginPlay (non-authority-initialized) as initialization-pending; empty after authority BeginPlay still errors; non-empty size≠Count always errors.
- Removed unconditional BuildingDefinition warning from `ValidateMainBaseContract`.
- Storage contract + presentation contract cover template / runtime / partial-array / no BuildingDefinition warning.
- Local BP_GP_MainBase DataValidation: **0 errors, 0 warnings** (asset not committed).
- Storage gameplay (5×100, add/remove, LOST, replication) unchanged.

## Goal
Expose stable Blueprint presentation attach points and a cargo visual signal so operator-authored BP children can look playable — without changing Mine command semantics, Storage/Threat, or resource reassignment.

## Delivered

### Worker (`AGP_Worker`)
- `PresentationRoot` → under Capsule
- `CargoVisualAnchor` → under PresentationRoot
- Accessors: `GetPresentationRoot`, `GetCargoVisualAnchor`, `GetCargoFillNormalized`, `HasCargoForVisual`
- `OnCargoVisualStateChanged` (`BlueprintAssignable`) synced from `UGP_CargoComponent::OnCargoAmountChanged` + BeginPlay initial sync
- No C++ StaticMesh; no new replicated cargo flag; no Tick

### MainBase (`AGP_MainBase`)
- `PresentationRoot` → under Capsule
- `DropOffVisualAnchor` → under PresentationRoot (presentation only; `DropOffRangeCm` unchanged)
- Accessors: `GetPresentationRoot`, `GetDropOffVisualAnchor`, `GetStorageComponent`, `GetPlanetaryStored`, `GetPlanetaryCapacity`

### ResourceNode (`AGP_ResourceNode`)
- No duplicate hierarchy — `GetPresentationRoot()` returns CollisionBox (root / AuthoredComponents parent)
- `GetRemainingNormalized()` added
- Existing `GetCurrentAmount` / `GetMaxAmount` / `IsDepleted` unchanged
- `OnResourceDepleted` deferred to P2

### Tests
- `gp.Resource.RunPresentationContractTest` (coordinator token)
- Included in `gp.Resource.RunS28RegressionSuite` (after Cargo)

### Builds
- GPEditor Win64 Development + UHT — **PASSED**

## Preserved
- RMB → Mine → haul → drop-off → remine path
- CommandComponent / Server_RequestCommand / Mine semantics
- FIFO, slots, depletion, Storage LOST, Threat, registry
- No BP/map/content assets created

## Operator next (PIE)
1. Create `BP_GP_Worker : AGP_Worker` — add StaticMesh under PresentationRoot; cargo mesh on CargoVisualAnchor; bind visibility to `OnCargoVisualStateChanged`.
2. Create `BP_GP_MainBase : AGP_MainBase` — meshes under PresentationRoot; optional marker at DropOffVisualAnchor.
3. Create `BP_GP_ResourceNode_Ferronite : AGP_ResourceNode` — AuthoredComponents / SCS under CollisionBox; visuals NoCollision / no nav.
4. Keep CapabilityTags; do not replace Capsule/Box roots.
5. Run `gp.Resource.RunPresentationContractTest` → Failures=0.
6. Smoke: select Worker → RMB node → haul loop still works.

## Out of scope (P2+)
Depletion lifecycle, cross-node reassignment, drop-off wait, HUD, launch/Orbital/Score, combat.
