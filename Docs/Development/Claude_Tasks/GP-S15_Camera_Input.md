# GP-S15 — Camera Enhanced Input and PlayerController Binding
(Working RTS Camera in PIE)

## Slice Group
Slice 3 — Camera (TDD/13: GP-S15 after GP-S14 BoundsVolume; before GP-S16 Selection)

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

**This pass:** closed DONE — C++ + `DefaultGame.ini` + five Input `.uasset` + operator PIE validation.

## Asset Changes Allowed
Yes — after explicit implementation assignment. Operator creates five Enhanced Input `.uasset`s at locked paths (Phase B). Cursor does not write raw binary assets.

## Depends On
- GP-S13 **DONE** — `AGP_CameraPawn` intent API + CDO config fallback
- GP-S14 **DONE** — bounds resolve/clamp
- TDD/11, TDD/13, ADR-0006
- `GPRuntime` already depends on EnhancedInput; plugin enabled; `DefaultInput.ini` already Enhanced + permanent capture

## Goal
After GP-S15, PIE provides a really controllable RTS camera:

- WASD + arrows → pan
- mouse wheel → zoom (wheel up = zoom in)
- MMB held + Mouse X → rotate
- edge-scroll works
- CameraPawn uses BoundsVolume or FallbackBounds
- PlayerController forwards input only
- CameraPawn owns all camera math
- Standalone + 2-player listen-server PIE required

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S16 until explicitly assigned (do not auto-materialize GP-S16 task file).

### Closed with
- exact five asset paths:
  - `GP/Content/GrimProtocol/Input/Camera/IA_Camera_Pan.uasset` (Axis2D)
  - `GP/Content/GrimProtocol/Input/Camera/IA_Camera_Zoom.uasset` (Axis1D)
  - `GP/Content/GrimProtocol/Input/Camera/IA_Camera_Rotate.uasset` (Axis1D)
  - `GP/Content/GrimProtocol/Input/Camera/IA_Camera_RotateToggle.uasset` (Bool)
  - `GP/Content/GrimProtocol/Input/Camera/IMC_GP_Camera.uasset`
- action value types: Pan Axis2D; Zoom Axis1D; Rotate Axis1D; RotateToggle Bool
- WASD / arrows pan mappings + modifiers (Swizzle YXZ; Negate as locked)
- Mouse Wheel Axis → Zoom (wheel up = zoom in; wheel down = zoom out)
- Mouse X → Rotate; MMB → RotateToggle (Started/Completed/Canceled)
- mapping priority `100`
- pure C++ `AGP_PlayerController` (no BP_PC)
- soft canonical asset references + one-time `LoadSynchronous` + transient resolved `TObjectPtr`s
- action bindings in `SetupInputComponent`; IMC add in local `BeginPlayingState`; IMC remove in `EndPlay`
- cursor visible; `FInputModeGameAndUI`; `HideCursorDuringCapture=false`; `LockAlways`
- CameraPawn owner-only replicated shell (`bReplicates=true`, `bOnlyRelevantToOwner=true`); camera transform/state **not** replicated; no movement replication; no CameraPawn RPC
- GameMode `DefaultPawnClass = AGP_CameraPawn`; `GlobalDefaultGameMode=/Script/GPRuntime.GP_GameMode`
- standard PlayerController tick enabled (`PrimaryActorTick.bCanEverTick = true`); **no** custom `PlayerTick`
- diagnosis: disabled PC tick left IMC/bindings initialized but prevented Enhanced Input action handlers from firing
- CameraPawn owns all camera math; PC forwards only
- Build.cs unchanged; DefaultInput.ini unchanged
- GPEditor Development / GP Development / GP Shipping **PASSED**
- Standalone PIE **PASSED**; 2-player listen-server PIE **PASSED**; independent cameras **PASSED**
- bounds clamp (fallback + temporary BoundsVolume) **PASSED**; temp actor deleted; no permanent map changes
- missing IA/IMC errors **ABSENT**; possession/network errors **ABSENT**
- asset rename incident: initially created without required underscores; canonical names restored; Editor restart/reload required after rename/redirector cleanup; final committed assets use canonical names only
- temporary GP-S15 DIAG code removed (not in production)
- GP-S16 not started

