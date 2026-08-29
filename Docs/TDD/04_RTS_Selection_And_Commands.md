# RTS Selection and Commands

## Pipeline Overview

```
Enhanced Input (mapped IA_*)
   |
   v
AGP_PlayerController
   |
   +-- IA_Select (LMB)       --> UGP_SelectionComponent::OnSelectPressed
   +-- IA_Marquee (LMB drag) --> UGP_SelectionComponent::OnMarqueeUpdate / End
   +-- IA_Command (RMB)      --> UGP_CommandComponent::OnSmartCommand
   +-- IA_Stop (S)           --> UGP_CommandComponent::OnStop
   +-- IA_Attack (A + LMB)   --> UGP_CommandComponent::OnAttackTarget
   ...
   |
   v
UGP_SelectionComponent (local)
   - Holds TArray<TWeakObjectPtr<AGP_UnitBase>> SelectedUnits
   - Highlights selection visuals (cosmetic only)
   - Not replicated
   |
   v
UGP_CommandComponent (local intent)
   - Builds FGP_CommandRequest struct
   - Calls AGP_PlayerController::Server_RequestCommand
   |
   v
AGP_PlayerController::Server_RequestCommand (server RPC, WithValidation)
   - Validate ownership of selected units
   - Validate command applicability per unit type
   |
   v
UGP_CommandComponent::ExecuteServerCommand (server-side)
   - For each unit: forward to UnitBase->ReceiveCommand
   |
   v
AGP_UnitBase::ReceiveCommand (server-side)
   - Routes to appropriate component (Movement / Combat / Mining / etc.)
   - May activate GAS ability (Repair, etc.). Do **not** route a `Build` ability that constructs the READY building (ADR-0009). Future Level Terrain / foundation-install / Wall-construction assignment commands are local engineering jobs (ADR-0010; names TBD).
   |
   v
Behavior tick on server, replicated to clients via attributes + transform
```

## FGP_CommandRequest

```cpp
USTRUCT(BlueprintType)
struct FGP_CommandRequest
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag CommandTag;       // GP.Command.Move / Attack / Mine / etc.

    UPROPERTY()
    TArray<TWeakObjectPtr<AGP_UnitBase>> Targets;   // units issuing the command

    UPROPERTY()
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY()
    TWeakObjectPtr<AActor> TargetActor;             // optional (attack target, mine node, build target)

    UPROPERTY()
    bool bQueue = false;                            // shift-queue command
};
```

Single struct для всіх command types — спрощує RPC signature.

## UGP_SelectionComponent

### Responsibilities

- Maintain selected unit list (local-only).
- Highlight selected units (cosmetic):
  - Selection ring decal OR
  - Material parameter (e.g., `EmissiveBoost`) toggle.
- Marquee selection: rectangle drag → screen-to-world projection → overlap check.
- Filter selection by team (player can select only owned units).
- Expose selection to UI via delegate `OnSelectionChanged`.

### Storage

```cpp
UPROPERTY()
TArray<TWeakObjectPtr<AGP_UnitBase>> SelectedUnits;
```

Weak pointers — units можуть умирати під час selection (death cleans automatically next access).

### Not Replicated

Selection — purely local concern. Кожен player бачить тільки свій selection. Server не знає, що ти "виділив юніт".

## UGP_CommandComponent

### Responsibilities

- Translate local input intent → `FGP_CommandRequest`.
- Smart command logic:
  - RMB on ground → Move.
  - RMB on enemy unit → Attack.
  - RMB on resource node → Mine (if selection has Worker).
  - RMB on owned construction site → Repair (if available).
- Call `Server_RequestCommand` через owning PlayerController.
- (Optional MVP) Predictive marker: spawn ground pulse decal at command location immediately, removed on server confirm or timeout.

### Smart Command Resolution

```cpp
FGP_CommandRequest UGP_CommandComponent::BuildSmartCommand(const FHitResult& Hit) const
{
    FGP_CommandRequest Req;
    Req.Targets = LocalSelectionComponent->GetSelectedUnits();

    if (AGP_UnitBase* HitUnit = Cast<AGP_UnitBase>(Hit.GetActor()))
    {
        if (HitUnit->GetTeamId() != GetOwnerPlayerState()->GetTeamId())
        {
            Req.CommandTag = FGPGameplayTags::Get().Command_Attack;
            Req.TargetActor = HitUnit;
            return Req;
        }
        if (HitUnit->HasTag(FGPGameplayTags::Get().Resource_Node))
        {
            Req.CommandTag = FGPGameplayTags::Get().Command_Mine;
            Req.TargetActor = HitUnit;
            return Req;
        }
    }

    // Default: Move
    Req.CommandTag = FGPGameplayTags::Get().Command_Move;
    Req.TargetLocation = Hit.Location;
    return Req;
}
```

## Server-Side Execution

### Validation

`AGP_PlayerController::Server_RequestCommand_Implementation`:

1. `IsValid(GetPlayerState<AGP_PlayerState>())` — sanity.
2. Loop over `Request.Targets`:
   - `Target.IsValid()` (unit still alive).
   - `Target->GetTeamId() == LocalTeamId` (ownership).
   - `Target->GetUnitDefinition()->AllowedCommands.HasTagExact(Request.CommandTag)` (capability).
3. Якщо validation fails — log warning, optionally `Client_NotifyCommandRejected`.

### Dispatch

`UGP_CommandComponent::ExecuteServerCommand` (server-side, called from PlayerController):

```cpp
for (TWeakObjectPtr<AGP_UnitBase> Target : Request.Targets)
{
    if (!Target.IsValid()) continue;
    Target->ReceiveCommand(Request);
}
```

### Unit Routing

`AGP_UnitBase::ReceiveCommand`:

