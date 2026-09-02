# RTS Camera

## Scope

Design spec для player camera у GrimProtocol MVP. Stage — design only (per [`Claude_Tasks/GP-0201_RTS_Camera`](../Development/Claude_Tasks/GP-0201_RTS_Camera.md)). Не містить C++ implementation. Описує responsibility split, data model, input mapping, edge cases, validation.

Pillar references: Pillar 9 (Technical — server-authoritative, data-driven, GAS-first), Pillar 8 (Simple Core, Combinatorial Depth — camera is simple input layer, no tutorial needed), [`ADR-0002 Data-Driven First`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md), [`ADR-0006 Indie Scope`](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md).

## Problem

RTS gameplay вимагає, щоб гравець:

- Швидко переглядав велику площу карти без втрати focal point selection.
- Тримав постійний god-view angle, без disorientation.
- Не отримував advantage за рахунок camera (no fog peek, no z-cheat).
- Мав consistent UX при WASD, edge-scroll, MMB rotate, mouse wheel zoom — без конфліктів з selection (LMB) і command (RMB) пайплайном з [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md).

Camera — purely local presentation concern. Це не gameplay state, не GAS, не replicated.

## Solution

### High-level

Один Pawn — `AGP_CameraPawn` — володіє всією camera-control логікою (Simple First). Лоиіка повністю локальна, server-side немає. Tuning живе у `UGP_CameraConfigDataAsset` (Data First). PlayerController власне забирає на себе лише Enhanced Input setup (already on path per existing `AGP_PlayerController` scaffold) і forward-ить axis values до Possessed Pawn.

```
EnhancedInput (IMC_Camera)
  └─ IA_Camera_Pan (2D)        ──┐
  └─ IA_Camera_Zoom (1D)       ──┼─► AGP_PlayerController (input forwarder)
  └─ IA_Camera_Rotate (1D)     ──┤            │
  └─ IA_Camera_RotateToggle    ──┘            ▼
                                      AGP_CameraPawn (local, Tick)
                                         ├─ ApplyPan(WASD + edge-scroll)
                                         ├─ ApplyZoom (SpringArm length + optional pitch interp)
                                         ├─ ApplyRotate (Yaw on RootScene)
                                         └─ ClampToMapBounds()

UGP_CameraConfigDataAsset  ──► AGP_CameraPawn::Config (EditDefaultsOnly TSoftObjectPtr, async-loaded)
```

### Responsibility Split

| Layer | Responsibility |
| --- | --- |
| `AGP_CameraPawn` | Owns RootScene + SpringArm + CameraComponent. Tick consumes pending input values, integrates pan/zoom/rotate, applies clamps. Reads tuning from `Config`. **No gameplay reads/writes**. |
| `AGP_PlayerController` | Pushes Enhanced Input IMC, binds IA_Camera_* → pawn setters. Possesses `AGP_CameraPawn` on `BeginPlay`. **No camera math**. |
| `UGP_CameraConfigDataAsset` | Immutable tuning: speeds, clamps, thresholds, curves. Read-only at runtime. |
| Map level | Provides `MapBounds` as a `UBoxComponent` on a world settings actor OR via Data Asset constant. Pawn clamps location against this volume. |

Selection / Command components (existing) — untouched. Camera Pawn does not interfere with their input bindings — usage of distinct IA_* actions guarantees no consumption conflict.

### Authority and Replication

- `AGP_CameraPawn::bReplicates = false` (already set in scaffold).
- Pawn auto-possessed on login by local PC only. Server has dummy camera or no possession — camera Pawn class is registered as `DefaultPawnClass` у GameMode для local PC use, але state never travels over network.
- Не реалізується server-camera fallback. Server у dedicated config просто не має camera state.

### State Model

Pawn зберігає три cumulative scalar/vector accumulators, що оновлюються на Tick з нормалізованих input values:

```
struct CameraState (members of AGP_CameraPawn, not USTRUCT)
  FVector2D  PendingPanInput     // current frame WASD vector, [-1..1]
  float      PendingZoomInput    // current frame wheel delta, signed
  float      PendingRotateInput  // current frame yaw delta (MMB drag)
  bool       bRotateActive       // MMB held
  FVector2D  PendingEdgeInput    // derived each Tick from cursor screen pos
  float      CurrentArmLength    // smoothed toward TargetArmLength
  float      TargetArmLength
  float      CurrentYaw
```

Інтеграція на Tick (frame-rate-independent через `DeltaSeconds`):

