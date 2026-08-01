# SUPERSEDED — DO NOT IMPLEMENT
This section describes the pre-orbital-delivery architecture (local production/construction). Retained for historical reference only. Current canonical direction: ADR_0009 (Orbital Delivery) + current TDD/GDD.

> Archived from `TDD/06_Building_Architecture.md` on the Orbital Delivery pivot. The active building architecture (orbital drop, containers, build grid, walls, sell/demolish) now lives in `TDD/06_Building_Architecture.md`. Everything below predates that pivot: local construction sites, per-building production queues, rally points, Barracks/Assembly Yard. None of it is implemented.

## Class Hierarchy (Pre-Pivot)

```
AGP_UnitBase
  AGP_BuildingBase                   // static units; owns ASC + UnitAttributeSet
    AGP_MainBase  (Blueprint child)
    AGP_Barracks  (Blueprint child)
    AGP_ResourceNode (Blueprint child)
```

Building — це **stationary unit** з tag `GP.Unit.Type.Building`. Це дозволяє reuse Health / damage flow.

## AGP_BuildingBase (Pre-Pivot)

### Composition

- Inherits `AGP_UnitBase`.
- `USceneComponent` root, `UStaticMeshComponent` visual.
- `UGP_ProductionComponent` (optional, для buildings, що produce units).
- `UGP_ConstructionComponent` (присутній у "construction site" stage; видаляється після complete або implemented через state flag).
- `UGP_StorageComponent` (optional, для resource gathering anchors; у MVP не використовується активно, тому що Resource — PlayerState attribute).
- `UGP_RepairComponent` (optional, post-MVP).
- `UGP_CaptureComponent` (optional, post-MVP).

### Replicated Properties

Inherits from `AGP_UnitBase`. Plus:

- `UPROPERTY(ReplicatedUsing=OnRep_BuildingDefinition) TSoftObjectPtr<UGP_BuildingDefinition> BuildingDefinitionAsset;`
- `UPROPERTY(ReplicatedUsing=OnRep_ConstructionState) EGP_ConstructionState ConstructionState = EGP_ConstructionState::Completed;`

`EGP_ConstructionState`: `Pending`, `InProgress`, `Completed`, `Destroyed`.

## Construction Flow (Pre-Pivot)

```
Player issues GP.Command.Build with payload (UnitDef = BuildingDef, Location)
   |
   v
Worker's Build ability activates on server
   - Validate location buildable (NavMesh? terrain rules?  Y/N flag in BuildingDefinition?)
   - Validate Resource >= Cost
   - Apply GE_GP_Cost_Resource_<Building> (deduct resource)
   |
   v
Server spawns AGP_BuildingBase with ConstructionState = Pending
   - UGP_ConstructionComponent added/active
   - Visual = construction-site mesh (или mesh з 10% opacity, etc.)
   |
   v
UGP_ConstructionComponent ticks server-side:
   - Elapsed += DeltaTime * BuildSpeedModifier (read from PlayerState ASC)
   - Replicate progress (RepNotify ConstructionState or separate Progress float)
   |
   v
When Elapsed >= BuildTime:
   - ConstructionState = Completed
   - Apply EffectsOnComplete to owning PlayerState (e.g., +5 MaxUnits for Barracks)
   - Grant GrantedAbilities to building ASC
   - Switch visual to completed mesh
   - Building becomes target-selectable and damage-able
```

## Production Flow (Pre-Pivot)

```
Player selects Building (e.g., Barracks)
   |
   v
UI shows AllowedProductions list (from DA_GP_Building_Barracks)
   |
   v
Player clicks "Produce Trooper"
   |
   v
Server_RequestCommand with Command = GP.Ability.ProduceUnit, payload UnitDef = Trooper
   |
   v
Building's ASC activates UGP_GameplayAbility_ProduceUnit
   - Validate Resource >= Trooper.Cost
   - Validate CurrentUnits + 1 <= MaxUnits
   - Apply GE_GP_Cost_Resource_Trooper
   - Enqueue в UGP_ProductionComponent
   |
   v
UGP_ProductionComponent ticks server-side:
   - When timer elapses, spawn unit at rally point (or near building if no rally)
   - Increment PlayerState ASC CurrentUnits
   - Replicate queue state for UI
```