```cpp
void AGP_UnitBase::ReceiveCommand(const FGP_CommandRequest& Request)
{
    if (!HasAuthority()) return;

    const FGameplayTag& Cmd = Request.CommandTag;
    if (Cmd.MatchesTagExact(FGPGameplayTags::Get().Command_Move))
    {
        if (UGP_MovementComponent* Move = FindComponentByClass<UGP_MovementComponent>())
        {
            Move->MoveTo(Request.TargetLocation);
        }
    }
    else if (Cmd.MatchesTagExact(FGPGameplayTags::Get().Command_Attack))
    {
        if (UGP_CombatComponent* Combat = FindComponentByClass<UGP_CombatComponent>())
        {
            Combat->EngageTarget(Request.TargetActor.Get());
        }
    }
    else if (Cmd.MatchesTagExact(FGPGameplayTags::Get().Command_Mine))
    {
        if (UGP_MiningComponent* Mine = FindComponentByClass<UGP_MiningComponent>())
        {
            Mine->BeginMining(Request.TargetActor.Get());
        }
    }
    else if (Cmd.MatchesTagExact(FGPGameplayTags::Get().Command_Build))
    {
        // Route to GAS ability activation
        UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
        if (ASC)
        {
            FGameplayEventData Payload;
            Payload.Target = Request.TargetActor.Get();
            Payload.OptionalObject = Request.TargetActor.Get();
            Payload.EventTag = FGPGameplayTags::Get().Ability_Build;
            ASC->HandleGameplayEvent(FGPGameplayTags::Get().Ability_Build, &Payload);
        }
    }
    else if (Cmd.MatchesTagExact(FGPGameplayTags::Get().Command_Stop))
    {
        // Stop all behavior components
    }
}
```

Розгалуження по tag — це data routing, не switch hell. Кожен component відповідає за свою responsibility.

## Cosmetic Feedback (Predictive)

Допустимо immediate local presentation:
- Ground pulse decal on RMB click.
- Sound effect "command issued" SFX.
- Selection ring update.

Це не gameplay prediction — це UI feedback. Real authority — server.

## Future: Local Engineering Jobs (ADR-0010)

Gameplay command **concepts** only. Exact tags, input actions, and classes **do not exist yet**.

Orbital READY buildings are **not** this pipeline. Local engineering uses **plan first, work second**: player may define the job before Workers are present; progress starts only when an assigned Worker reaches a valid work position; 0 Workers = 0 progress; multiple Workers accelerate (formula TBD).

- **Level Terrain / site preparation:** planned BuildGrid-aligned zone (the zone is the job). Per-cell GREY / YELLOW. Zone sizing UX is **DESIGN REQUIRED**. Terrain converges **progressively** while Workers work.
- **Install foundation:** planned job on leveled cells; delivered MainBase stock; progressive per-cell labor. Consume/reserve moment **DESIGN REQUIRED**. No second Orbital spend.
- **Foundation Repair:** future planned job on damaged/destroyed cells. Tunables TBD.
- **Wall Construction:** planned path from Wall inventory; Workers assemble on terrain; **no Foundation required**. Terrain suitability TBD. Consume/reserve moment **DESIGN REQUIRED**. Segments operational on completion — not instant spawn on click.
- **Work presentation:** gameplay emits work-pulse start/end (~1 s presentation target). Blueprint owns Niagara. No hardcoded project Niagara in native Worker gameplay. Mining presentation is the reference pattern.

See [`../GDD/13_Terrain_Engineering_And_Foundations.md`](../GDD/13_Terrain_Engineering_And_Foundations.md) and [`16_Voxel_Terrain_And_Foundations.md`](16_Voxel_Terrain_And_Foundations.md).

## Out of MVP

- Command queue (Shift+RMB chain) — design exists у `bQueue`, але queue логіка може бути спрощена до 1-deep replace-only у MVP.
- Hold position. Patrol MVP is implemented (`GP.Command.Patrol`: current location ↔ one clicked point; two-click A→B patrol remains out of MVP).
- Formation movement.
- ~~Attack-move.~~ **Implemented by GP-S32A** (`GP.Command.AttackMove`, A → LMB ground) — see `Claude_Tasks/GP-S32A_Attack_Move_Reconciliation.md`. **Operator FULL PASS / FINALIZATION_READY_FOR_MERGE** (not yet merged).
- Smart auto-acquire targets without explicit attack command — **Idle auto-acquire delivered by GP-S30R**; Attack-Move acquisition while travelling by GP-S32A.
- Timed retaliation pursuit — **GP-S40R** (**FINALIZED / READY FOR MERGE**): after successful hostile damage, an idle mobile combat unit may pursue/engage that attacker for `GetRetaliationPursuitSeconds()`. Handoff to Attack requires existing sight range **and** `GPCombatLOS::HasLineOfSight`. Blocked LOS keeps retaliation-owned pursuit. Manual Held commands win. Not a player-visible command. Turret/Worker do not gain movement retaliation.

## Detailed Selection Rules (GP-0202)

Поглиблені правила selection поверх pipeline вище. Stage — design only (per [`Claude_Tasks/GP-0202_Selection`](../Development/Claude_Tasks/GP-0202_Selection.md)). Implementation — окремий ticket post-approval.

### Two Local States

`UGP_SelectionComponent` тримає **два** незалежних local-only поля:

```cpp
UPROPERTY()  TArray<TWeakObjectPtr<AGP_UnitBase>> SelectedUnits;    // commandable group
UPROPERTY()  TWeakObjectPtr<AActor>               InspectedTarget;  // single view-only
```

- `SelectedUnits` — owned units/buildings, що приймають команди.
- `InspectedTarget` — будь-який актор з capability `GP.Capability.Inspectable` (включно з ворожими/нейтральними), показує HP/name у HUD, **не приймає команди**.

Обидва — не реплікуються. Single delegate `OnSelectionChanged` тригериться для обох змін; UI підписується раз.

### Selection Cap

- **Hard cap = 24** одиниць у `SelectedUnits`.
- Overflow rule:
  - Marquee: при перевищенні — взяти 24 closest-to-cursor-в-screen-space.
  - Additive (Shift+LMB): truncate — не додавати після досягнення 24, log info-level.
  - Control group recall: truncate так само, dead-entries skipped first.
- Buildings зайняти cap = 1 (single-only, див. Mixed Selection).

### Selectable Capability — Tags

Нові tags (реєструються у `GPGASRuntime`):

```
GP.Capability.Selectable      // owned actor може бути доданий у SelectedUnits
GP.Capability.Inspectable     // будь-який actor може бути inspected (HP/name readout)
GP.Selection.Type.Unit        // routing tag для HUD panel
GP.Selection.Type.Building    // routing tag для HUD panel
```

UnitDefinition і BuildingDefinition отримують поле:

```cpp
UPROPERTY(EditAnywhere, Category = "GP|Capability")
FGameplayTagContainer CapabilityTags;
```

Per-data baseline:

| Asset | CapabilityTags |
| --- | --- |
| `DA_GP_Unit_Worker`, `DA_GP_Unit_SalvageWalker` | `{Selectable, Inspectable, Selection.Type.Unit}` |
| `DA_GP_Building_MainBase`, `DA_GP_Building_LogisticsHub`, `DA_GP_Building_DefensiveTurret` | `{Selectable, Inspectable, Selection.Type.Building}` |
| SWARM units | `{Inspectable, Selection.Type.Unit}` (no Selectable) |
| Ferronite deposits | `{Inspectable}` |