---

## Canonical scope (GP-S15)

1. Five Input assets at locked paths (operator-created)
2. `AGP_PlayerController` Enhanced Input bind + forward + cursor/input mode
3. `AGP_GameMode::DefaultPawnClass = AGP_CameraPawn`
4. `AGP_CameraPawn` replication revision: owner-only shell; non-replicated camera state
5. `DefaultGame.ini` → `GlobalDefaultGameMode=/Script/GPRuntime.GP_GameMode`
6. Full Standalone + 2P listen-server PIE validation

---

## Tech-lead locks (OD-1…OD-42) — RESOLVED

### OD-1 — Asset directory — RESOLVED
Package paths:
```
/Game/GrimProtocol/Input/Camera/IA_Camera_Pan
/Game/GrimProtocol/Input/Camera/IA_Camera_Zoom
/Game/GrimProtocol/Input/Camera/IA_Camera_Rotate
/Game/GrimProtocol/Input/Camera/IA_Camera_RotateToggle
/Game/GrimProtocol/Input/Camera/IMC_GP_Camera
```

Filesystem:
```
GP/Content/GrimProtocol/Input/Camera/IA_Camera_Pan.uasset
GP/Content/GrimProtocol/Input/Camera/IA_Camera_Zoom.uasset
GP/Content/GrimProtocol/Input/Camera/IA_Camera_Rotate.uasset
GP/Content/GrimProtocol/Input/Camera/IA_Camera_RotateToggle.uasset
GP/Content/GrimProtocol/Input/Camera/IMC_GP_Camera.uasset
```

### OD-2 — Input Action value types — RESOLVED
| Action | Type |
| --- | --- |
| `IA_Camera_Pan` | Axis2D |
| `IA_Camera_Zoom` | Axis1D |
| `IA_Camera_Rotate` | Axis1D |
| `IA_Camera_RotateToggle` | Bool |

Use actual UE 5.8 asset UI enum names at creation.

### OD-3 — Pan bindings — RESOLVED
In `IMC_GP_Camera`:

| Key | Action | Modifiers |
| --- | --- | --- |
| W / Up Arrow | Pan +Y | Swizzle Input Axis Values = YXZ |
| S / Down Arrow | Pan −Y | Swizzle YXZ + Negate |
| A / Left Arrow | Pan −X | Negate |
| D / Right Arrow | Pan +X | none |

No explicit Down trigger. No DeadZone for digital keyboard.

### OD-4 — Zoom binding and sign — RESOLVED
- Mouse Wheel Axis → `IA_Camera_Zoom`
- No modifier initially
- CameraPawn: positive → zoom in
- Operator verifies physical wheel-up; if UE reports negative, **add Negate on mapping** — do **not** change CameraPawn math
- Final asset: wheel up = zoom in; wheel down = zoom out

### OD-5 — Rotate axis — RESOLVED
- Mouse X → `IA_Camera_Rotate`
- No chord; no Negate
- PC performs MMB gate
- `bInvertRotate` stays in CameraPawn config only

### OD-6 — Rotate toggle — RESOLVED
- Middle Mouse Button → `IA_Camera_RotateToggle`
- Started → `SetRotateActive(true)`
- Completed / Canceled → `SetRotateActive(false)`
- No Triggered handler for toggle

### OD-7 — Mapping priority — RESOLVED
`CameraMappingPriority = 100` as named private constant / `static constexpr int32`. Not a tuning UPROPERTY.

### OD-8 — Mapping context ownership — RESOLVED
`AGP_PlayerController` owns mapping for local lifetime.

