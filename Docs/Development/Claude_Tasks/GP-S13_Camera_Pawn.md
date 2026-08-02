# GP-S13 — AGP_CameraPawn
(Local RTS Camera Movement)

## Slice Group
Slice 3 — Camera (TDD/13: GP-S13 after GP-S12 Config; before GP-S14 BoundsVolume / GP-S15 Input)

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S12 **DONE** — `UGP_CameraConfigDataAsset`
- TDD/13, TDD/11, ADR-0002, ADR-0006
- Module `GPRuntime`

## Goal
Local RTS camera pawn: pan / edge-scroll / zoom / pitch / yaw; soft-ref config + async load; CDO fallback; no replication / RPC / GAS / PC input wiring.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S14 until explicitly assigned (do not auto-materialize GP-S14 task file).

### Closed with
- exact class/base: `AGP_CameraPawn : public APawn`
- exact C++ file paths:
  - `GP/Source/GPRuntime/Public/Camera/GPCameraPawn.h`
  - `GP/Source/GPRuntime/Private/Camera/GPCameraPawn.cpp`
- component hierarchy: RootScene → SpringArm → Camera
- constructor component settings: SpringArm collision/lag/rotation-lag off; no pawn control rotation on SpringArm/Camera
- provisional arm 2500 / pitch −45
- `bReplicates = false`
- `SetReplicateMovement(false)`
- local-control Tick guard (`IsLocallyControlled()` + `ResetFrameInput` early-out)
- `ConfigRef` soft reference (`TSoftObjectPtr<UGP_CameraConfigDataAsset>`)
- CDO fallback via `GetDefault<UGP_CameraConfigDataAsset>()`
- self StreamableManager async loading (`UAssetManager::GetStreamableManager`)
- no MatchAssetLoader integration
- one `TSharedPtr<FStreamableHandle>`
- `FStreamableDelegate::CreateUObject` callback
- EndPlay cancellation/reset of active handle
- BeginPlay initialization sequence (CDO → yaw/arm/pitch/clamp → optional async)
- public C++ input API: `SetPanInput` / `AddZoomInput` / `AddRotateInput` / `SetRotateActive`
- exact Tick order: Super → guard → config → edge → zoom → pitch → rotation → pan → clamp → reset
- edge threshold/falloff formula
- pan smoothing with `FMath::Vector2DInterpTo`
- yaw-relative XY pan
- zoom sign (positive wheel zooms in) and `FInterpTo`
- pitch Lerp formula (`PitchAtMinZoom` → `PitchAtMaxZoom` by zoom fraction)
- RootScene relative yaw
- XYZ `FallbackBounds` clamp
- no BlueprintCallable
- no `SetupPlayerInputComponent`
- no replication/RPC
- no Build.cs changes
- actual UE 5.8 forward declaration: `struct FStreamableHandle`
- explicit destructor required for incomplete `FStreamableHandle` type
- three builds **PASSED**
- Class Viewer **PASSED**
- component hierarchy/settings **PASSED**
- PIE **PASSED**
- GP-S13 errors **ABSENT**
- temporary Blueprint deleted
- no permanent assets/maps/config/input
- full live movement validation deferred to PlayerController/Input wiring
- GP-S14 not started

---

## Tech-lead locks (OD-1…OD-40) — RESOLVED + IMPLEMENTED

All OD-1…OD-40 locks from SPEC_READY are implemented as closed above.

---

## Acceptance Criteria
- [x] Compiles (GPEditor Dev, GP Dev, GP Shipping) — PASSED
- [x] Class/base + component hierarchy/settings per OD-1…OD-4
- [x] Non-replicated + local-control Tick guard
- [x] CDO fallback + self async load + handle/EndPlay
- [x] Input API + exact Tick order + pan/edge/zoom/pitch/rotation/bounds math
- [x] No PC / GameMode / MatchAssetLoader / Config DA / Build.cs / config / maps / committed `.uasset` / Input
- [x] Editor / module load (operator) — PASSED
- [x] Class Viewer finds `GP_CameraPawn` (operator) — PASSED
- [x] Component hierarchy RootScene → SpringArm → Camera (operator) — PASSED
- [x] SpringArm settings (operator) — PASSED
- [x] Camera settings (operator) — PASSED
- [x] PIE (operator) — PASSED
- [x] GP-S13 related errors — ABSENT
- [x] Temporary Blueprint deleted
- [x] No tracked Content/map/config changes
- [x] Full live pan/zoom/rotate/edge-scroll validation — **deferred** to PC/Input wiring (accepted; not a blocker)
- [x] Tech lead accepted
- [x] Operator accepted
- [x] GP-S14 not started

## Manual Editor validation (operator) — PASSED
1. Editor / module load — OK
2. Class Viewer → `GP_CameraPawn` — found
3. Component hierarchy RootScene → SpringArm → Camera — OK
4. SpringArm settings — OK
5. Camera settings — OK
6. Temporary Blueprint deleted
7. Normal PIE — OK
8. GP-S13 errors ABSENT
9. No tracked Content/map/config changes

### Deferred (accepted, not a blocker)
Full live pan / zoom / rotate / edge-scroll proof deferred until PlayerController / Enhanced Input wiring (out of GP-S13 scope).

## Build results
| Target | Result |
| --- | --- |
| GPEditor Win64 Development | PASSED |
| GP Win64 Development | PASSED |
| GP Win64 Shipping | PASSED |

## Out of Scope (confirmed)
- PlayerController / Enhanced Input / IA / IMC
- GameMode DefaultPawnClass / possession wiring
- CameraBoundsVolume / map config / committed `.uasset`
- MatchAssetLoader / PrimaryAsset registration
- Selection/commands / UI edge suppression
- Replication/RPC / cinematic / shake / tests
- GP-S14+

## Linked canonical docs
TDD/13, TDD/11, TDD/10, ADR-0002, ADR-0006, GP-S12 task.

## Stop Condition
GP-S13 closed as DONE. Do **not** start GP-S14. Do **not** auto-materialize the next task file.