Team check (own vs enemy) — runtime, не tag-encoded. `SelectionComponent` дивиться `Actor->GetTeamId() == LocalTeamId` і `Capability.Selectable`. Якщо team mismatch, fallback на `Inspectable`-path.

### Friendly Input Rules

| Input | Behavior |
| --- | --- |
| LMB on owned + Selectable | Replace: `SelectedUnits = [hit]`. Building → single-only. |
| Shift + LMB on owned + Selectable | Additive: append (skip duplicates, truncate at 24). |
| Ctrl + LMB on already-selected | Remove single from `SelectedUnits`. |
| LMB drag (marquee) | Rect → screen-space center of each owned + Selectable actor inside rect; clamp to 24 closest-to-cursor. Buildings excluded from marquee (single-only). |
| Double-click LMB on unit | Select all units of same `UnitDefinition` currently visible on screen (frustum-only, не whole map), clamp 24. |
| LMB on ground | Clear both `SelectedUnits` і `InspectedTarget`. |
| Esc | Same as LMB on ground + cancel any modal hotkey (A/M/B). |

### Enemy / Neutral Rules (Inspect Mode)

| Input | Behavior |
| --- | --- |
| LMB on enemy/neutral + Inspectable | `InspectedTarget = hit`. `SelectedUnits` лишається без змін. |
| Shift + LMB on enemy | Ignored (no enemy mixing into commandable group). |
| Marquee covers enemies | Enemies ignored у marquee result. |
| Enemy dies | `InspectedTarget` resolves null → HUD hides inspect panel. |

Inspect — суто read-only. Command bar disabled при `SelectedUnits.IsEmpty()`. RMB при тільки `InspectedTarget` (без owned selection) — no command issued.

### Mixed Selection Rules

- **Units + Building у `SelectedUnits` — заборонено.**
  - LMB on building при SelectedUnits=[units] → replace: `SelectedUnits = [building]`.
  - LMB on unit при SelectedUnits=[building] → replace: `SelectedUnits = [unit]`.
  - Shift+LMB на іншому типі — ignored (no append).
- Причина: command bar context — per-type. Mixing зламає UI contract.

### Control Groups (Ctrl+1..9)

State (local, per-PC):

```cpp
TStaticArray<TArray<TWeakObjectPtr<AGP_UnitBase>>, 9> ControlGroups;
double LastGroupRecallTimes[9];   // for double-tap focus
```

| Input | Behavior |
| --- | --- |
| `Ctrl + N` (1..9) | Overwrite slot N з поточних `SelectedUnits`. Empty selection → clear slot. |
| `Ctrl + Shift + N` | Append `SelectedUnits` → slot N (skip duplicates, truncate at 24). |
| `N` | Replace `SelectedUnits` з slot N (filter dead, clamp 24). |
| `Shift + N` | Append slot N до `SelectedUnits` (skip duplicates, truncate). |
| `N` двічі (≤ 0.4 s) | Focus camera on slot centroid via `AGP_CameraPawn::FocusOnLocation(FVector)`. |

Constraints:

- Building може бути у control group (single-entry slot), але recall спрацьовує як "select that building".
- Mixing у slot заборонено (assign дотримується Mixed Selection rule на момент assign).
- Slot не персистує між матчами (no save), no replication.

### Highlight Implementation

**Material parameter:** `EmissiveBoost` (scalar) на MID per-unit.

Contract:

- `AGP_UnitBase::BeginPlay` створює MID для всіх mesh slots (`UMaterialInstanceDynamic::Create`).
- `AGP_UnitBase::SetSelectionHighlight(bool bSelected)` set `EmissiveBoost` 0 → `Config.HighlightBoost` (наприклад `2.5`).
- `UGP_SelectionComponent` викликає `Unit->SetSelectionHighlight` на add/remove у `SelectedUnits`. Inspect mode highlight — окремий scalar `InspectBoost` (наприклад `1.2`), тонший за selection.
- Team color tint — окремий scalar `TeamTint` на тих самих MID, set один раз на BeginPlay.

Trade-offs vs decal: менше draw calls, не потребує land-projection per unit, працює коректно на воді/uneven terrain. Cost — MID per unit (~few KB), acceptable для MVP cap 50.

### Deselect / Invalidation

| Trigger | Effect |
| --- | --- |
| Esc / LMB on ground | Clear both `SelectedUnits` і `InspectedTarget`, cancel modal hotkey. |
| Unit death (`OnDeath` delegate) | Explicit removal з `SelectedUnits` і всіх control groups, fire `OnSelectionChanged`. |
| Authority lost / disconnect | Clear all. |
| Map travel / match end | Clear all on `EndPlay`. |
| Actor ownership change | Out of MVP. |

**Death handling rule:** `TWeakObjectPtr` сам resolve у null on access, але UI не оновиться без сигналу. Тому `AGP_UnitBase` exposes `OnDeath` multicast delegate; `UGP_SelectionComponent` subscribes per added unit, on event — remove і fire `OnSelectionChanged`. На unsubscribe (deselect) — clear delegate.

### UI Surface

Canonical HUD IA: [`12_UI_Architecture`](12_UI_Architecture.md) and
[`GP-Production-HUD-Layout-Spec`](../Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md).
The previous SelectionPanel bottom-left / CommandBar bottom-center / InspectPanel split is **SUPERSEDED**.

| State | Widget slot | Notes |
| --- | --- | --- |
| `SelectedUnits` single unit | Bottom-center Selection/Info, single-entity mode | Icon, name, HP current/max + normalized, Damage, Armor, Move Speed, Attack Range (`AttackRangeCm`), conditional cargo. |
| `SelectedUnits` multi (units only) | Bottom-center Selection/Info, group mode | Authored visual **8×3** icon+health grid. Gameplay cap **24**. Row data may still include DisplayName. |
| `SelectedUnits` single building | Bottom-center Selection/Info, single-entity mode | Same Single fields; Attack Range is factual `AttackRangeCm` (0 or authored). Procurement does **not** replace this panel. |
| `InspectedTarget` (any) | Same bottom-center block | No separate overlapping InspectPanel slot. |
| Empty selection | Info empty; Context Action Grid idle | |
| Unit/group selected | Bottom-right Context Action Grid, Unit Action Mode | Move, Stop, Attack-Move, Patrol. Direct RMB Attack stays separate. |
| Building selected (not MainBase) | Bottom-right Context Action Grid, Building Action Mode | Contextual actions only. Not a procurement source. |
| MainBase selected | Bottom-right Building Actions include **PURCHASE** | Design / not implemented. PURCHASE → UNITS / BUILDINGS / DEFENSE in the same panel. |