### UGP_ProductionComponent (Pre-Pivot)

```cpp
USTRUCT(BlueprintType)
struct FGP_ProductionEntry
{
    GENERATED_BODY()

    UPROPERTY()
    TSoftObjectPtr<UGP_UnitDefinition> UnitDef;

    UPROPERTY()
    float TimeRemaining = 0.f;
};

UCLASS()
class GPRUNTIME_API UGP_ProductionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(Replicated)
    TArray<FGP_ProductionEntry> Queue;

    UPROPERTY(EditAnywhere, Category = "GP|Production")
    FVector RallyPointOffset = FVector::ZeroVector;     // local-space, server uses, BP-overridable

    void EnqueueProduction(UGP_UnitDefinition* UnitDef);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
```

Queue replicated as array — для UI display.

## Detailed Main Base Rules (GP-0301, Pre-Pivot)

Stage — design only. Базується на існуючому `AGP_BuildingBase` + `UGP_ProductionComponent` + `UGP_StorageComponent`. Main Base — economic anchor, не auto-lose object.

### Identity

- Class: `AGP_MainBase : AGP_BuildingBase` (BP child of native abstract).
- DataAsset: `DA_GP_Building_MainBase` (per GDD/05).
- Tags: `GP.Unit.Type.Building`, `GP.Building.Role.Command`, `GP.Building.Type.MainBase`, `GP.Faction.Corporate`.
- Capability tags (per GP-0202): `{Selectable, Inspectable, Selection.Type.Building}`.
- AllowedCommands: `{Command_Stop, Command_SetRallyPoint, Command_QueueProduction, Command_CancelProduction}`.
  - **No** `Command_Move`, `Command_Attack`, `Command_AttackMove` — static + non-combat.

### DataAsset Schema (DA_GP_Building_MainBase)

`UGP_BuildingDefinition`-derived fields:

| Field | Type | Value / Notes |
| --- | --- | --- |
| `DisplayName` | `FText` | "Main Base" |
| `BuildingTags` | `FGameplayTagContainer` | per Identity above |
| `MaxHealth` | `float` | `1000` (TBD balance) |
| `Armor` | `float` | `50` |
| `Cost` | `int32` | `0` (not buildable у MVP — starting building) |
| `BuildTime` | `float` | `0` |
| `UnitCapContribution` | `int32` | `0` (Main Base не дає cap; Assembly Yard дає +5) |
| `bProvidesDropOff` | `bool` | `true` |
| `DropOffRange` | `float` | `400` cm (per GDD/05) |
| `bCanProduce` | `bool` | `true` |
| `AllowedProductions` | `TArray<TSoftObjectPtr<UGP_UnitDefinition>>` | `[DA_GP_Unit_Worker]` (у MVP only Worker) |
| `MaxProductionQueue` | `int32` | `5` |
| `bSupportsRallyPoint` | `bool` | `true` |
| `DefaultRallyOffset` | `FVector` | `(0, 500, 0)` — relative to building forward |
| `bSelfRegen` | `bool` | `false` (per decision — only Worker Repair Ability) |
| `RepairAbility` | `TSoftClassPtr<UGameplayAbility>` | `UGP_GA_Repair` (на Worker side; async-loaded) |
| `Mesh` | `TSoftObjectPtr<UStaticMesh>` | per asset pass |
| `Icon` | `TSoftObjectPtr<UTexture2D>` | per asset pass |
| `Materials` | `TArray<TSoftObjectPtr<UMaterialInterface>>` | per asset pass |

### Production — FIFO Queue 5

`UGP_ProductionComponent` server-side state:

```cpp
USTRUCT()
struct FGP_ProductionOrder
{
    GENERATED_BODY()
    UPROPERTY()  TSoftObjectPtr<UGP_UnitDefinition> UnitDef;
    UPROPERTY()  float StartTime = 0.f;          // server time of start (тільки для head slot)
    UPROPERTY()  float Duration  = 0.f;          // cached at start
};

UPROPERTY(ReplicatedUsing=OnRep_Queue)
TArray<FGP_ProductionOrder> Queue;   // head [0] in progress, [1..4] pending
```

Rules:

