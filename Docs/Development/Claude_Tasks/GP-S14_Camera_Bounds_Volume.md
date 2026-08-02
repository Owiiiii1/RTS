# GP-S14 — AGP_CameraBoundsVolume
(Level-Placed Optional Camera Bounds Actor + Minimal CameraPawn Integration)

## Slice Group
Slice 3 — Camera (TDD/13: GP-S14 after GP-S13 CameraPawn; before GP-S15 Input)

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S13 **DONE** — `AGP_CameraPawn`
- GP-S12 **DONE** — `UGP_CameraConfigDataAsset::FallbackBounds`
- TDD/13, TDD/11, ADR-0002, ADR-0006
- Module `GPRuntime`

## Goal
Optional level-placed camera bounds provider + minimal CameraPawn discovery/clamp integration.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S15 until explicitly assigned (do not auto-materialize GP-S15 task file).

### Closed with
- exact class/base: `AGP_CameraBoundsVolume : public AActor`
- exact file paths:
  - `GP/Source/GPRuntime/Public/Camera/GPCameraBoundsVolume.h`
  - `GP/Source/GPRuntime/Private/Camera/GPCameraBoundsVolume.cpp`
  - `GP/Source/GPRuntime/Public/Camera/GPCameraPawn.h` (minimal)
  - `GP/Source/GPRuntime/Private/Camera/GPCameraPawn.cpp` (minimal)
- BoundsBox as root (`UBoxComponent`)
- constructor settings: NoCollision; overlap disabled; navigation disabled; Static; HiddenInGame; `SetCanBeDamaged(false)`
- default BoxExtent `(50000, 50000, 3000)`
- non-replicated / no Tick / no RPC
- collision / nav / overlap disabled
- Static + HiddenInGame
- axis-aligned policy (rotation > 0.1° = Error)
- Editor validation: extent / scale / rotation / FBox Errors only
- exact world bounds API: `CalcBounds(GetComponentTransform()).GetBox()`
- CameraPawn integration included
- `TActorIterator` discovery (one-time BeginPlay)
- weak cache (`TWeakObjectPtr`)
- zero/one/multiple semantics: 0 → FallbackBounds; 1 → volume; >1 → first + Warning
- fallback hierarchy: valid volume → volume; else Config FallbackBounds
- invalid FBox warning guard (`bInvalidCameraBoundsWarningLogged`)
- XYZ clamp retained
- no changes to pan/zoom/rotate/input math
- Build.cs unchanged
- three builds **PASSED**
- Class Viewer **PASSED**
- component/settings **PASSED**
- PIE **PASSED**
- GP-S14 errors **ABSENT**
- temporary actor deleted
- no permanent map/assets/input/config changes
- full live camera clamp validation deferred to GP-S15
- GP-S15 not started

---

## Tech-lead locks (OD-1…OD-33) — RESOLVED + IMPLEMENTED

All OD-1…OD-33 locks from SPEC_READY are implemented as closed above.

---

## Acceptance Criteria
- [x] Compiles (GPEditor Dev, GP Dev, GP Shipping) — PASSED
- [x] BoundsVolume class/settings/validation/getter
- [x] CameraPawn discovery + resolve + XYZ clamp
- [x] No Input / GameMode / MatchAssetLoader / Config DA / Build.cs / committed maps/assets
- [x] Editor / module load (operator) — PASSED
- [x] Class Viewer finds `GP_CameraBoundsVolume` (operator) — PASSED
- [x] BoundsBox root / default extent / collision / Static / HiddenInGame (operator) — PASSED
- [x] Extent editing (operator) — PASSED
- [x] PIE (operator) — PASSED
- [x] GP-S14 related errors — ABSENT
- [x] Temporary actor deleted; map not saved
- [x] No tracked Content/map changes
- [x] Full live camera clamp validation — **deferred** to GP-S15 (accepted; not a blocker)
- [x] Tech lead accepted
- [x] Operator accepted
- [x] GP-S15 not started

## Manual Editor validation (operator) — PASSED
1. Editor / module load — OK
2. Class Viewer → `GP_CameraBoundsVolume` — found
3. BoundsBox root — OK
4. Default extent 50000/50000/3000 — OK
5. Collision/overlap/Static/HiddenInGame — OK
6. Extent editing — OK
7. PIE — OK; GP-S14 errors ABSENT
8. Temporary actor deleted; map not saved
9. No tracked Content/map changes

### Deferred (accepted, not a blocker)
Full live camera clamp validation deferred until GP-S15 input wiring.

## Build results
| Target | Result |
| --- | --- |
| GPEditor Win64 Development | PASSED |
| GP Win64 Development | PASSED |
| GP Win64 Shipping | PASSED |

## Out of Scope (confirmed)
Input / IMC / PC binding / GameMode DefaultPawn / committed maps / Blueprint / Config instance / PrimaryAsset / multi-volume priority / oriented bounds / tests / GP-S15

## Linked canonical docs
TDD/13, TDD/11, GP-S13, ADR-0002, ADR-0006.

## Stop Condition
GP-S14 closed as DONE. Do **not** start GP-S15. Do **not** auto-materialize the next task file.