### Edge Cases

| Case | Behavior |
| --- | --- |
| Marquee partially off-screen | Screen-space test still works on visible-portion centers. Off-screen units excluded. |
| Unit finishes construction during marquee | Marquee uses snapshot at mouse-release; новий unit потрапить у наступний select. |
| Control group з усіма мертвими | Recall дає `SelectedUnits = []`. HUD панель ховається. Slot не очищається (повторний assign overwrite-не). |
| Group recall while modal hotkey active (A/M) | Recall replace selection; modal hotkey зберігається. |
| LMB на UI panel | Selection layer ignored (`HUD::IsCursorOverInteractiveUI`). |
| Double-click через 0.4+ s | Не вважати double-click (per `Config.DoubleClickWindow`, default 0.4 s). |
| Selection while paused | Allowed. Inspect — allowed. Commands — заборонені (gameplay paused). |
| Marquee на 60 SWARM юнітах | SWARM не Selectable; marquee result порожній (own units only). |

### Validation (per GP-0202 stop condition)

- [x] Selection is client-local — `UGP_SelectionComponent` не реплікується, тримає `TArray<TWeakObjectPtr>`.
- [x] Selection не мутує gameplay state — пайплайн пише intent у `FGP_CommandRequest`, авторитет server-side.
- [x] Destroyed selected actor handled — `OnDeath` delegate + weak ptr null-cleanup.
- [x] Capability експоновано через tags + DataAsset CapabilityTags.

### Open Questions

1. **Camera rotate input divergence:** GDD/09 заявляє `Q/E` для rotate, TDD/11 закріплює MMB. Потрібна узгодження одним PR. Recommend: GDD updated to MMB (per [`11_RTS_Camera`](11_RTS_Camera.md) decision).
2. **`Space` — center camera on selection:** API contract requires `AGP_CameraPawn::FocusOnLocation(FVector)`. Додати у GP-0201A code task scope.
3. **Selection panel sort order** для multi-select — by `UnitDefinition` group? by health? Defer до UI implementation pass (GP-0401).
4. **Double-tap recall focus camera** — потребує camera API; залежить від Open Question #2.
5. **InspectedTarget for ground/empty space** — out of scope; ignored.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Single select / replace | LMB on unit A, LMB on unit B → SelectedUnits=[B]. |
| 2 | Shift-add cap | Shift+LMB 30 разів → SelectedUnits.Num()=24, log info-truncated. |
| 3 | Marquee | LMB drag over 10 owned units → SelectedUnits=[10]. |
| 4 | Marquee 30 closest-clamp | Drag over 30 owned → 24 closest-to-cursor у result. |
| 5 | Inspect enemy | LMB on enemy → InspectedTarget set, SelectedUnits unchanged. |
| 6 | Building single-only | LMB on building → SelectedUnits=[building], previous units dropped. |
| 7 | Mixed prevention | SelectedUnits=[units], LMB on building → replace, no mixed state. |
| 8 | Control group assign + recall | Select 10, Ctrl+1, deselect, press 1 → SelectedUnits=[10 same units]. |
| 9 | Dead unit purge | Unit у Ctrl+1, kill it, press 1 → recall без мертвого. |
| 10 | Double-tap recall | Press 1 двічі → camera focuses on group centroid. |
| 11 | Esc clear | Esc → both states clear, command bar disabled. |
| 12 | Death during selection | Unit у selection dies → panel updates immediately, no stale UI. |

### Out of MVP (Selection)

- Drag-rearrange selection panel order.
- Persistent control groups across matches.
- Subgroup tab cycling within selection.
- Hotkey rebinding UI (use `IMC_GP_Selection` + Enhanced Input remap layer post-MVP).
- Selection priorities / smart-select (e.g., select all wounded).

## Detailed Move Command Rules (GP-0203)

Stage — design only (per [`Claude_Tasks/GP-0203_Move_Command`](../Development/Claude_Tasks/GP-0203_Move_Command.md)). Decisions поверх pipeline і unit-side `UGP_MovementComponent` з [`05_Unit_Architecture`](05_Unit_Architecture.md).

### Intent Flow

Стандартний pipeline (див. вище):

```
RMB on ground / unit-not-enemy
  → UGP_CommandComponent::BuildSmartCommand (Move branch)
  → AGP_PlayerController::Server_RequestCommand(FGP_CommandRequest{CommandTag=GP.Command.Move, Targets, TargetLocation})
  → server validation
  → for each Target: UGP_MovementComponent::MoveTo(SnappedDestination)
```

### Server Validation (per move request)

1. **Owner alive sanity:** `IsValid(GetPlayerState<AGP_PlayerState>())`.
2. **Spam debounce:** server тримає `TMap<TWeakObjectPtr<APlayerController>, double> LastMoveRequestTimes`. Якщо `Now - Last < 0.050` (50 ms) — reject silently, log `Verbose`.
3. **Tag presence:** `Request.CommandTag.MatchesTagExact(GP.Command.Move)`.
4. **TargetLocation finite:** не NaN / не Inf.
5. **Per-target loop:**
   - `Target.IsValid()` — alive.
   - `Target->GetTeamId() == OwningPlayerTeamId` — client cannot move enemy units.
   - `Target->GetUnitDefinition()->AllowedCommands.HasTagExact(GP.Command.Move)`.
   - Target NOT has tag `GP.Unit.State.Dead` or `GP.Unit.State.Stunned`.
   - Target is `AGP_MobileUnit` (buildings reject Move).
6. **Destination snap:** server викликає `UNavigationSystemV1::ProjectPointToNavigation(TargetLocation, OutSnapped, FVector(Config.MaxNavSnapExtent))`. Default `MaxNavSnapExtent = 1500 cm`.
   - Success → use `OutSnapped` для всіх targets.
   - Failure → `Client_NotifyCommandRejected(GP.Command.Move, EReason::Unreachable)`; no move.
7. **Per-target dispatch:** `Target->ReceiveCommand(Request_with_SnappedLocation)` → routes to `UGP_MovementComponent::MoveTo`.

Жодні валідації не пишуть у gameplay state до повного успіху per-target. Часткові fail — log `Warning`, continue з рештою.

### Group Movement (24-cap)

