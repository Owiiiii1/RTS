# GP-S08 — AGP_PlayerController Scaffold
(Camera Pawn Possession and Future PlayerState ASC Linkage)

## Slice Group
Slice 2 — Match Flow + Asset Loader

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S07 DONE — `AGP_GameMode` match orchestration.
- `UGP_AbilitySystemComponent` exists in `GPGASRuntime` (GP-S04) — **not** owned by PlayerController.
- Module `GPRuntime` (already depends on `GPGASRuntime`, `EnhancedInput`).
- Native tags / AttributeSets already on disk (not required for PC scaffold compile beyond ASC forward decl / include if getter is typed).

## Goal
Implement a minimal network-correct `AGP_PlayerController : APlayerController` in `GPRuntime` that:

- exists on server and owning client;
- owns the RTS camera-pawn possession *slot* (generic until `AGP_CameraPawn` exists);
- prepares lifecycle hooks for future `AGP_PlayerState` + ASC linkage;
- does **not** implement selection, commands, Enhanced Input assets, UI, or gameplay abilities.

Honest scope before `AGP_PlayerState` (GP-S09), `AGP_CameraPawn` (GP-S13), and IMC assets (GP-S15): scaffold + idempotent lifecycle only.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S09 until explicitly assigned (do not auto-materialize GP-S09 task file).

### Closed with
- `AGP_PlayerController : APlayerController` — Tick disabled.
- Generic `APawn` lifecycle; server `OnPossess` / `OnUnPossess`; owning-client `AcknowledgePossession` / `OnRep_PlayerState` / `BeginPlayingState`.
- Local-only pawn initialization; idempotency guards via `TWeakObjectPtr`.
- PlayerState queried through `IAbilitySystemInterface`; `GetGPAbilitySystemComponent` is live query (not permanent nullptr stub).
- No ASC creation; no second ASC; no `InitAbilityActorInfo`; no OwnerActor / AvatarActor assignment.
- `AGP_GameMode` sets `PlayerControllerClass = AGP_PlayerController`; no `PlayerStateClass`; no `DefaultPawnClass`.
- No input mappings; no cursor policy; no selection / commands / UI; no RPC; no replicated gameplay fields.
- No assets / Blueprint / map / config.
- Operator Editor/PIE validation **PASSED**.
- Runtime PlayerControllerClass / listen-server proof **deferred** until temporary GameMode wiring.
- ASC runtime proof **deferred** until GP-S09 PlayerState.

---

## Tech-lead locks (OD-1…OD-10) — RESOLVED

### OD-1 — RESOLVED: CameraPawn
- No CameraPawn created; no PC spawn/Possess.
- Standard GameMode/DefaultPawnClass lifecycle (DefaultPawnClass still engine default until GP-S13).
- Generic `APawn` in OnPossess/AcknowledgePossession; CameraPawn class check deferred.

### OD-2 — RESOLVED: PlayerState
- No `AGP_PlayerState`. Uses `APlayerState` + engine `IAbilitySystemInterface` only.
- Real PlayerState = GP-S09.

### OD-3 — RESOLVED: ASC access
- `GetGPAbilitySystemComponent()` queries PlayerState → ASI → Cast to `UGP_AbilitySystemComponent`.
- No create / second ASC / cache / `InitAbilityActorInfo` / Owner/Avatar assignment.

### OD-4 — RESOLVED: Lifecycle
Overrides: BeginPlay, OnPossess, OnUnPossess, AcknowledgePossession, OnRep_PlayerState, BeginPlayingState, SetupInputComponent.  
Server: OnPossess/OnUnPossess. Owning client: AcknowledgePossession, OnRep_PlayerState, BeginPlayingState, local BeginPlay. No extra empty SetupInput hook.

### OD-5 — RESOLVED: Input
No Enhanced Input subsystem usage, IMC, IA, camera/selection bindings. SetupInputComponent = Super only.

### OD-6 — RESOLVED: Cursor
No `bShowMouseCursor` / click / mouse-over / InputMode changes. Deferred.

### OD-7 — RESOLVED: Network
No RPC, no replicated gameplay props, no camera/selection replication, no Tick.

### OD-8 — RESOLVED: GameMode wiring
`AGP_GameMode` constructor: `PlayerControllerClass = AGP_PlayerController::StaticClass()` (cpp include only; header unchanged).

### OD-9 — RESOLVED: PlayerStateClass
Engine default retained until GP-S09.

### OD-10 — RESOLVED: Local-only
`OnLocalPawnReady` / local pawn init only when `IsLocalController()`.

---

## Canonical responsibilities

