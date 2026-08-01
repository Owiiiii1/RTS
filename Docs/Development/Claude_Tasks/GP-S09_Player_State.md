# GP-S09 — AGP_PlayerState
(Player ASC Ownership and Player AttributeSet)

## Slice Group
Slice 2 — Match Flow + Asset Loader

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S08 DONE — `AGP_PlayerController` queries ASC via `IAbilitySystemInterface` (no PC API change in S09).
- GP-S07 DONE — `AGP_GameMode` (gains `PlayerStateClass` wiring only).
- GP-S04 DONE — `UGP_AbilitySystemComponent` (`SetProjectReplicationMode`, `InitAbilityActorInfo`).
- GP-S03 DONE — `UGP_PlayerAttributeSet` (attrs + `GetLifetimeReplicatedProps`).
- Module `GPRuntime` (depends on `GPGASRuntime`).

## Goal
Implement authoritative replicated `AGP_PlayerState : APlayerState, IAbilitySystemInterface` in `GPRuntime` that owns one `UGP_AbilitySystemComponent` and one `UGP_PlayerAttributeSet`, initializes GAS actor-info with **Owner = Avatar = PlayerState**, wires `PlayerStateClass` on `AGP_GameMode`, and lets the existing PlayerController ASC query return a real ASC — without economy gameplay, TeamId, CameraPawn, abilities, UI, or commands.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S10 until explicitly assigned (do not auto-materialize GP-S10 task file).

### Closed with
- `AGP_PlayerState : APlayerState, IAbilitySystemInterface`.
- One `UGP_AbilitySystemComponent` default subobject; ASC replicated; mode **Mixed**.
- One `UGP_PlayerAttributeSet` default subobject.
- `GetAbilitySystemComponent` returns project ASC; typed ASC getter; const PlayerAttributeSet getter.
- OwnerActor = PlayerState; AvatarActor = PlayerState; `InitAbilityActorInfo(this, this)`.
- Idempotent actor-info helper; `BeginPlay` + `ClientInitialize` initialization; no Pawn-dependent re-init.
- No PlayerController changes; `AGP_GameMode` sets `PlayerStateClass = AGP_PlayerState`.
- No TeamId / FactionId / ready state; no startup GameplayEffect; no hardcoded initial attributes.
- No RPC; no Tick; no new replicated PlayerState gameplay fields; no assets / Blueprint / map / config.
- Operator Editor/PIE validation **PASSED**.
- Runtime PlayerStateClass / ASC / listen-server proof **deferred** until temporary GameMode wiring.
- GP-S10 not started.

---

## Tech-lead locks (OD-1…OD-14) — RESOLVED

### OD-1 — RESOLVED: ASC ownership
- Exactly one `UGP_AbilitySystemComponent` via `CreateDefaultSubobject` on `AGP_PlayerState`.
- ASC replicated (`SetIsReplicated(true)` / equivalent).
- Mode = **`EGameplayEffectReplicationMode::Mixed`** via existing `SetProjectReplicationMode(Mixed)` **before** `InitAbilityActorInfo`.
- No ASC on PlayerController or Pawn.

### OD-2 — RESOLVED: AttributeSet ownership
- Exactly one `UGP_PlayerAttributeSet` via `CreateDefaultSubobject` on PlayerState.
- No second instance; not on PC/Pawn.
- Attribute replication stays in `UGP_PlayerAttributeSet::GetLifetimeReplicatedProps`.
- No PlayerState mirror UPROPERTYs for attrs.

### OD-3 — RESOLVED: API
```cpp
virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
UGP_AbilitySystemComponent* GetGPAbilitySystemComponent() const; // BlueprintPure OK
const UGP_PlayerAttributeSet* GetPlayerAttributeSet() const;     // BlueprintPure OK
```
- Non-const AttributeSet getter only if proven necessary (default: const only).
- No BlueprintCallable mutation API.
- `VisibleAnywhere` / `BlueprintReadOnly` on subobjects for inspection (project convention).

### OD-4 — RESOLVED: OwnerActor / AvatarActor = Option A
```cpp
AbilitySystemComponent->InitAbilityActorInfo(this, this);
```
- OwnerActor = `AGP_PlayerState`
- AvatarActor = `AGP_PlayerState`

Rationale (permanent for this player ASC, not temporary until GP-S13):
- ASC is player/account/economy-level; attrs belong to the player, not CameraPawn.
- State must survive pawn replacement.
- `AGP_CameraPawn` is absent and TDD/11 marks it non-replicated — unsuitable as authoritative GAS Avatar.
- Camera possession must not change player ASC actor info.
- Future pawn/unit abilities use that actor’s ASC; they do **not** rebind player ASC Avatar.

Note: GP-S04 Owner≠Avatar diagnostic will **not** fire for this policy (Owner == Avatar → Verbose path).

### OD-5 — RESOLVED: Initialization lifecycle
| Phase | Responsibility |
| --- | --- |
| Constructor | Create/configure subobjects only (ASC + AttributeSet, Mixed, replicate ASC). No `InitAbilityActorInfo`. |
| `BeginPlay` | Call idempotent `InitializeAbilitySystemActorInfo()`. |
| `ClientInitialize(AController* C)` | `Super::ClientInitialize(C)` then same helper. |