1. Sample cursor screen position → derive edge-scroll vector (if `Config.bEdgeScrollEnabled` and viewport focused).
2. Compose pan input = `PendingPanInput + PendingEdgeInput`, clamp magnitude.
3. Transform pan into world-space relative to current camera yaw (so WASD пан по екрану, не по world axis).
4. `RootScene` translation += `PanWorld * Config.PanSpeed * (1 + Config.ZoomPanScale * ZoomFraction) * DeltaSeconds`.
5. `TargetArmLength = Clamp(TargetArmLength + PendingZoomInput * Config.ZoomStep, Config.MinArmLength, Config.MaxArmLength)`.
6. `CurrentArmLength = FMath::FInterpTo(CurrentArmLength, TargetArmLength, DeltaSeconds, Config.ZoomInterpSpeed)`.
7. `SpringArm->TargetArmLength = CurrentArmLength`.
8. Optional pitch interpolation by zoom fraction (`FMath::Lerp(Config.PitchAtMaxZoom, Config.PitchAtMinZoom, ZoomFraction)`).
9. If `bRotateActive`, accumulate yaw += `PendingRotateInput * Config.RotateSpeed` (MouseX is already delta per frame).
10. Apply `RootScene->SetWorldRotation(FRotator(0, CurrentYaw, 0))`.
11. Clamp Root location into `Config.FallbackBounds` (intersected with level-provided bounds if any).
12. Reset per-frame inputs (`PendingPanInput`, `PendingZoomInput`, `PendingRotateInput`, `PendingEdgeInput`) to zero.

### Public Interface (Design Contract)

Header skeleton — design contract only, no bodies:

```cpp
UCLASS()
class GPRUNTIME_API AGP_CameraPawn : public APawn
{
    GENERATED_BODY()
public:
    AGP_CameraPawn();

    // Input setters called by PlayerController. Pure intent; integrated on Tick.
    void SetPanInput(const FVector2D& AxisXY);          // [-1..1] each axis
    void AddZoomInput(float WheelDelta);                // signed step count
    void AddRotateInput(float MouseDeltaX);             // raw mouse-X delta
    void SetRotateActive(bool bActive);                 // MMB pressed / released

protected:
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditDefaultsOnly, Category = "GP|Camera")
    TSoftObjectPtr<UGP_CameraConfigDataAsset> ConfigRef;

    // Resolved after async load (transient, runtime-only).
    UPROPERTY(Transient)
    TObjectPtr<UGP_CameraConfigDataAsset> CachedConfig;

    void AsyncLoadConfig();   // BeginPlay → UAssetManager async load → sets CachedConfig
    void OnConfigLoaded();    // callback

    // existing scaffold (RootScene / SpringArm / Camera) — unchanged
};
```

PC-side surface (additive to existing scaffold):

```cpp
// in AGP_PlayerController
void OnCameraPan(const FInputActionValue& Value);
void OnCameraZoom(const FInputActionValue& Value);
void OnCameraRotateAxis(const FInputActionValue& Value);
void OnCameraRotateToggle(const FInputActionValue& Value); // Started / Completed
```

Implementation само лишається на code task (out of scope тут).

## Trade-offs

| Decision | Trade-off |
| --- | --- |
| Все в Pawn (vs Component) | (+) Найменше boilerplate, легше читати, відповідає Simple First. (−) Якщо пізніше додамо replay/spectator/cinematic camera — рефакторити у `UGP_CameraControlComponent`. Acceptable: один файл, ~300 LOC, < soft limit 1000. |
| DataAsset (vs UPROPERTY-only) | (+) Designer-tunable без recompile, можна мати profiles (`Default`, `Cinematic`, `Debug`). (−) Один extra asset для setup. Justified per ADR-0002. |
| WorldRotation на RootScene (vs SpringArm) | (+) Pan завжди screen-relative, бо WASD трансформується через camera yaw. (−) SpringArm pitch керується separately. Це навмисно — yaw і pitch decoupled. |
| Edge-scroll on Tick (vs platform cursor events) | (+) Cross-platform, не залежить від OS cursor edge events. (−) Працює тільки при focused viewport — корисний guard, див. edge cases. |
| Pawn-owned camera (vs PC-owned ViewTarget) | (+) Стандарт UE, легше SetViewTarget switching (e.g., end-of-match cinematic). (−) Зайвий actor у world. Acceptable. |
| No replication | (+) Безкоштовно, zero bandwidth, zero anti-cheat surface. (−) Spectator camera пізніше доведеться додати окремо. Acceptable у MVP. |

## UGP_CameraConfigDataAsset Schema

