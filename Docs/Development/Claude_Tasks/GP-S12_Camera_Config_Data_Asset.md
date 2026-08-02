# GP-S12 — UGP_CameraConfigDataAsset
(RTS Camera Tuning Schema)

## Slice Group
Slice 2 — Match Flow + Asset Loader → Camera foundation (TDD/13: GP-S12 before GP-S13 CameraPawn)

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S11 DONE (prior stage; not a hard code dependency).
- ADR-0002 / ADR-0006 / TDD/11 / TDD/10.
- Module `GPRuntime`.
- Future consumer: GP-S13 `AGP_CameraPawn` — **out of S12**.

## Goal
Implement one immutable designer-tunable `UGP_CameraConfigDataAsset : UPrimaryDataAsset` that holds the full TDD/11 RTS camera tuning schema, Editor metadata, and Editor `IsDataValid` — without runtime camera logic, CameraPawn, Input assets, AssetManager registration, or committed `.uasset` instances.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S13 until explicitly assigned (do not auto-materialize GP-S13 task file).

### Closed with
- `UGP_CameraConfigDataAsset : UPrimaryDataAsset`
- exact 19 fields/defaults
- BlueprintType
- public EditDefaultsOnly BlueprintReadOnly fields
- inline defaults
- no constructor
- Editor-only IsDataValid
- strict `MinArmLength < DefaultArmLength < MaxArmLength`
- exact validation errors
- two warnings only
- FBox bounds validation
- actual UE 5.8 include: `Engine/DataAsset.h`
- no GetPrimaryAssetId override
- no PrimaryAsset config
- no AssetManager settings
- no `.uasset`
- no CameraPawn
- no CameraBoundsVolume
- no PlayerController/Input changes
- no MatchAssetLoader changes
- no Build.cs changes
- no config/maps/assets/tests
- three builds **PASSED**
- Class Viewer **PASSED**
- Data Asset picker **PASSED**
- PIE **PASSED**
- GP-S12 errors **ABSENT**
- unrelated render-thread warnings not part of this slice
- GP-S13 not started

### Files
- `GP/Source/GPRuntime/Public/Camera/GPCameraConfigDataAsset.h`
- `GP/Source/GPRuntime/Private/Camera/GPCameraConfigDataAsset.cpp`

---

## Tech-lead locks (OD-1…OD-26) — RESOLVED + IMPLEMENTED

All OD-1…OD-26 locks from SPEC_READY are implemented as closed above.

---

## Acceptance Criteria
- [x] Compiles (GPEditor Dev, GP Dev, GP Shipping) — PASSED
- [x] Exact 19 fields + defaults + categories + metadata
- [x] Editor `IsDataValid` with OD-10/11 rules; Super first
- [x] No GetPrimaryAssetId override; no AssetManager/config; no committed `.uasset`
- [x] No CameraPawn/Input/MatchAssetLoader/Build.cs changes
- [x] Editor / module load (operator) — PASSED
- [x] `GP_CameraConfigDataAsset` found in Class Viewer (operator) — PASSED
- [x] `GP_CameraConfigDataAsset` found in Data Asset picker (operator) — PASSED
- [x] PIE (operator) — PASSED
- [x] GP-S12 related errors — ABSENT
- [x] No permanent Data Asset instance created
- [x] Tech lead accepted
- [x] Operator accepted
- [x] PrimaryAsset registration — **deferred**
- [x] CameraPawn soft-ref integration — **deferred** to later slice
- [x] GP-S13 not started

## Manual Editor validation (operator) — PASSED
1. Module load — OK  
2. Class Viewer → `GP_CameraConfigDataAsset` — found  
3. Data Asset picker — found  
4. No permanent `.uasset` created  
5. Normal PIE — OK  
6. GP-S12 errors ABSENT  

### Unrelated observations (not part of GP-S12)
Output Log render-thread warnings observed for `grass.GrassMap.UseRuntimeGeneration`, `grass.Enable`, `r.MotionVectorSimulation`. Classified as **unrelated engine/render warnings**; not GP-S12 defects; not fixed in this slice.

## Build results
| Target | Result |
| --- | --- |
| GPEditor Win64 Development | PASSED |
| GP Win64 Development | PASSED |
| GP Win64 Shipping | PASSED |

## Out of Scope (confirmed)
- CameraPawn / CameraBoundsVolume / Enhanced Input / IA / IMC  
- Camera math / MatchAssetLoader / PrimaryAsset registration / AssetManager config  
- Committed `.uasset` / maps / UI / networking / automation tests  
- GP-S13+

## Linked canonical docs
TDD/13, TDD/11, TDD/10, ADR-0002, ADR-0006, GP-S10, GP-S11.

## Stop Condition
GP-S12 closed as DONE. Do **not** start GP-S13. Do **not** auto-materialize the next task file.