**Forbidden:** Tick, polling, pawn possess callbacks, `OnPawnSet`, Controller concrete casts, `OnRep_Owner` without proven need, `PostInitializeComponents` for final actor-info lock.

Helper:

```cpp
void InitializeAbilitySystemActorInfo();
```

Idempotency (preferred):
1. If ASC null → return (Error log).
2. Read current `AbilityActorInfo` (engine `AbilityActorInfo` / `GetAvatarActor()` / `GetOwnerActor()` — use safest public ASC API available in UE 5.8).
3. If OwnerActor already `this` **and** AvatarActor already `this` → **no-op**.
4. Else → `InitAbilityActorInfo(this, this)`.

Fallback if both pointers cannot be read safely: re-calling `InitAbilityActorInfo(this, this)` is allowed (GAS re-init OK), but implementation **must prefer** the explicit check and document which ASC API was used in the implementation report.

### OD-6 — RESOLVED: Client initialization
- Subobjects exist from constructor/CDO on server and clients.
- `BeginPlay` performs base init.
- `ClientInitialize` after Super re-guarantees actor info after controller association.
- Existing `AGP_PlayerController::OnRep_PlayerState` / `BeginPlayingState` then query ASC via ASI.
- **No** PC API change; **no** ASC RPC / OnRep for ASC pointer.

### OD-7 — RESOLVED: GameMode wiring
```cpp
PlayerStateClass = AGP_PlayerState::StaticClass();
```
in `AGP_GameMode` constructor only (cpp include). Do **not** change PlayerControllerClass, GameStateClass, DefaultPawnClass, map/config, Blueprint.

### OD-8 — RESOLVED: Net update policy
- Engine `APlayerState` already sets `bAlwaysRelevant = true` and a low default `NetUpdateFrequency` (engine ctor; typically 1 Hz) — **do not duplicate** `bAlwaysRelevant` unless implementation proves otherwise for UE 5.8.1.
- Do **not** set `NetUpdateFrequency = 100.0f`.
- Do not copy Lyra/sample tuning.

### OD-9 — RESOLVED: Attribute replication
No PlayerState attr mirrors. Do not change COND_* in S09.

| Attribute | Replication |
| --- | --- |
| OrbitalFerronite | OwnerOnly |
| FerroniteScore | all (`COND_None`) |
| MaxUnits | OwnerOnly |
| CurrentUnits | OwnerOnly |

### OD-10 — RESOLVED: Team / score support fields
Do **not** add TeamId, FactionId, ready, connection state, score mirror, timeout fields.  
`FerroniteScore` remains AttributeSet-only. GameMode TimerScore evaluation stays deferred (GP-S07 gap).

### OD-11 — RESOLVED: Initial attributes
Use AttributeSet defaults (0). No startup GE. No hardcoded economy. No `SetNumericAttributeBase` in PlayerState.

### OD-12 — RESOLVED: Mixed replication semantics
ASC replicated + Mixed; Owner/Avatar = PlayerState; controller association via standard PS lifecycle; `ClientInitialize` re-ensures actor info. No custom owner replication.

### OD-13 — RESOLVED: PlayerController integration
Do **not** modify `AGP_PlayerController`. Existing ASI getter + OnRep/BeginPlayingState hooks suffice once PS class is wired.

### OD-14 — RESOLVED: Pawn changes
Pawn is **not** AvatarActor. No re-init on possess/unpossess. CameraPawn replacement irrelevant to player ASC. No PS→PC concrete dependency. No pawn delegates.

---

## Canonical responsibilities

### AGP_PlayerState
- Authoritative replicated player GAS container.
- Owns ASC + PlayerAttributeSet.
- Implements `IAbilitySystemInterface`.
- Survives pawn replacement with stable Owner/Avatar = self.
- Does not own camera, input, match timer, selection, or commands.

### Not PlayerState (this slice)
- Economy mutation / orbital spend / score evaluation / TimerScore FinishMatch.
- Team / faction / disconnect / lobby ready.
- CameraPawn, IMC, UI, abilities grant.

## Network existence model
- Spawned per player connection via GameMode `PlayerStateClass`.
- Exists on server + all clients (engine AlwaysRelevant).
- ASC Mixed; AttributeSet COND_* as OD-9.

## ASC ownership
OD-1. One CDS `UGP_AbilitySystemComponent`; Mixed; replicated.

## AttributeSet ownership
OD-2. One CDS `UGP_PlayerAttributeSet`.

## Existing attribute inventory
See OD-9 table. Do not expand. TDD/02 Resource/modifiers list is stale — ignore for S09.

## IAbilitySystemInterface contract
`GetAbilitySystemComponent()` → owned ASC. Typed getters per OD-3.

## OwnerActor / AvatarActor model
Permanent Option A: `InitAbilityActorInfo(this, this)`. No pawn Avatar ever for this ASC.