Path: `/Game/GrimProtocol/DataAssets/Camera/DA_GP_Camera_Default`.
Owner module: `GPRuntime`. Naming per [`STYLE.md`](../../STYLE.md).

### Field List

| Field | Type | Default | Units | Purpose |
| --- | --- | --- | --- | --- |
| `PanSpeed` | `float` | `2500` | cm/s | Base WASD/edge pan speed at mid-zoom. |
| `ZoomPanScale` | `float` | `0.5` | dimensionless | Pan speed multiplier scaled by zoom fraction (further out → faster pan). Final speed = `PanSpeed * (1 + ZoomPanScale * ZoomFraction)`. |
| `EdgeScrollSpeed` | `float` | `1.0` | scalar | Multiplier on `PanSpeed` while edge-scrolling. `1.0` = parity with WASD. |
| `EdgeThresholdPx` | `int32` | `8` | pixels | Distance from viewport edge that activates edge-scroll. |
| `EdgeFalloffPx` | `int32` | `24` | pixels | Linear ramp 0→1 over [`EdgeThresholdPx`, `EdgeThresholdPx+EdgeFalloffPx`] for soft activation. |
| `bEdgeScrollEnabled` | `bool` | `true` | — | Global toggle (settings menu). |
| `MinArmLength` | `float` | `1200` | cm | Closest zoom. |
| `MaxArmLength` | `float` | `4500` | cm | Farthest zoom. |
| `DefaultArmLength` | `float` | `2500` | cm | Spawn arm length. |
| `ZoomStep` | `float` | `300` | cm per wheel notch | Per-tick zoom delta. |
| `ZoomInterpSpeed` | `float` | `8.0` | 1/s | `FInterpTo` rate (higher = snappier). |
| `PitchAtMaxZoom` | `float` | `-65` | degrees | Pitch when fully zoomed out (steeper). |
| `PitchAtMinZoom` | `float` | `-45` | degrees | Pitch when fully zoomed in (flatter). |
| `bPitchInterpEnabled` | `bool` | `true` | — | Toggle zoom-driven pitch. |
| `RotateSpeed` | `float` | `4.0` | deg per mouse-px | MMB drag → yaw delta. |
| `bInvertRotate` | `bool` | `false` | — | Designer-toggleable. |
| `FallbackBounds` | `FBox` | `{(-50000,-50000,-1000), (50000,50000,5000)}` | cm | Used when level provides no `AGP_CameraBoundsVolume`. |
| `MoveAccelTime` | `float` | `0.08` | s | Time-to-full-pan-speed; smooths WASD start. |
| `MoveDecelTime` | `float` | `0.10` | s | Time-to-zero on key release. |

### Validation Rules

- `MinArmLength < DefaultArmLength < MaxArmLength` — assert у `IsDataValid`.
- `EdgeThresholdPx >= 0`, `EdgeFalloffPx >= 0`.
- `PanSpeed > 0`, `ZoomStep > 0`, `ZoomInterpSpeed > 0`.
- `PitchAtMaxZoom <= PitchAtMinZoom <= 0` (negative pitch = looking down).

`UGP_CameraConfigDataAsset` inherits `UPrimaryDataAsset`. PrimaryAssetType: `GP_CameraConfig`. Registered у [`10_Data_Assets`](10_Data_Assets.md) при коміті.

### Profile Plan

- `DA_GP_Camera_Default` — baseline.
- `DA_GP_Camera_Debug` (post-MVP) — wider bounds, faster pan, higher zoom range.
- Faction-specific або cinematic — out of MVP.

## Enhanced Input Set

### IMC

`IMC_GP_Camera` — додається у Enhanced Input subsystem на `AGP_PlayerController::BeginPlay`, priority `100` (нижчий за UI capture, вищий за gameplay default). Co-exists з `IMC_GP_Selection` і `IMC_GP_Commands` — окремий IMC щоб можна було вимикати окремо (e.g., у paused / end-of-match).

### Input Actions

| Action | Value Type | Default Bindings | Triggers | Modifiers |
| --- | --- | --- | --- | --- |
| `IA_Camera_Pan` | `Axis2D` | `W/A/S/D`, `↑/←/↓/→` | `Down` | Per-key axis modifier composing (W→ +Y, S→ −Y, A→ −X, D→ +X), DeadZone(0.0) |
| `IA_Camera_Zoom` | `Axis1D` | `MouseWheelAxis` | `Down` (axis-active) | Scalar(1.0) |
| `IA_Camera_Rotate` | `Axis1D` | `MouseX` | `Down` while `IA_Camera_RotateToggle` is active | Negate (optional, by `Config.bInvertRotate` — set via dynamic modifier or PC-side flip) |
| `IA_Camera_RotateToggle` | `Bool` | `Middle Mouse Button` | `Started` / `Completed` | — |