1. **Add to queue** (`Server_QueueProduction(UnitDef)`):
   - Validate: `Queue.Num() < MaxProductionQueue (=5)`.
   - Validate: `Building->IsAlive()` (no production from dead base).
   - Validate: `UnitDef ∈ AllowedProductions`.
   - Validate: player `Ferronite >= UnitDef->Cost` AND `CurrentUnits + 1 <= MaxUnits`.
   - Apply `GE_GP_SpendFerronite(Cost)` immediately (committed spend; cancel refunds).
   - Append `FGP_ProductionOrder` to `Queue`. If `Queue.Num() == 1` (just-added is head), set `StartTime=Now`, `Duration=UnitDef->ProductionTime`.
2. **Tick** (server, 0.25 s):
   - If `Queue.Num() > 0` AND `Now - Queue[0].StartTime >= Queue[0].Duration`:
     - `SpawnUnitFromOrder(Queue[0])` → location = building spawn socket; rotation toward rally direction.
     - `Queue.RemoveAt(0)`.
     - If `Queue.Num() > 0`: `Queue[0].StartTime = Now`; `Duration = Queue[0].UnitDef->ProductionTime`.
     - Fire `OnProductionCompleted` delegate (HUD listens).
3. **Cancel last** (`Server_CancelProduction(int32 SlotIndex)`):
   - Slot `0` (in-progress) cancellable: refund `Cost * RefundRate` (default `1.0` = full refund). Reset `Queue[0]`.
   - Slot `1..4`: pop, refund full.
   - Cancel будь-якого slot allowed per decision (10-slot deeper variant deferred).
4. **Pause on death:** `Building->IsAlive() == false` → production tick skip; queue frozen. На death — `AbortAllOrders()` refund all.
5. **Replication:** `Queue` replicated. Client UI binds `OnRep_Queue` для render.

### Rally Point

State:

```cpp
UPROPERTY(Replicated)
FVector RallyPoint = FVector::ZeroVector;   // world-space

UPROPERTY(Replicated)
TWeakObjectPtr<AActor> RallyActor;          // optional: rally to resource/building
```

Rules:

- On `BeginPlay` (server) — `RallyPoint = GetActorLocation() + GetActorForwardVector() * DefaultRallyOffset.Y` (configurable via DataAsset).
- Set via `Server_SetRallyPoint(FVector Point, AActor* Actor)`:
  - `Actor != null` → store `RallyActor`, `RallyPoint = Actor->GetActorLocation()`.
  - `Actor == null` → store `RallyPoint`, clear `RallyActor`.
- Input wiring (per GP-0202/0203): selected base + RMB:
  - On ground → `RallyPoint`, no `RallyActor`.
  - On Ferronite deposit → `RallyActor = deposit`, smart-command "mine".
  - On own building → `RallyActor = building`, smart-command "guard / move to" (passive у MVP).
  - On enemy → out of scope (Main Base не атакує).
- New tag: `GP.Command.SetRallyPoint`.

**Spawned unit behavior:** after spawn, server applies command to new unit per rally:

```
if (RallyActor.IsValid())
{
    if (RallyActor->HasTag(GP.Resource.Type.Ferronite))
        NewUnit->ReceiveCommand(Mine, RallyActor);
    else
        NewUnit->ReceiveCommand(Move, RallyActor->GetActorLocation());
}
else
{
    NewUnit->ReceiveCommand(Move, RallyPoint);
}
```

### Drop-Off Flow (Pre-Pivot)

- Worker з повним cargo рухається до nearest `MainBase` із `bProvidesDropOff=true`.
- Within `DropOffRange (=400cm)` and within sphere overlap — `UGP_MiningComponent::TryDropOff()`:
  - `MainBase->StorageComponent->ReceiveDeposit(Amount, ResourceType)`.
  - StorageComponent applies `GE_GP_AddFerronite(+Amount)` AND `GE_GP_SendToOrbit(+Amount * Resource.ScoreConversionRate)` на `OwnerPlayerState.ASC`.
  - Worker resumes mining cycle.
- Drop-off blocked if `MainBase->IsAlive() == false`.

### Destruction Flow (Pre-Pivot — NOT auto-lose)