## Server initialization lifecycle
1. Constructor: CDS ASC + AttributeSet; `SetIsReplicated(true)`; `SetProjectReplicationMode(Mixed)`.
2. `BeginPlay` → `InitializeAbilitySystemActorInfo()` (idempotent).
3. No economy bootstrap.

## Client initialization lifecycle
1. Constructor subobjects.
2. `BeginPlay` → helper.
3. `ClientInitialize` → Super → helper.
4. PC OnRep_PlayerState discovers non-null ASC via ASI.

## Pawn replacement behavior
None for ASC actor info (OD-14).

## PlayerController integration
Zero PC file changes (OD-13).

## GameMode wiring
`PlayerStateClass` only (OD-7).

## Replication mode
Mixed (OD-1 / OD-12).

## Net update policy
Engine PlayerState defaults; no NetUpdateFrequency=100; no redundant AlwaysRelevant assign (OD-8).

## Initial attribute policy
Defaults 0; no startup GE; no SetNumericAttributeBase (OD-11).

## Exact planned API

```cpp
UCLASS()
class GPRUNTIME_API AGP_PlayerState
	: public APlayerState
	, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGP_PlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GP|AbilitySystem")
	UGP_AbilitySystemComponent* GetGPAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Attributes")
	const UGP_PlayerAttributeSet* GetPlayerAttributeSet() const;

protected:
	virtual void BeginPlay() override;
	virtual void ClientInitialize(AController* C) override;

private:
	void InitializeAbilitySystemActorInfo();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|AbilitySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Attributes", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_PlayerAttributeSet> PlayerAttributeSet;
};
```

Constructor also: Tick disabled if project convention for Info actors applies (optional; engine PlayerState typically does not tick).

## Out of Scope
- CameraPawn / DefaultPawnClass
- TeamId / Faction / ready / bConnected
- TimerScore / FinishMatch evaluation
- Startup GE / granted abilities / attr expansion
- Input / cursor / selection / commands / UI
- PC API changes / ASC RPC
- Blueprint / map / ini
- GP-S10+

## Files delivered
- `GP/Source/GPRuntime/Public/Player/GPPlayerState.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerState.cpp`
- `GP/Source/GPRuntime/Private/Game/GPGameMode.cpp` — `PlayerStateClass` only
- `GPPlayerController.*` — **unchanged**

## Acceptance Criteria (implementation)
- [x] Compiles (GPEditor Dev, GP Dev, GP Shipping).
- [x] `AGP_PlayerState` + ASI; ASC Mixed; AttributeSet present.
- [x] `InitAbilityActorInfo(this, this)` via idempotent helper from BeginPlay + ClientInitialize.
- [x] GameMode `PlayerStateClass` wired; no DefaultPawnClass / PC / GameStateClass / map / ini changes.
- [x] PC sources unchanged; ASI path ready for real ASC.
- [x] No TeamId / economy GE / UI / abilities / attr mirrors.
- [x] No pawn-driven ASC re-init.
- [x] No GP-S10 bundled.
- [x] Operator Editor validation (Class Viewer / PIE without map wiring) PASSED.
- [x] PlayerStateClass runtime proof — **deferred** (accepted for close).
- [x] Listen-server / ASC runtime proof — **deferred** (accepted for close).
- [x] Tech lead accepted / operator accepted → DONE.

## Manual Editor validation (operator — no assets committed by agent)
1. Open project — no module/load errors.
2. Class Viewer — `AGP_PlayerState` and `AGP_GameMode` visible.
3. PIE on current map (without assigning AGP_GameMode) — must not break.
4. **Deferred runtime proof** (temporary wiring, do not commit): assign `AGP_GameMode`; confirm PS class + ASC via PC getter; attrs default 0.

## Listen-server validation plan
Host + client: both receive `AGP_PlayerState`; owner sees OwnerOnly attrs; both see FerroniteScore (still 0); PC ASC query works; pawn possess does not re-init Avatar.

## Risks / edge cases
- Calling Init before ASC fully ready — helper null-checks ASC.
- Double BeginPlay + ClientInitialize — idempotency required.
- Stale TDD/02 AttributeSets list must not drive new attrs.
- GP-S07 timeout still cannot FinishMatch until later score-read slice — intentional.

## Remaining non-blocking
- Idempotency uses `UAbilitySystemComponent::GetOwnerActor()` + `GetAvatarActor()` (UE 5.8 public API).
- `PrimaryActorTick.bCanEverTick = false` set explicitly for hygiene.

## Open decisions OD-1…OD-14
All **RESOLVED** and implemented.

## Linked canonical docs
- TDD/13, TDD/03, TDD/02 (Mixed / PS ASC; AttributeSets section partly stale), TDD/07, TDD/11, TDD/12
- GDD/06, GDD/08, First_Playable
- GP-S03, GP-S04, GP-S07, GP-S08
- Disk ASC / PlayerAttributeSet / PlayerController / GameMode

## Stop Condition
**STOP.** DONE.  
Tech lead accepted. Operator accepted.  
Do **not** start GP-S10. Do **not** auto-materialize the next task file.