`IA_Camera_Rotate` chained behind `IA_Camera_RotateToggle` через `InputAction Chord` або через PC-side gate (set bool, dispatch only коли true). Recommended — PC-side gate, простіше для debug.

### Mouse Capture Mode

- `DefaultViewportMouseCaptureMode = CapturePermanently_IncludingInitialMouseDown` — already set у `DefaultInput.ini`. ОК для RTS.
- `bShowMouseCursor = true` на PC (RTS standard).
- Edge-scroll вимагає cursor visible і within viewport — guard на `IsLocalPlayerController() && GetWorld()->GetGameViewport()->Viewport->HasFocus()`.

### Action Boundaries (Conflict Avoidance)

| Conflict | Resolution |
| --- | --- |
| LMB selection vs camera | Camera does not bind LMB. No conflict. |
| RMB command vs camera | Camera does not bind RMB. No conflict. |
| MMB pan (common RTS alt) vs MMB rotate | MMB reserved for rotate per this spec. If pan-drag (MMB) is later requested — bind to alternate `IA_Camera_DragPan` with `Shift+MMB` chord. |
| UI focus | When focusable widget captured (e.g., chat, settings), `IMC_GP_Camera` deactivated by UI subsystem call to `RemoveMappingContext`. |
| Edge-scroll over HUD panels | Per-HUD widget gives panel rects; pawn checks cursor against `bIgnoreEdgeScrollOverUI` flag — see edge cases. |

## Map Bounds

Two sources, intersected:

1. **Level-provided.** Optional actor `AGP_CameraBoundsVolume` (UBoxComponent), placed in level. If present, pawn clamps to its world-space AABB.
2. **DataAsset fallback.** If no bounds volume in level, use `Config.FallbackBounds`.