```
GE_GP_Damage_Basic on MainBase
  → Health <= 0
  → AGP_BuildingBase::OnHealthZero (existing)
    1. Set tag GP.Unit.State.Dead.
    2. Disable input listeners (no production / no drop-off).
    3. ProductionComponent->AbortAllOrders (refund all queued slots).
    4. Multicast destruction VFX + sound.
    5. Delay 5 s → Destroy() (allow rubble pass).
  → AGP_GameMode::OnUnitDied(MainBase, KillerPlayerState)
    - Building has tag GP.Building.Type.MainBase → NOT EndMatch.
    - Log informational analytics event (StateEvent: MainBaseDestroyed).
  → Player A loses ability to produce Workers, receive Ferronite drop-off, refill Ferronite.
  → Match continues per score until 10-min timer expires.
```

**Note:** the post-pivot model changes this to an annihilation-path victory (MainBase destroyed → opponent wins) because without containers + launch the player has no way to ship resource. See active `TDD/06`.

## Detailed Assembly Yard Rules (GP-0304, Pre-Pivot)

Stage — design only. Building name: **Assembly Yard** (renamed from "Barracks"). Поверх `AGP_BuildingBase` + `UGP_ProductionComponent` + `UGP_ConstructionComponent`.

### Identity

- Class: `AGP_AssemblyYard : AGP_BuildingBase` (BP child of native abstract OR pure DA-driven instance).
- DataAsset: `DA_GP_Building_AssemblyYard`.
- Tags: `GP.Unit.Type.Building`, `GP.Building.Role.Production`, `GP.Building.Type.AssemblyYard`, `GP.Faction.Corporate`.
- Capability tags: `{Selectable, Inspectable, Selection.Type.Building}`.
- AllowedCommands: `{Command_Stop, Command_SetRallyPoint, Command_QueueProduction, Command_CancelProduction}`.

### DataAsset Schema (DA_GP_Building_AssemblyYard)

All numerics are placeholders pending balance pass.

| Field | Type | Placeholder | Notes |
| --- | --- | --- | --- |
| `DisplayName` | `FText` | "Assembly Yard" | |
| `BuildingTags` | `FGameplayTagContainer` | per Identity | |
| `Cost` | `int32` | per balance | Spend на construction commit. |
| `BuildTime` | `float` | per balance | Base time at 1 worker. |
| `MaxHealth` | `float` | per balance | |
| `Armor` | `float` | per balance | |
| `UnitCapContribution` | `int32` | `+5` | Applied via `GE_GP_UnitCap_Plus5`. |
| `bProvidesDropOff` | `bool` | `false` | No drop-off у Assembly Yard. |
| `bCanProduce` | `bool` | `true` | |
| `AllowedProductions` | `TArray<TSoftObjectPtr<UGP_UnitDefinition>>` | `[DA_GP_Unit_SalvageWalker]` | Single combat unit у MVP. |
| `MaxProductionQueue` | `int32` | per balance | Same FIFO model as MainBase. |
| `bSupportsRallyPoint` | `bool` | `true` | Default rally — front-of-building. |
| `DefaultRallyOffset` | `FVector` | `(0, 600, 0)` | Configurable, balance lever. |
| `bSelfRegen` | `bool` | `false` | Worker repair only. |
| `RepairAbility` | `TSoftClassPtr<UGameplayAbility>` | `UGP_GA_Repair` | Shared з MainBase. Async-loaded. |
| `ConstructionApplyEffects` | `TArray<TSoftClassPtr<UGameplayEffect>>` | `[GE_GP_UnitCap_Plus5]` | Applied на complete construction. Async-loaded. |
| `bMultiWorkerBuild` | `bool` | `true` | Decision: multi-worker speedup enabled. |
| `MaxBuilders` | `int32` | per balance | Cap на одночасних builders. |
| `CancelRefundRate` | `float` | `1.0` | 100% refund per decision (cancel any time). |
| `Mesh` / `Materials` / `Icon` | art refs | per asset pass | |

### Construction Flow (Multi-Worker Speedup)

Поверх `UGP_ConstructionComponent`. Decisions:

```cpp
USTRUCT()
struct FGP_ConstructionState
{
    UPROPERTY(Replicated) float Progress01 = 0.f;          // [0..1]
    UPROPERTY(Replicated) int32 ActiveBuilders = 0;
    UPROPERTY()           TArray<TWeakObjectPtr<AGP_Worker>> AssignedWorkers;
};
```

Server tick (0.25 s):

1. `ActiveBuilders = AssignedWorkers.Num()`, clamp to `Definition->MaxBuilders`.
2. `Speed = ActiveBuilders` (linear N× per decision).
3. `Progress01 += Speed * DeltaTime / Definition->BuildTime`.
4. `Progress01 >= 1.0` → `OnConstructionComplete`:
   - Spawn final `AGP_AssemblyYard` actor at site location/rotation.
   - Apply `GE_GP_UnitCap_Plus5` (Infinite duration, source: this building) на `OwnerPlayerState.ASC`.
   - Workers released → return to previous mining (per GP-0302 auto-cycle).
   - Construction site actor destroyed.

Builder assignment:

- Worker з `GP.Ability.Build` issued на site → request `ConstructionComponent->AssignBuilder(Worker)`:
  - If `AssignedWorkers.Num() < MaxBuilders` → assign, worker enters channeling state.
  - Else → reject (`Client_NotifyCommandRejected(EReason::BuildSlotFull)`), worker idle.
- Worker death / interrupt → `ConstructionComponent->ReleaseBuilder(Worker)`. `Progress01` зберігається; speed зменшується наступний tick.
- All builders left → `Progress01` frozen. Recommendation у HUD: "Stalled" overlay.

Multi-worker decision rationale: linear N× speedup до cap `MaxBuilders`. Diminishing-returns model відкинуто у пользу простоти.

### Cancel Construction — 100% Refund

Decision: cancel будь-який час, full refund.

Trigger: player selects construction site, "Cancel" button у `WBP_GP_HUD_SelectionPanel` building mode (single-select), OR `Esc` while site selected.

Flow:

1. `Server_CancelConstruction(SiteActor)`:
   - Validate ownership.
   - Apply `GE_GP_RefundFerronite(Definition->Cost * CancelRefundRate)` (=1.0).
   - Release all builders (they auto-resume previous activity).
   - Destroy site actor + spawn cancel VFX (multicast unreliable).

**Abuse vector flagged:** 100% refund + low BuildTime → spam place-cancel cycles to scout. Mitigations (deferred): minimum-commit time, per-PC cancel debounce, place-while-fog protection.

### Build Validity — Placement Rules (Pre-Pivot)

Server-side validation on `Server_BuildAt(BuildingDef, Location, Rotation)`:

1. `BuildingDef ∈ Player.AllowedBuildings` (from Faction DA).
2. `Player.Ferronite >= BuildingDef.Cost`.
3. NavMesh navigable at `Location`.
4. No overlap із existing buildings / units / impassable terrain (sphere check `Definition->FootprintRadius`).
5. Within map bounds.
6. NOT inside enemy territory (reserved hook).
7. Not too close to enemy MainBase (reserved hook).

Invalid → reject, no charge, `Client_NotifyCommandRejected(EReason::BuildInvalid)`.

### Unit Cap Effect

`GE_GP_UnitCap_Plus5`:

- Duration: `Infinite`.
- Modifier: `MaxUnits += UnitCapContribution` (DA-driven, default +5 for Assembly Yard).
- Source object: this Assembly Yard instance.
- Removed on building destruction via `RemoveActiveGameplayEffectBySourceEffect`.

> Post-pivot: this +N MaxUnits contribution now comes from the orbital-dropped Logistics Hub, not a locally-constructed Assembly Yard. See active `TDD/06`.

### Smart-Command Resolution (Assembly Yard subset)

| Hit target | Assembly Yard selected → command |
| --- | --- |
| Ground | `SetRallyPoint(ground)` |
| Own undamaged building | `SetRallyPoint(building)` (rally near building) |
| Own Ferronite deposit | N/A — Assembly Yard does not interact |
| Enemy | No command. Tooltip "Assembly Yard cannot attack". |

### Out of MVP (Assembly Yard-specific, Pre-Pivot)

- Tech tree / upgrades.
- Multiple unit types per yard.
- Researchable production buffs.
- Yard-specific rally formations.