- **Formation deferred** per [`ADR-0006`](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md). No formation object / facing persistence.
- **GP-S33M:** Move and AttackMove multi-select dispatch assigns **deterministic grid slots** around the click (`GroupSlotSpacingCm`), each nav-projected with ring fallback; failed projection keeps the desired grid offset (no center collapse). 1 unit → exact click.
- Soft unit separation during travel via MovementComponent overlap steering; static obstacles via NavMesh (not hard WorldStatic sweep block on unit capsules).
- Workers і Combat у одному selection — обидва дістають Move (spread slots); Mining/Combat поведінка перебивається move command (див. Command Interrupt).

### Spam Handling

- **Last-wins на side юніта:** новий `MoveTo` перезаписує `MovementComponent::Destination`, NavMesh re-paths без затримки.
- **Server-side debounce 50 ms per PlayerController** (див. validation step 2). Це не клієнтський throttle — клієнт може спамити, але RPC ігноруються.
- Спам розцінюється як норма для micro; debounce — захист проти flood/auto-clicker.

### Unreachable / Invalid Terrain

| Case | Resolution |
| --- | --- |
| Click on water / cliff / static mesh interior | `ProjectPointToNavigation` snap до closest navigable у `MaxNavSnapExtent`. |
| Click outside NavMesh entirely (за межами map nav) | Projection fails → `Client_NotifyCommandRejected(EReason::Unreachable)`. |
| Path exists, but NavMesh dynamically blocked (e.g., player put building) | Engine re-path on next tick; no extra logic. |
| Unit currently inside non-navigable region (spawned helper, kicked, etc.) | `MovementComponent::MoveTo` повертає fail; unit лишається; log `Verbose`. |
| TargetLocation NaN/Inf | Validation step 4 rejects. |

`Client_NotifyCommandRejected` — `Client_Unreliable` RPC, payload: `FGameplayTag CommandTag, uint8 Reason`. Reason enum:

```cpp
UENUM()
enum class EGP_CommandRejectReason : uint8
{
    Unknown = 0,
    Unreachable,
    NoOwnership,
    UnitDead,
    UnitStunned,
    DebounceThrottled,        // не надсилається — silent
    CapabilityMissing,
    ValidationFailed
};
```

### Command Interrupt

RMB Move на selection, що активно mining / combat / building:

- Server: `Target->ReceiveCommand(Move)` → у роутингу stop попередньої component activity (`MiningComponent::Cancel`, `CombatComponent::Cancel`).
- Чисто per-target; не блокує селекцію.
- Виняток: юніт з тагом `GP.Ability.State.Channeling` (наприклад, construct) ігнорує move до завершення/cancel. Reject reason: `CapabilityMissing` (можна uточнити enum).

### Feedback

**Local (predictive, cosmetic):**

| Channel | Trigger | Visual / Audio |
| --- | --- | --- |
| Ground decal | LMB click → smart Move issued | `M_GP_Decal_CommandPulse` зеленого team color, fade 0.6 s |
| SFX | Same | `SFX_GP_CommandIssued_Move` (один-шот, 2D) |
| Selection ring flash | All commanded units | Material parameter `EmissiveBoost` +0.5 на 200 ms |

**Server-confirmed:** server не шле positive ack — successful command вже видно через `MovementComponent` replication (юніт почав рух).

**Rejected:**

| Channel | Trigger | Visual / Audio |
| --- | --- | --- |
| Ground decal | `Client_NotifyCommandRejected` received within 250 ms of last issue | Red pulse `M_GP_Decal_CommandReject`, fade 0.4 s |
| SFX | Same | `SFX_GP_CommandDenied` |

Predictive cosmetic dec/decay не корелює з server delay; cosmetic відображення зникає за фіксованим часом незалежно від RPC. Це не gameplay prediction — server залишається authority.

### Death During Command

- `UGP_UnitAttributeSet::PostGameplayEffectExecute` → Health ≤ 0 → tag `GP.Unit.State.Dead` (loose).
- `AGP_MobileUnit::OnRep_Tags` / server `OnDeath` → `MovementComponent::StopMovementImmediately()`, `MovementComponent::AbortMove()`.
- `UGP_SelectionComponent` (local) ловить `OnDeath` через delegate (per GP-0202 spec) → remove з `SelectedUnits` / control groups, fire `OnSelectionChanged`.
- Командна RPC, що ще "в польоті" і прийшла на сервер після death: validation step 5 (Target.IsValid + Tag.Dead absent) rejects per-target. Loop completes; решта juniits, що живі, рухаються.

### Tag Surface

| Tag | Purpose | Owner |
| --- | --- | --- |
| `GP.Command.Move` | Required — command identity | `GPGASRuntime` native |
| `GP.Unit.State.Dead` | Blocks command exec | `GPGASRuntime` native (set by GAS) |
| `GP.Unit.State.Stunned` | Blocks command exec | reserved post-MVP, exists для майб. abilities |
| `GP.Ability.State.Channeling` | Blocks move interrupt | reserved post-MVP |

UnitDefinition `AllowedCommands` має містити `GP.Command.Move` для всіх `AGP_MobileUnit`-derived (workers, troopers, SalvageWalker).

### Authority Confirmation

- Client tells intent (`Server_RequestCommand`).
- Server validates, snaps, dispatches.
- Server-side `UGP_MovementComponent` ticks path; transform replicates через standard `UPawnMovementComponent`.
- Client отримує rolling transform updates, **не** runs simulation локально (no client-side prediction для RTS у MVP).

### Validation Checklist (Stop Condition)

- [x] Client cannot move enemy units — server ownership check (validation step 5).
- [x] Spam має defined behavior — last-wins + 50 ms server debounce.
- [x] Unit death during command — `AbortMove` + tag-gated per-target rejection.
- [x] Required tag `GP.Command.Move` зафіксовано.

### Open Questions