Init in `BeginPlayingState`:
- `Super::BeginPlayingState()`
- local controller only
- add once
- safe if SetupInputComponent earlier
- safe across pawn replacement
- listen-server local + remote client local PC supported
- dedicated server no-op

Not tied to CameraPawn possession.

### OD-9 — Mapping removal — RESOLVED
Override `EndPlay`: remove IMC if added; clear mapping-added flag; `Super::EndPlay`.

Do **not** remove mapping in `OnUnPossess`.

### OD-10 — Input asset reference policy — RESOLVED
Soft refs on C++ PC CDO with canonical package paths; defaults set in constructor; resolve once via `LoadSynchronous()` at local init.

Properties (`EditDefaultsOnly`, `Category="GP|Camera|Input"`):
- `TSoftObjectPtr<UInputMappingContext> CameraMappingContext`
- `TSoftObjectPtr<UInputAction> CameraPanAction`
- `TSoftObjectPtr<UInputAction> CameraZoomAction`
- `TSoftObjectPtr<UInputAction> CameraRotateAction`
- `TSoftObjectPtr<UInputAction> CameraRotateToggleAction`

Forbidden: ConstructorHelpers::FObjectFinder; native transient IA/IMC; BP solely for assignment.

### OD-11 — PlayerController Blueprint — RESOLVED
No `BP_GP_PlayerController`. Pure C++ `AGP_PlayerController`.

### OD-12 — CameraPawn Blueprint / config DA — RESOLVED
No `BP_GP_CameraPawn`. No `DA_GP_Camera_Default`. Use C++ pawn + CDO fallback.

### OD-13 — Possession architecture — RESOLVED
Normal server-authoritative GameMode spawn/possess.

```cpp
DefaultPawnClass = AGP_CameraPawn::StaticClass();
```

PC does **not** spawn or locally Possess. No client Possess. No unpossessed local camera. No SetViewTarget path.

### OD-14 — CameraPawn network policy revision — RESOLVED
**Deliberate GP-S15 correction** of earlier `bReplicates=false`:

```cpp
bReplicates = true;
bOnlyRelevantToOwner = true;
SetReplicateMovement(false);
```

Contract: **replicated owner-only shell, non-replicated camera state.**

- Actor existence + possession replicate so owning remote client receives pawn
- Camera transform does **not** replicate
- No CameraPawn replicated properties
- No RPC
- No movement replication
- Input/math remain local
- No camera-state bandwidth after actor/possession setup

### OD-15 — Tick local policy after revision — RESOLVED
Keep:
```cpp
if (!IsLocallyControlled()) { ResetFrameInput(); return; }
```

Owning client / listen-server local moves; server copy for remotes does not; dedicated server no camera work; non-owners do not receive pawn (owner-only relevant).

### OD-16 — PlayerController activation — RESOLVED
`AGP_GameMode` remains authority for:
- `PlayerControllerClass = AGP_PlayerController::StaticClass()`
- `DefaultPawnClass = AGP_CameraPawn::StaticClass()`
PlayerState/GameState assignments unchanged.

### OD-17 — Global GameMode activation — RESOLVED
Update `GP/Config/DefaultGame.ini`:
```ini
[/Script/EngineSettings.GameMapsSettings]
GlobalDefaultGameMode=/Script/GPRuntime.GP_GameMode
```
Preserve unrelated entries. Do **not** set GameDefaultMap / EditorStartupMap in GP-S15.

### OD-18 — Map policy — RESOLVED
No committed `.umap`. No dedicated camera test map. Validate on currently opened level with global default GameMode. Fallback bounds sufficient for basic live validation. Temporary BoundsVolume allowed; delete; do not save.

### OD-19 — EnhancedInput dependency — RESOLVED
Already in `GPRuntime.Build.cs`. Do not modify Build.cs unless inspection proves absent. Prefer forward decls; include official Enhanced Input headers minimally if needed.