### AGP_PlayerController (this slice)
- Network-correct PC subclass in `GPRuntime`.
- Possession lifecycle scaffold for future `AGP_CameraPawn`.
- PlayerState / ASC discovery hooks (no authoritative player data ownership).
- Input component hook placeholder.
- Optional GameMode `PlayerControllerClass` wiring (implementation).

### Not PlayerController
- Authoritative player attributes / score / team → future `AGP_PlayerState`.
- ASC create/init → future `AGP_PlayerState` (+ later pawn lifecycle as needed).
- Camera math → future `AGP_CameraPawn`.
- Selection / smart commands / RPCs → later components + command slices.
- HUD / ViewModels → `GPUIRuntime` / UI slices.

## Network existence model
- Server: one `AGP_PlayerController` per human connection (GameMode spawn).
- Owning client: autonomous proxy / local controller for input + future camera.
- Listen server: host has both server + local roles — guards must use `IsLocalController()` correctly.
- Does not own match SoT (`AGP_GameState`) or match timer (`AGP_GameMode`).

## Server lifecycle
1. GameMode spawns `AGP_PlayerController` (after OD-8 wiring).
2. Engine may spawn default pawn (engine DefaultPawn until CameraPawn exists) and possess on server.
3. `OnPossess` / `OnUnPossess` run; no gameplay spawn of units/buildings.
4. No MatchState mutation from PC.
5. No RPCs.

## Owning-client lifecycle
1. `BeginPlay` (local).
2. `AcknowledgePossession` when pawn is acknowledged.
3. `OnRep_PlayerState` / `BeginPlayingState` → `TryInitializePlayerStateLink`.
4. `OnLocalPawnReady` if local + pawn valid (empty / log in GP-S08).
5. `OnAbilitySystemLinkReady` **not** called until ASC non-null (never in GP-S08 without PlayerState ASC).

## Possession lifecycle
- Controlled pawn slot = RTS camera pawn when available; **never** selected units/buildings.
- Unit commands later via server-authoritative command flow — out of scope.
- GP-S08: generic `APawn` OK; CameraPawn class check deferred.

## CameraPawn ownership model
| Item | Decision |
| --- | --- |
| Class name (docs) | `AGP_CameraPawn` |
| Module | `GPRuntime` |
| Exists on disk | **No** (GP-S13) |
| Spawn owner | GameMode `DefaultPawnClass` (later) |
| Replication | `bReplicates = false` (TDD/11) |
| GP-S08 | generic possession hooks only |

## PlayerState relationship
- Future owner of player ASC + `UGP_PlayerAttributeSet`.
- GP-S08 talks only to `APlayerState*` via engine PC APIs.
- No `PlayerStateClass` change.

## ASC ownership and linkage
- Owner: future `AGP_PlayerState`.
- PC: query-only via `GetGPAbilitySystemComponent()` → `nullptr` in GP-S08.
- No `CreateDefaultSubobject<UGP_AbilitySystemComponent>` on PC.
- No `InitAbilityActorInfo` from PC in GP-S08.
- AvatarActor / OwnerActor pairing deferred to GP-S09 (non-blocking open detail).

## Input policy
Super `SetupInputComponent` only. No IMC/assets. No camera/selection bindings.

## Mouse / cursor policy
Deferred (see OD-6). TDD/11 remains future SoT for `bShowMouseCursor = true`.

## Replication / RPC rules
None added in GP-S08 (see OD-7).

## GameMode wiring decision
**OD-8 = A:** set `PlayerControllerClass` in `AGP_GameMode` constructor during **implementation** assignment. Spec pass does not touch C++.

## Exact planned API

```cpp
UCLASS()
class GPRUNTIME_API AGP_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGP_PlayerController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void OnRep_PlayerState() override;
	virtual void BeginPlayingState() override;
	virtual void SetupInputComponent() override;

	UGP_AbilitySystemComponent* GetGPAbilitySystemComponent() const;

protected:
	void TryInitializePlayerStateLink();
	void TryNotifyLocalPawnReady(APawn* InPawn);

	virtual void OnLocalPawnReady(APawn* InPawn);
	virtual void OnPlayerStateReady(APlayerState* InPlayerState);
	virtual void OnAbilitySystemLinkReady(UGP_AbilitySystemComponent* InASC);

private:
	TWeakObjectPtr<APawn> LastInitializedPawn;
	TWeakObjectPtr<APlayerState> LastInitializedPlayerState;
};
```

Notes:
- `InitPlayerState` engine override **not required** if `OnRep_PlayerState` + `BeginPlayingState` cover the seam.
- `OnAbilitySystemLinkReady` remains a no-call stub until GP-S09 exposes ASC.
- Tick disabled (`PrimaryActorTick.bCanEverTick = false`) unless a later slice proves need — default **off**.