1. **`MaxNavSnapExtent` tuning:** 1500 cm — generous, може схапати unintended ground. Reduce to 800 cm після playtest?
2. **Move command на own building target:** RMB на own building — це поточно "default Move to building's location". OK? Або treat як garrison/drop-off intent (post-MVP)? Recommend keep Move у MVP.
3. **Reject feedback decal scale:** одна decal на rejected command чи per-unit? Recommend single global pulse (less noise).
4. **Channeling tag scope:** які abilities/states фіксують `GP.Ability.State.Channeling`? Defer до GP-0301/GP-0302 abilities spec.
5. **Pathfinding cost on group of 24:** NavMesh repath один-в-один — потенційно `~24 × RecastRequest` на кожен RMB. Profile у GP-0203A code task; consider shared path або path caching post-MVP.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Single worker move | RMB on grass → worker moves to clicked point. |
| 2 | Group 10 move | Marquee 10 workers, RMB → всі починають рух, спред через avoidance. |
| 3 | Group 24 move | Cap. RMB → всі 24 рухаються; no jitter beyond avoidance noise. |
| 4 | Move on cliff | RMB on impassable → red pulse, denied SFX, no move. |
| 5 | Move on water (no nav) | Same as #4 if outside snap range; otherwise snap to shore. |
| 6 | Spam RMB | 20 click/sec → юніти рухаються до останньої точки, server logs Verbose debounce. |
| 7 | Move enemy units | Select enemy via Inspect, RMB → no command issued (command bar disabled per GP-0202). |
| 8 | Interrupt mining | Worker mining → RMB elsewhere → mining cancels, worker moves. |
| 9 | Move while dying | Unit at 5 HP, takes lethal damage during path → path aborts on death; no zombie movement. |
| 10 | Move on opposite side of map | Path 200 m → unit completes via NavMesh repath as needed. |
| 11 | Reject feedback delay | Rejected command — decal/SFX appear within 250 ms of click. |
| 12 | Two-player concurrent move | Player A and B both RMB same point → independent paths, no interaction. |

### Out of MVP (Move-specific)

- Formation movement (line, wedge, box).
- Move queue (Shift+RMB chain) — `bQueue` exists, але implementation deferred to GP-0203B.
- ~~Attack-move (covered у GP-0204).~~ **GP-S32A** implements Attack-Move MVP — **FINALIZATION_READY_FOR_MERGE** (operator FULL PASS).
- ~~Patrol.~~ **Implemented MVP:** HUD PATROL / `EnterPatrolMode` → one LMB destination → `GP.Command.Patrol`. Anchor A = unit location at accept; Point B = clicked ground. Combat-capable units (`UGP_UnitCommandComponent::IsCombatCapable()`: live, valid attack range, damage > 0, not Worker) may temporarily engage via existing Attack FSM without replacing Held Patrol, then resume the same A/B leg. Attack-Move eligibility remains SalvageWalker-capability. Stop or a replacing explicit command cancels. Two-click A→B patrol remains out of MVP.
- Stance-aware move (aggressive, hold-fire) — post-MVP.
- Path preview indicator before click.

## Detailed Attack Command Rules (GP-0204)

Stage — design only (per [`Claude_Tasks/GP-0204_Attack_Command`](../Development/Claude_Tasks/GP-0204_Attack_Command.md)). Поверх GAS damage flow з [`02_GAS_Architecture`](02_GAS_Architecture.md) і `UGP_CombatComponent` / `UGP_TargetingComponent` з [`05_Unit_Architecture`](05_Unit_Architecture.md).

### Intent Sources

| Source | Tag | Behavior |
| --- | --- | --- |
| Smart RMB on enemy / building (Inspectable, hostile) | `GP.Command.Attack` | Explicit engagement. `TargetActor` set. |
| `A` hotkey + LMB on ground | `GP.Command.AttackMove` | Move to point з auto-acquire. `TargetLocation` set. |
| `A` hotkey + LMB on enemy | `GP.Command.Attack` | Same as RMB-on-enemy. |
| Auto-acquire (`UGP_TargetingComponent`) | Internal — no RPC | Idle combat unit detects enemy in range, hands off to `CombatComponent`. Local server-side decision, no command intent on the wire. |

`FGP_CommandRequest` reused для обох tags. Для AttackMove — `TargetLocation` обов'язковий, `TargetActor` порожній.

### Server Validation

Спільні для `GP.Command.Attack` і `GP.Command.AttackMove`:

1. **Owner sanity:** `IsValid(GetPlayerState<AGP_PlayerState>())`.
2. **Spam debounce:** окремий `LastAttackRequestTimes` map, 50 ms threshold (як Move).
3. **Tag match:** `Request.CommandTag.MatchesTagExact(Command_Attack | Command_AttackMove)`.
4. **Per-attacker loop:**
   - `Target.IsValid()` і живий (`!HasTag(GP.Unit.State.Dead)`).
   - `Target->GetTeamId() == OwningPlayerTeamId` (own units only issue command).
   - `!Target->HasTag(GP.Unit.State.Stunned)`.
   - `Target->GetUnitDefinition()->AllowedCommands.HasTagExact(Request.CommandTag)`.
   - Target instance of `AGP_MobileUnit` (buildings cannot attack у MVP; reserved для defensive turrets через окрему ability).

For `GP.Command.Attack` додатково:

5. **TargetActor validation:**
   - `Request.TargetActor.IsValid()` і живий.
   - `TargetActor` не той самий, що Target (no self-attack).
   - `TargetActor->HasTag(GP.Capability.Inspectable)` (target must be visible/legible).
   - **Friendly fire:** team check skipped — explicit attack on ally allowed. Auto-acquire team check залишається (див. нижче).
6. **Initial range check:** не потрібен — `CombatComponent` сам route через movement якщо out of range.

For `GP.Command.AttackMove`:

5. **Destination snap:** ті ж правила, що Move (`ProjectPointToNavigation`, `MaxNavSnapExtent=1500cm`). Fail → `Client_NotifyCommandRejected(Unreachable)`.

### Range Check & Line-of-Sight

Per attack tick у `UGP_CombatComponent` (server-only):

```cpp
bool UGP_CombatComponent::CanFireAt(const AGP_UnitBase* Target) const
{
    if (!Target || Target->IsDead()) return false;

    const float Range = AttackerASC->GetNumericAttribute(UGP_UnitAttributeSet::GetAttackRangeAttribute());
    const FVector AttackerLoc = Owner->GetActorLocation();
    const FVector TargetLoc   = Target->GetActorLocation();
    if (FVector::DistSquared(AttackerLoc, TargetLoc) > FMath::Square(Range)) return false;

    return HasLineOfSight(Target);
}
```

**LOS — multi-point trace:** три line traces у каналі `ECC_Visibility` від attacker до target. Це навмисно forgiving: якщо хоча б одна лінія чиста — hit valid (combat units не "застрягають" за низькими укриттями).

```
Source points (on attacker):
  - Socket "AttackOrigin_Eye"   (fallback: bounds Top)
  - Socket "AttackOrigin_Chest" (fallback: bounds Center)
  - Socket "AttackOrigin_Feet"  (fallback: bounds Bottom + 10 cm)

Target points (on target):
  - Socket "Hit_Head"   (fallback: bounds Top)
  - Socket "Hit_Chest"  (fallback: bounds Center)
  - Socket "Hit_Feet"   (fallback: bounds Bottom + 10 cm)

Trace pairs (3 total):
  Eye   → Head
  Chest → Chest
  Feet  → Feet

If ANY trace returns !bBlockingHit OR HitActor == Target → LOS valid.
```