### OD-20 — Exact PlayerController handler API — RESOLVED
Private:
```cpp
void InitializeCameraInput();
void RemoveCameraInputMapping();
AGP_CameraPawn* GetCameraPawn() const;
void OnCameraPan(const FInputActionValue& Value);
void OnCameraZoom(const FInputActionValue& Value);
void OnCameraRotate(const FInputActionValue& Value);
void OnCameraRotateStarted(const FInputActionValue& Value);
void OnCameraRotateStopped(const FInputActionValue& Value);
```
`OnCameraRotateStopped` bound to Completed **and** Canceled. No public camera input API on PC.

### OD-21 — CameraPawn lookup — RESOLVED
`GetCameraPawn()` → `Cast<AGP_CameraPawn>(GetPawn())`. No weak cache. Null/wrong → silent return. `OnLocalPawnReady` one-time validation/logging.

### OD-22 — SetupInputComponent — RESOLVED
1. `Super::SetupInputComponent()`
2. Cast to `UEnhancedInputComponent`
3. Resolve/load IA assets
4. Bind available actions once

Mapping add is separate (`BeginPlayingState`). If Enhanced component unavailable → one Error; return; no crash.

### OD-23 — Mapping state — RESOLVED
```cpp
bool bCameraMappingContextAdded = false;
bool bCameraInputBindingsInstalled = false;
bool bCameraRotateHeld = false;
```
Prevent duplicate mapping/bindings; reset rotate-held on UnPossess and EndPlay.

### OD-24 — Missing asset policy — RESOLVED
One Error per missing IA/IMC during init. Missing IMC → no mapping add. Missing IA → skip that bind only. Partial OK for diagnosis. No crash/assert/per-frame logs. Final acceptance requires all five assets present with zero missing-asset errors.

### OD-25 — Cursor and input mode — RESOLVED
In local `BeginPlayingState`:
```cpp
bShowMouseCursor = true;
FInputModeGameAndUI InputMode;
InputMode.SetHideCursorDuringCapture(false);
InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
SetInputMode(InputMode);
```
No UI focus management in GP-S15.

### OD-26 — Mouse capture config — RESOLVED
`DefaultInput.ini` already correct. Do not change unless inspection contradicts.

### OD-27 — Rotation cursor behavior — RESOLVED
Cursor remains visible; no warp/restore/hide while rotating; MMB = rotate; permanent capture supplies Mouse X.

### OD-28 — Edge-scroll focus — RESOLVED
No CameraPawn edge-scroll changes. Existing guards remain. Slate/UI focus suppression deferred.

### OD-29 — Input conflicts/consumption — RESOLVED
No LMB/RMB binds; MMB rotate; WASD/arrows/wheel/MouseX only; future UI may remove context; no GP-S15 UI context manager. Default consumption unless asset inspection exposes blocking conflict.

### OD-30 — Input event bindings — RESOLVED
| Action | Event | Handler |
| --- | --- | --- |
| Pan | Triggered | `OnCameraPan` |
| Zoom | Triggered | `OnCameraZoom` |
| Rotate | Triggered | `OnCameraRotate` (+ `bCameraRotateHeld` gate) |
| RotateToggle | Started | `OnCameraRotateStarted` |
| RotateToggle | Completed/Canceled | `OnCameraRotateStopped` |

No pan Completed binding (pawn resets + decelerates).

### OD-31 — Input forwarding — RESOLVED
- Pan: `Value.Get<FVector2D>()` → `SetPanInput`
- Zoom: `Value.Get<float>()` → `AddZoomInput`
- Rotate: if held → `AddRotateInput`
- Started: `bCameraRotateHeld=true`; `SetRotateActive(true)`
- Stopped: `bCameraRotateHeld=false`; `SetRotateActive(false)`

No camera math in PC.