Clamp performed on RootScene world XY only; Z is fixed (camera doesn't rise vertically with pan). If pawn detects unbounded level — log warning `LogGPCamera Warning` on BeginPlay.

Resolved bounds are the same `FBox` `ClampToBounds` uses. Public seam for local presentation:

- `AGP_CameraPawn::GetResolvedCameraBounds(FBox& OutBounds) const` — valid `AGP_CameraBoundsVolume` AABB, else the active `Config.FallbackBounds` (CDO until async config load, then authored DA).
- `OnResolvedCameraBoundsChanged` fires after `BeginPlay` (volume find + first clamp) and after async `HandleConfigLoaded`. Bounds are static for MVP after that; no Tick poll.
- GPUIRuntime must not re-scan the world for volumes. Minimap displayed extents consume this getter.

`AGP_CameraBoundsVolume` is a trivial actor; spec deferred to code task. No replication. The FoW gameplay grid (2000×2000 / 100 cm) is a separate technical visibility field and is **not** the camera/playable bounds.

## Edge Cases

| Case | Behavior |
| --- | --- |
| **Alt-tab / focus loss** | Viewport `HasFocus() == false` → suppress edge-scroll, freeze pending inputs to zero, keep current location. Reset on regain focus. |
| **UI focus capture** | When a focusable widget grabs input (modal dialog, settings panel) → PC removes `IMC_GP_Camera`. Pawn naturally stops receiving setters. |
| **Cursor over HUD panel** | If `HUD->IsCursorOverInteractiveUI(MousePos)` is true → suppress edge-scroll *only* (WASD still works). Avoids accidental scroll while clicking command bar. |
| **Marquee selection (LMB drag)** | Selection layer drags rectangle; camera ignores LMB. Edge-scroll *should* remain active during marquee — це навмисно, гравець тягне за межі viewport. |
| **Match end** | `IMC_GP_Camera` removed by HUD/PC; pawn free-floats. End-of-match cinematic may possess другий pawn via `SetViewTargetWithBlend`. |
| **Pause (singleplayer)** | `IMC_GP_Camera` лишається активним (RTS pause часто дозволяє огляд), але can be toggled by `Config.bAllowPanWhilePaused`. Default: `true`. |
| **Low frame-rate** | All deltas multiplied by `DeltaSeconds`. Test at 30 fps and 240 fps — values consistent. |
| **Window resize** | Edge-scroll thresholds є absolute pixels — at 4K thresholds still 8 px → дуже тонка зона. Plan: scale by `ViewportSize.Y / 1080` (DPI factor) — TODO у code task. |
| **No bounds volume present** | Use `Config.FallbackBounds`; emit warning. Map artist responsible per level. |
| **Cursor exits viewport during MMB rotate** | Mouse capture is `LockOnCapture` — cursor locked to viewport while MMB held. Mouse position reset center on release. |
| **Spectator / dedicated server** | Pawn is local-only; dedicated server doesn't spawn it. Spectator post-MVP — separate spec. |
| **Replay** | Out of MVP. Future: record `RootScene` transform + `SpringArm` length per frame у replay subsystem (not GAS, not replicated). |

## Multiplayer Impact

None. Confirms validation criteria у GP-0201:

- Camera does **not** mutate gameplay state — only its own transform.
- Camera does **not** issue RPCs.
- Camera does **not** read replicated game state for authority decisions.
- Server has no camera. Cheating via camera impossible (fog of war handled by visibility system, not camera).

## Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | **Basic pan reach** | WASD travels full map diagonal in ≤ 6 s at mid-zoom. |
| 2 | **Edge-scroll parity** | Cursor at viewport edge moves at parity with WASD pan speed (± 10%). |
| 3 | **Zoom range** | Wheel scroll min↔max within 1.5 s; pitch smoothly interpolates if enabled. |
| 4 | **Rotate around selection** | MMB rotate keeps map area near cursor visible without flipping pitch. |
| 5 | **Bounds clamp** | Pan blocked at map edges; камера не виходить за `MapBounds`. |
| 6 | **UI gate** | Hover над command bar → edge-scroll вимикається; WASD працює. |
| 7 | **Alt-tab freeze** | Alt-tab during edge-scroll → камера зупиняється негайно. |
| 8 | **Selection + scroll** | Marquee LMB drag за межі viewport → камера тягне; selection rectangle continues coherent. |
| 9 | **30 fps consistency** | Pan/zoom feel identical to 144 fps reference within tolerance. |
| 10 | **Multiplayer non-impact** | Player A пересуває camera 60 s; Player B (LAN) — нульове замітне network usage у camera-related RPC. |

## Validation Checklist (Stop Condition)

- [ ] Camera does not mutate gameplay state — confirmed by spec (no GAS, no replication, no RPC).
- [ ] Camera supports selection/commands without blocking — Enhanced Input mappings disjoint with `IMC_GP_Selection` / `IMC_GP_Commands`.
- [ ] No multiplayer authority concerns — `bReplicates = false`, server has no camera state.
- [ ] All tuning fields live in `UGP_CameraConfigDataAsset`.
- [ ] All edge cases documented.
- [ ] Playtest scenarios defined.

## Open Questions

1. Slot rotation snap-to-cardinal на double-MMB? (Out of MVP unless requested.)
2. Camera focus on selected unit (`F` key) — у GP-0202 чи у GP-0201? Recommend GP-0202 (selection-driven).
3. Minimap click-to-pan — driven by HUD, not camera spec. Documented у GP-0401.
4. DPI scaling for `EdgeThresholdPx` — apply at PC level чи у DataAsset? Recommend PC-level via viewport size, документувати у code task.
5. Cinematic / end-of-match camera — окрема spec post-MVP. Чи варто зразу зарезервувати `AGP_CinematicCameraPawn` clas name? Recommend yes, лише name reservation.

## Out of Scope

- Edge-scroll DPI scaling implementation details (lives in code task).
- Cinematic camera, spectator camera, replay camera.
- Camera shake / impact feedback.
- Faction-specific or per-map camera profiles (still possible via DataAsset, no code change needed).
- Gamepad input (PC-only MVP).
- Touch input.

## Follow-up Code Task

Code implementation tracked separately. Recommended task: **GP-0201A RTS Camera Implementation** — Code Allowed: Yes, scope = single slice (DataAsset + Pawn + PC bindings + level bounds actor). Stop condition = working camera passes playtest scenarios 1–10. Will be added to backlog after this spec is approved.

## References

- [`GP-0201_RTS_Camera`](../Development/Claude_Tasks/GP-0201_RTS_Camera.md) — owning task.
- [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md) — input pipeline this camera coexists with.
- [`10_Data_Assets`](10_Data_Assets.md) — DataAsset ownership table (to be amended with `GP_CameraConfig`).
- [`ADR-0002 Data-Driven First`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md).
- [`ADR-0006 Indie Scope`](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md).
- [`STYLE.md`](../../STYLE.md) — naming and asset path conventions.