Trace channel `ECC_Visibility` configured у Project Settings (collision profile already у default). Sockets — convention; `AGP_UnitBase::GetAttackPoints()` API повертає resolved world locations (out-of-scope для spec, документуємо у GP-0204A code task).

### Out-of-Range / LOS Broken

`UGP_CombatComponent` server tick:

```
if CanFireAt(Target):
    if !HasTag(GP.Unit.State.AttackCooldown):
        ApplyDamage()
        ApplyCooldown()
        Multicast_PlayAttackVFX()
else:
    MovementComponent->MoveTo(Target.Location)   // chase
```

- Chase tick rate: 0.5 s (configurable у DataAsset).
- Якщо `Target` рухається — `MoveTo` оновлює destination кожен tick.
- Target dies during chase → `OnTargetDeath` handler (див. нижче).

### Damage Authority

Server-only:

```cpp
FGameplayEffectContextHandle Ctx = AttackerASC->MakeEffectContext();
Ctx.AddSourceObject(Owner);
FGameplayEffectSpecHandle Spec = AttackerASC->MakeOutgoingSpec(GE_GP_Damage_Basic, Level=1.f, Ctx);
AttackerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
AttackerASC->ApplyGameplayEffectToSelf(GE_GP_Cooldown_Attack, Level=1.f, Ctx);
```

- `GE_GP_Damage_Basic` — Instant, magnitude через `UGP_DamageCalculation`. Reads source `Damage`, target `Armor` + `DamageResistance`. Outputs Health modifier.
- `GE_GP_Cooldown_Attack` — Duration, grants `GP.Unit.State.AttackCooldown`. Duration = `1.0 / AttackSpeed`.
- Client ніколи не apply damage / cooldown. Cheating через client = impossible.

### Multicast Cosmetic

`Multicast_PlayAttackVFX(FVector TargetLoc, AActor* TargetActor)` — `Unreliable`. Triggers:

- Attack animation (montage).
- Muzzle flash / weapon VFX.
- Projectile spawn (MVP — hitscan-style instant impact effect).
- Hit VFX at TargetActor location.
- Sound: attack SFX (3D, attenuated).

Damage не залежить від cosmetic — VFX може теряти, damage все одно applied.

### Auto-Acquire (`UGP_TargetingComponent`)

Server-only. Tick rate: 0.5 s (configurable). Per idle combat unit:

```
candidates = OverlapSphere(AttackerLoc, AcquireRange) filter to:
    - HasTag(GP.Capability.Inspectable)
    - TeamId != AttackerTeamId          // friendly fire NEVER via auto-acquire
    - !HasTag(GP.Unit.State.Dead)
    - HasLineOfSight(candidate)         // single-trace cheap variant; full multi-trace тільки при actual fire
sort by squared distance
target = candidates[0]
if target != null:
    CombatComponent->EngageTarget(target)
```

- `AcquireRange` = `AttackRange * 1.25` (configurable per `UnitDefinition`).
- Activates тільки якщо `!CombatComponent->HasEngagement()` AND `!MovementComponent->HasOrders()` (except attack-move).
- Auto-acquire **не активний для workers** — `UnitDefinition.bAutoAttacks=false`. Worker = peaceful, потребує explicit command.

`UnitDefinition` нове поле:

```cpp
UPROPERTY(EditAnywhere, Category = "GP|Combat")
bool bAutoAttacks = true;          // false for Worker / non-combat

UPROPERTY(EditAnywhere, Category = "GP|Combat")
float AutoAcquireRangeMultiplier = 1.25f;
```

### Attack-Move

State on `AGP_MobileUnit`:

```cpp
UPROPERTY(Replicated)  FVector AttackMoveDestination = FVector::ZeroVector;
UPROPERTY(Replicated)  bool    bAttackMoving = false;
```

Flow:

1. Server receives `GP.Command.AttackMove` → set `AttackMoveDestination`, `bAttackMoving=true`, `MovementComponent->MoveTo(Snapped)`.
2. Кожен `TargetingComponent` tick — якщо в range з'явився enemy → `CombatComponent->EngageTarget`, `MovementComponent->Pause()` (зберігає `AttackMoveDestination`).
3. Combat фіналізує (`OnTargetDeath` або out-of-acquire-range) → перевіряє `bAttackMoving`:
   - Так → `MovementComponent->MoveTo(AttackMoveDestination)`, продовжує scan.
   - Ні → idle.
4. Player issues plain `Move` → `bAttackMoving=false`, `AttackMoveDestination` clear, normal move.
5. Player issues `Stop` → both cleared.

Replication: `AttackMoveDestination` + `bAttackMoving` реплікуються тільки для local rendering (e.g., A-move indicator decal). Не для simulation на клієнті — server-authoritative.

### Target Loss / Death Handling

`UGP_CombatComponent::OnTargetDeath` (delegate from `Target->OnDeath`):

```
- Clear engagement.
- If bAttackMoving:
    - Re-run TargetingComponent immediately (skip 0.5s delay).
    - If new target acquired: engage.
    - Else: MovementComponent->MoveTo(AttackMoveDestination).
- Else:
    - Idle. Wait for TargetingComponent next tick.
```

`Target.IsValid()` test також на кожному `CombatComponent` tick — guard проти target destroyed без OnDeath fire (edge case при actor stream-out / level change).

### Building Attacks

- Building = `AGP_BuildingBase : AGP_UnitBase` (per [`06_Building_Architecture`](06_Building_Architecture.md), `ADR-0007`).
- Same Health attribute → same damage flow без додаткової логіки.
- LOS multi-trace до building: sockets `Hit_Head/Chest/Feet` placed по bounding box corners (top/center/bottom of largest face).
- MainBase destruction → `AGP_GameMode::OnUnitDied` дивиться на `UnitDefinition.UnitTags.HasTagExact(GP.Building.Type.MainBase)` → trigger match end per [`GDD/08_Win_Lose_Conditions`](../GDD/08_Win_Lose_Conditions.md). **Реальна логіка match-end** документується у GP-0301.

### Friendly Fire Semantics

| Source | FF Allowed? |
| --- | --- |
| Explicit `GP.Command.Attack` з `TargetActor.TeamId == AttackerTeamId` | **Yes** — server validation skips team check для explicit attack. |
| `GP.Command.AttackMove` auto-acquire scan | **No** — `TargetingComponent` filter excludes same team. |
| Idle auto-acquire | **No** — same filter. |
| Mind-control / debuff (post-MVP) | Out of scope. |