### OD-32 — Possession lifecycle integration — RESOLVED
Scaffold remains. `OnLocalPawnReady`: existing behavior + accept `AGP_CameraPawn` or one Warning. `OnUnPossess`: clear rotate-held; `SetRotateActive(false)` on old CameraPawn if available; **do not** remove mapping. Other lifecycle hooks intact.

### OD-33 — Binary asset workflow — RESOLVED
**Phase A — Cursor:** C++ + DefaultGame.ini; three builds; stop before functional completion claim if assets absent.

**Phase B — Operator:** create five assets at exact paths; configure types/mappings/modifiers; save; report inspection.

Then Cursor: git status (only five intended `.uasset`); rebuild if needed; operator PIE; close stage.

No Python plugin. No raw binary write. No commandlet automation.

### OD-34 — Binary source control — RESOLVED
Commit all five `.uasset`. Do not commit DDC/Intermediate/Saved/autosaves/map/IDE junk. Review via paths + operator Editor inspection + PIE.

### OD-35 — Camera config asset — RESOLVED
No `DA_GP_Camera_Default`. CDO fallback active.

### OD-36 — CameraBoundsVolume validation — RESOLVED
Base acceptance may use FallbackBounds. Optional: temp place BoundsVolume; shrink; test clamp; delete; do not save. No permanent placement.

### OD-37 — Test level — RESOLVED
Currently opened Editor level. No `.umap` create/change. Before PIE verify World Settings does not override GlobalDefaultGameMode; if overridden, temporarily clear; do not save; report.

### OD-38 — Build and validation sequence — RESOLVED
SPEC_READY → implement C++ + DefaultGame.ini → three builds → operator assets → reload Editor → verify settings → PIE → optional BoundsVolume → no map dirtiness → docs → commit/push (when asked).

### OD-39 — Logging policy — RESOLVED
`LogTemp`, one-time only.

**Error:** non-Enhanced InputComponent; LocalPlayer subsystem unavailable; IMC missing; any IA missing.

**Warning:** possessed local pawn is not `AGP_CameraPawn`.

**No logs:** normal input values; per-frame forward; normal mapping add/remove; normal possession; absent BoundsVolume.

### OD-40 — Multiplayer acceptance — RESOLVED
| Mode | Requirement |
| --- | --- |
| Standalone PIE | Fully functional |
| 2-player listen-server PIE | Each local window own camera; no cross-move; no replicated camera transform; no possession errors |
| Dedicated server | Build passes; no local camera input/movement expected |

Full separate-machine network test **not** required.

### OD-41 — Persistent project changes — RESOLVED
**Allowed:**
- `GPPlayerController.h/.cpp`
- `GPCameraPawn.cpp` constructor replication flags
- `GPGameMode.cpp` DefaultPawnClass
- `DefaultGame.ini`
- five input `.uasset`
- stage documentation

**Do not change:** maps; BoundsVolume; CameraConfigDataAsset; MatchAssetLoader; PlayerState/GameState behavior; selection/commands; UI modules.

### OD-42 — Exact out of scope — RESOLVED
Excluded: selection; smart commands; UI mapping manager; pause/end-match disabling; key rebinding UI; gamepad; touch; drag-pan; cursor warp/restore; cinematic; CameraManager; camera state replication; replicated movement; CameraPawn RPC; permanent map placement; dedicated camera test map; **GP-S16**.

---

## Implemented code / config (Phase A)

- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h` — soft IA/IMC refs, transient loaded ptrs, EndPlay, camera input API
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp` — Enhanced Input bind/forward; mapping add in `BeginPlayingState`; remove in `EndPlay`; cursor/`FInputModeGameAndUI`; `PrimaryActorTick.bCanEverTick = true` (required for Enhanced Input)
- `GP/Source/GPRuntime/Private/Camera/GPCameraPawn.cpp` — owner-only replicated shell (`bReplicates=true`, `bOnlyRelevantToOwner=true`, `SetReplicateMovement(false)`); camera state/movement still non-replicated; Tick local-control guard unchanged
- `GP/Source/GPRuntime/Private/Game/GPGameMode.cpp` — `DefaultPawnClass = AGP_CameraPawn::StaticClass()`
- `GP/Config/DefaultGame.ini` — `GlobalDefaultGameMode=/Script/GPRuntime.GP_GameMode`
- `GPRuntime.Build.cs` — **unchanged** (EnhancedInput already present)
- `DefaultInput.ini` — **unchanged** (permanent capture already configured)