## Idempotency strategy
- `TryNotifyLocalPawnReady`: no-op if `InPawn == LastInitializedPawn` (and valid).
- `TryInitializePlayerStateLink`: no-op if same `LastInitializedPlayerState`; if ASC still null, may re-check when PlayerState object identity changes or when a later slice adds an explicit refresh — GP-S08 keeps it simple (identity guard + log once per PS).
- Clear pawn weak ptr on `OnUnPossess`.
- Prefer `TWeakObjectPtr` for guards; do not cache raw ASC pointers as authoritative state.

## Out of Scope
- `AGP_PlayerState` / AttributeSet grant / ASC create / `InitAbilityActorInfo`
- `AGP_CameraPawn` / `DefaultPawnClass` assignment
- Enhanced Input assets / IMC / camera movement
- Selection / Command / PlayerUI components
- Command RPCs / selection RPCs / camera RPCs
- HUD / ViewModels / widgets / CommonUI routing
- Cursor `GameAndUI` policy
- Replicated PC gameplay fields
- Tick-driven logic
- Blueprint / map / DefaultEngine.ini wiring beyond C++ `PlayerControllerClass`
- GP-S09+

## Files delivered
- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPRuntime/Private/Game/GPGameMode.cpp` — `PlayerControllerClass` (OD-8); header unchanged

## Acceptance Criteria (implementation)
- [x] Compiles (GPEditor Dev, GP Dev, GP Shipping).
- [x] `AGP_GameMode` sets `PlayerControllerClass` to `AGP_PlayerController`.
- [x] `PlayerStateClass` / `DefaultPawnClass` unchanged.
- [x] No ASC subobject on PC; getter uses `IAbilitySystemInterface` (nullptr until GP-S09).
- [x] No RPC / replicated gameplay props / Tick.
- [x] No CameraPawn / PlayerState / input assets / UI created.
- [x] Lifecycle overrides present and idempotent; client path not OnPossess-only.
- [x] No GP-S09 bundled.
- [x] Operator Editor validation (Class Viewer / PIE without map wiring) PASSED.
- [x] PlayerControllerClass runtime proof — **deferred** (accepted for close).
- [x] Listen-server proof — **deferred** (accepted for close).
- [x] ASC runtime proof — **deferred** to GP-S09 (accepted for close).
- [x] Tech lead accepted / operator accepted → DONE.

## Manual Editor validation (operator — no assets committed by agent)
1. Open project — no module/load errors.
2. Class Viewer — `AGP_PlayerController` and `AGP_GameMode` visible.
3. PIE on current map (without assigning AGP_GameMode) — must not break.
4. **Deferred runtime proof** (temporary wiring, do not commit): assign `AGP_GameMode`; confirm PC class + idempotent pawn/PS callbacks; ASC getter null until GP-S09.

## Listen-server validation plan
Host + client: both get `AGP_PlayerController`; host local path initializes once; no camera RPC; possession does not claim selected-unit semantics (N/A until selection). Full camera/ASC proof deferred to GP-S13 / GP-S09.

## Risks / edge cases
- Engine DefaultPawn until CameraPawn exists — scaffold must tolerate non-camera pawn.
- Listen-server dual role — idempotent guards required.
- TDD/13 title oversells “Possess CameraPawn / ASC linkage” vs honest deferred hooks — document as intentional gap (like GP-S07 timeout evaluation).
- AvatarActor for player ASC still unlocked — must not invent in GP-S08.

## Remaining non-blocking decisions
- Exact AvatarActor for `AGP_PlayerState` ASC (`CameraPawn` vs `PlayerState` self vs other) — decide in GP-S09.
- Whether GP-S13 GameMode also sets `DefaultPawnClass` in same PR as CameraPawn (recommended yes) vs separate wiring.
- Exact log verbosity for deferred ASC (Verbose vs Log once).

## Open decisions OD-1…OD-10
All **RESOLVED** (tech-lead locks applied in implementation). Remaining non-blocking items above only.

## Linked canonical docs
- TDD/13 (class list + S08 order), TDD/03 (PlayerState ASC authority), TDD/04 (PC command owner — later), TDD/11 (camera / DefaultPawnClass / cursor), TDD/12 (UI on PC — later), TDD/00
- GDD First_Playable (PlayerState disconnect — later)
- GP-S07 `AGP_GameMode`, GP-S06 `AGP_GameState`, GP-S04 `UGP_AbilitySystemComponent`
- CONTRIBUTING / STYLE / Coding_Rules / Naming_Conventions

## Stop Condition
**STOP.** DONE.  
Tech lead accepted. Operator accepted.  
Do **not** start GP-S09. Do **not** auto-materialize the next task file.