UI feedback: при наведенні A-mode cursor на ally — show neutral/yellow cursor color (not red enemy). Detail у HUD pass (GP-0401).

**Rationale:** explicit FF — це власна стратегічна помилка гравця, не grief surface (тільки 2 player, тільки allies під одним PC у MVP — реально лише own units). Auto-acquire FF — grief vector, тому hard-block.

### Damage → Match End Path

Не дублюємо GP-0301 spec. Коротко:

```
GE_GP_Damage_Basic on MainBase
  → UGP_UnitAttributeSet::PostGameplayEffectExecute
  → Health <= 0
  → MainBase set tag GP.Unit.State.Dead
  → AGP_GameMode::OnUnitDied(MainBase, AttackerOwner)
  → Check UnitTags.HasTagExact(GP.Building.Type.MainBase)
  → AGP_GameMode::EndMatch(Winner=AttackerOwner.TeamId, Reason=BaseDestroyed)
```

Detail rule у GP-0301. Тут — confirm hookup існує.

### Tag Surface

| Tag | Owner | Purpose |
| --- | --- | --- |
| `GP.Command.Attack` | native (existing) | Explicit attack command identity. |
| `GP.Command.AttackMove` | native (new) | Attack-move command identity. |
| `GP.Unit.State.Attacking` | loose (set by CombatComponent) | Visual / SFX gating, optional analytics. |
| `GP.Unit.State.AttackCooldown` | from `GE_GP_Cooldown_Attack` (existing у TDD/02) | Blocks next fire. |
| `GP.Capability.Inspectable` | DataAsset (per GP-0202) | Required on any target to receive attack. |

UnitDefinition `AllowedCommands` для combat units: `{Command_Move, Command_Attack, Command_AttackMove, Command_Stop}`.

### Validation Checklist (Stop Condition)

- [x] Client cannot fake damage — server-only `ApplyGameplayEffectSpecToTarget`. Client має тільки `Multicast_PlayAttackVFX` cosmetic.
- [x] Target destroyed mid-command — `OnTargetDeath` delegate clears engagement, attack-move resumes or idles.
- [x] Base damage → match end — chain через `OnUnitDied` + `GP.Building.Type.MainBase` tag → `EndMatch`.
- [x] Required tag `GP.Command.Attack` зафіксовано; `GP.Command.AttackMove` додано.

### Open Questions

1. **Projectile vs hitscan:** MVP usage — hitscan instant impact. Projectile travel time (post-MVP) потребує client-side projectile actor + server damage authority sync.
2. **AttackRange units:** cm, як AttackRange attribute у TDD/02. Worker melee = ~150 cm? Trooper ranged = ~1500 cm? Defer balance до GP-0302/0304.
3. **Self-cast / area abilities:** не covered у GP-0204; this spec — single-target attack only.
4. **Building attack collateral:** building hit explosion → damage nearby? Out of MVP. Single-target only.
5. **Reveal-on-attack:** attack from fog of war → reveal attacker? FoW не у MVP; defer.
6. **A-mode cursor visual:** cursor change при A-mode pending? UI pass (GP-0401).
7. **LOS performance:** 3 traces × N attackers × per-tick. Profile у GP-0204A; optional shared LOS cache per attacker pair within frame.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Trooper attacks enemy worker | RMB on enemy → trooper chases (if out of range), fires, kills, idles. |
| 2 | A-move sweep | A + LMB across map → trooper moves, auto-engages enemies en route, resumes after kill. |
| 3 | LOS — low cover | Enemy ducked behind low wall, head visible → Eye→Head trace passes, fire valid. |
| 4 | LOS — full cover | Enemy behind tall wall, no trace clear → fire blocked, trooper chases for LOS. |
| 5 | Range edge | Enemy at AttackRange + 10 cm → trooper micro-walks 10 cm, fires. |
| 6 | Worker passive | Worker selected, enemy walks adjacent → worker does NOT attack (no auto-acquire). |
| 7 | Worker explicit attack | RMB on enemy with worker selected → если `bAutoAttacks=false` AND `AllowedCommands.HasTagExact(Attack)` is false → reject; worker can't attack. |
| 8 | Friendly fire explicit | Attack-RMB on own unit → trooper fires (allowed by explicit). Cosmetic FF feedback shown. |
| 9 | Friendly fire auto | Trooper idles next to own units only → no auto-fire. |
| 10 | Target dies mid-attack | Two troopers vs one enemy → first kills, second re-acquires next target (auto-acquire) or idles. |
| 11 | A-move re-engage | Trooper A-moves, kills enemy → resumes path to original destination. |
| 12 | Base destruction | All damage applied to MainBase → Health 0 → match ends, winner correct. |
| 13 | Cooldown enforcement | Trooper fires faster than `AttackCooldown` allows → blocked by tag. |
| 14 | Death during chase | Trooper chases low-HP enemy that dies before reaching → chase aborts, idle (or attack-move resume). |
| 15 | Two attackers, one target | Both fire concurrently → no race on damage; GAS handles serial application. |

### Out of MVP (Attack-specific)

- Projectile travel time / dodging.
- Area-of-effect attacks.
- Splash damage.
- Critical hits / armor pen abilities.
- Stance system (aggressive / hold-fire / hold-position).
- Reload mechanics.
- Reveal-on-attack / fog of war integration.
- Friendly-fire UI confirmation prompts.

## References

- Multiplayer authority — [`03_Multiplayer_Architecture`](03_Multiplayer_Architecture.md).
- GAS event hookup — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- Unit components — [`05_Unit_Architecture`](05_Unit_Architecture.md).
- Building architecture — [`06_Building_Architecture`](06_Building_Architecture.md).
- Win/Lose conditions — [`../GDD/08_Win_Lose_Conditions`](../GDD/08_Win_Lose_Conditions.md).
- Camera focus API contract — [`11_RTS_Camera`](11_RTS_Camera.md).
- Selection task — [`../Development/Claude_Tasks/GP-0202_Selection`](../Development/Claude_Tasks/GP-0202_Selection.md).
- Move task — [`../Development/Claude_Tasks/GP-0203_Move_Command`](../Development/Claude_Tasks/GP-0203_Move_Command.md).
- Attack task — [`../Development/Claude_Tasks/GP-0204_Attack_Command`](../Development/Claude_Tasks/GP-0204_Attack_Command.md).