### Soft asset paths (constructor)

```
/Game/GrimProtocol/Input/Camera/IMC_GP_Camera.IMC_GP_Camera
/Game/GrimProtocol/Input/Camera/IA_Camera_Pan.IA_Camera_Pan
/Game/GrimProtocol/Input/Camera/IA_Camera_Zoom.IA_Camera_Zoom
/Game/GrimProtocol/Input/Camera/IA_Camera_Rotate.IA_Camera_Rotate
/Game/GrimProtocol/Input/Camera/IA_Camera_RotateToggle.IA_Camera_RotateToggle
```

### Input lifecycle

- Priority `100`
- One-time synchronous `LoadSynchronous` into transient `TObjectPtr`s
- Bindings in `SetupInputComponent` (Triggered pan/zoom/rotate; Started/Completed/Canceled rotate toggle)
- `AddMappingContext` in local `BeginPlayingState` after cursor/input mode
- `RemoveMappingContext` in `EndPlay` (not on UnPossess)
- No camera math in PlayerController

### PlayerController tick rationale (production)

- `AGP_PlayerController` tick **must remain enabled** (`PrimaryActorTick.bCanEverTick = true`).
- Enhanced Input action evaluation and event dispatch occur through the PlayerController input/tick lifecycle (`APlayerController::PlayerTick`).
- Disabling controller tick leaves IMC and bindings initialized but prevents action handlers from firing.
- CameraPawn still owns all camera math; PlayerController performs no custom per-frame camera work and has **no** custom `PlayerTick` override.

## Assets (operator Phase B — committed)

Five canonical `.uasset` under `GP/Content/GrimProtocol/Input/Camera/`. Old underscore-less names and redirectors absent from final tree.

## Acceptance Criteria (implementation phase)

- [x] Compiles: GPEditor Dev / GP Dev / GP Shipping — PASSED
- [x] Five assets present; no missing-asset errors — PASSED
- [x] Standalone PIE: pan/zoom/rotate/edge-scroll; cursor visible — PASSED
- [x] 2P listen-server: independent local cameras; no camera transform replication — PASSED
- [x] Dedicated server build passes — GP Shipping PASSED
- [x] Optional BoundsVolume clamp without map save — PASSED (temp actor deleted; map not committed)
- [x] No permanent map/assets beyond five IA/IMC
- [x] GP-S16 not started
- [x] Tech lead accepted
- [x] Operator accepted

## Manual Editor validation — PASSED

Standalone + 2P listen-server PIE; wheel polarity confirmed (up=in, down=out); MMB gate confirmed; temporary BoundsVolume clamp validated then deleted; map not committed.

## Risks / edge cases

- Owner-only relevance means non-owners never see other cameras (intended).
- World Settings GameMode override can still hide GlobalDefaultGameMode on a dirty map — do not commit map overrides.
- Asset rename/redirector stale Editor state required full Editor restart after Fix Up Redirectors (documented incident).

## Linked canonical docs
TDD/13, TDD/11, TDD/04 (secondary), ADR-0006, GP-S13, GP-S14, STYLE, CONTRIBUTING, Coding_Rules, Naming_Conventions.

## Stop Condition
GP-S15 closed as **DONE**. Tech lead accepted. Operator accepted. Next allowed stage per TDD/13: **GP-S16 UGP_SelectionComponent**. GP-S16 not started; task file not materialized.
