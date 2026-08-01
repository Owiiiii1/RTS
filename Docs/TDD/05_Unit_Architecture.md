# Unit Architecture

## Class Hierarchy

```
AActor
  AGP_UnitBase                  // abstract; owns ASC, UnitAttributeSet, tags, TeamId, UnitDefinition
    AGP_MobileUnit              // moving units (workers, troopers)
    AGP_BuildingBase            // static units (buildings, resource nodes)  -- see 06_Building_Architecture
```

`AGP_UnitBase` — common ancestor. Building теж є "unit" у тег-сенсі (`GP.Unit.Type.Building`). Це дозволяє uniform GAS attribute handling (Health works the same way для unit і building).

## AGP_UnitBase

### Composition

- `UAbilitySystemComponent* AbilitySystemComponent` — unit-scoped ASC. Replication mode: `Minimal`.
- `UGP_UnitAttributeSet* UnitAttributeSet` — owned by ASC.
- `UStaticMeshComponent` / `USkeletalMeshComponent` — visual; BP-overrideable.
- `USphereComponent` — root collision/selection bound (sized by UnitDefinition or default).

### Replicated Properties

- `UPROPERTY(ReplicatedUsing=OnRep_UnitDefinition) TSoftObjectPtr<UGP_UnitDefinition> UnitDefinitionAsset;`
- `UPROPERTY(Replicated) int32 TeamId = 0;`
- `UPROPERTY(Replicated) FGameplayTagContainer UnitTags;` (cached snapshot from UnitDefinition + runtime mods)

### Lifecycle

```
Server: AGP_GameMode::SpawnUnit(UnitDef, Location, TeamId)
   |
   v
SpawnActor<AGP_UnitBase>
   |
   v
AGP_UnitBase::BeginPlay
   - Initialize ASC ActorInfo
   - Apply UnitDefinition (read attributes, grant abilities)
   - Replicate UnitDefinitionAsset (RepNotify on client triggers visual setup)
   |
   v
Client: OnRep_UnitDefinition
   - Setup mesh, materials, team color from UnitDefinition
```

### Authority Helpers

```cpp
FORCEINLINE bool AGP_UnitBase::IsAuthority() const { return HasAuthority(); }
FORCEINLINE int32 AGP_UnitBase::GetTeamId() const { return TeamId; }
```

## UGP_UnitDefinition (Data Asset)

```cpp
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_UnitDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    TSoftObjectPtr<UStaticMesh> Mesh;            // або SkeletalMesh — фіналізується per-unit

    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    FGameplayTagContainer UnitTags;

    UPROPERTY(EditAnywhere, Category = "GP|Economy")
    int32 Cost = 0;                              // OrbitalFerronite cost of the drop order

    UPROPERTY(EditAnywhere, Category = "GP|Economy")
    float DeliveryTime = 0.f;                    // orbital drop descent/telegraph time (no local production)

    UPROPERTY(EditAnywhere, Category = "GP|Combat")
    float Damage = 0.f;

    UPROPERTY(EditAnywhere, Category = "GP|Combat")
    float AttackRange = 0.f;

    UPROPERTY(EditAnywhere, Category = "GP|Combat")
    float AttackSpeed = 0.f;

    UPROPERTY(EditAnywhere, Category = "GP|Movement")
    float MoveSpeed = 0.f;

    UPROPERTY(EditAnywhere, Category = "GP|Vitals")
    float MaxHealth = 100.f;

    UPROPERTY(EditAnywhere, Category = "GP|Vitals")
    float HealthRegenRate = 0.f;

    UPROPERTY(EditAnywhere, Category = "GP|Vitals")
    float RepairRate = 0.f;

    UPROPERTY(EditAnywhere, Category = "GP|Commands")
    FGameplayTagContainer AllowedCommands;

    UPROPERTY(EditAnywhere, Category = "GP|Abilities")
    TArray<TSoftClassPtr<UGameplayAbility>> GrantedAbilities;     // async-loaded, resolved at spawn

    UPROPERTY(EditAnywhere, Category = "GP|Animations")
    TSoftObjectPtr<UAnimMontage> DeathAnimation;

    UPROPERTY(EditAnywhere, Category = "GP|Components")
    TSoftClassPtr<AGP_UnitBase> SpawnedClass;     // async-loaded by AssetManager; resolved at spawn time
};
```

`UPrimaryDataAsset` — для asset manager registration.

`SpawnedClass` дозволяє через одне Data Asset spawn'ити правильний AActor subclass. Альтернатива — окрема production logic, що map'ить UnitTag → Class.

## AGP_MobileUnit

### Composition (Default)

- Inherits AGP_UnitBase.
- `UCharacterMovementComponent` (через `ACharacter` base) — або custom `UGP_MovementComponent` поверх `UPawnMovementComponent`. **Decision:** `AGP_MobileUnit : public APawn` з custom `UGP_MovementComponent` для RTS-style movement (no jump, no Character humanoid coupling). Це простіше масштабувати на не-humanoid units.
- `UGP_CommandReceiverComponent` (optional — або просто overridden `ReceiveCommand`).
- For Worker: `UGP_MiningComponent`.
- For Combat: `UGP_CombatComponent`, `UGP_TargetingComponent`.

Композиція задається у BP child (`BP_GP_Worker` додає MiningComponent; `BP_GP_Trooper` додає CombatComponent + TargetingComponent).

Альтернатива: компоненти добавляються через `UGP_UnitDefinition.ComponentsToAdd : TArray<TSoftClassPtr<UActorComponent>>` (data-driven composition, async). Цей варіант **deferred** — для MVP BP-driven composition достатньо. Документувати у Backlog.

## Components

### UGP_MovementComponent

- Pawn movement component.
- `MoveTo(FVector Destination)` — RPC-free, server-only call.
- Pathfinding — UE NavMesh (default RecastNavMesh).
- Client interpolation through movement component replication (standard).

### UGP_MiningComponent

- Server-only tick.
- `BeginMining(AActor* Node)` → stores target, starts tick.
- On tick:
  - If in range of the deposit: extract raw Ferronite into the Worker's `UGP_CargoComponent` (planetary, not spendable). When cargo is full, route to the nearest MainBase and drop off into its container `UGP_StorageComponent`.
  - Else: route to MovementComponent::MoveTo(Node->GetActorLocation()).
- No player income GE is applied by mining or drop-off. Spendable `OrbitalFerronite` (+ `FerroniteScore`) is granted only when a full container is launched to orbit (`GE_GP_AddOrbital` + `GE_GP_AddScore` on MainBase). Full cargo→container flow in §Mining Loop below and TDD/06 §Drop-Off Flow.

### UGP_CombatComponent

- Server-only tick.
- `EngageTarget(AActor* Target)`.
- On tick:
  - If target dead → clear.
  - Else if in range:
    - If `Cooldown_Attack` tag absent: apply `GE_GP_Damage_Basic` to target.
    - Trigger `GE_GP_Cooldown_Attack` on self.
    - Multicast cosmetic (attack animation, VFX) — `Multicast_PlayAttackVFX()` (Unreliable).
  - Else: route to MovementComponent::MoveTo(Target->GetActorLocation()).

### UGP_TargetingComponent

- Auto-targeting logic для combat units (auto-acquire visible enemies у range).
- Out-of-scope для MVP першого playable target; **basic auto-target** включити, якщо MVP playtest показує необхідність.

## Death Flow

```
Server: UGP_UnitAttributeSet::PostGameplayEffectExecute
   - If Health <= 0:
     - Apply tag GP.Unit.State.Dead (loose tag, durable until destroy).
     - Stop all components (Movement, Combat, Mining).
     - Schedule Destroy with delay (e.g., 3s for death animation).
     - Multicast_PlayDeathVFX.
     - AGP_GameMode::OnUnitDied -> decrement CurrentUnits на PlayerState.
   |
   v
Client: OnRep on Health attribute -> presentation
        Multicast_PlayDeathVFX -> spawn VFX, animation
```

## Replication Hot-Path

Movement — `UCharacterMovementComponent` / `UPawnMovementComponent` standard replication. Не custom.

Health — through GAS attribute replication.

Position correction — engine standard.

## Spawning Discipline

- Server-only spawn. Жодного `SpawnActor` на client side.
- `AGP_GameMode::SpawnUnit(UGP_UnitDefinition* Def, FVector Loc, int32 TeamId, AGP_PlayerState* Owner)` — єдиний entry point.
- Spawn class — `Def->SpawnedClass`.
- ASC ActorInfo ініціалізується після spawn, перед attribute apply.

## Out of MVP

- Unit veterancy / experience.
- Carrying capacity / inventory on units (Worker cargo is engineered as gameplay scalar, not generic inventory).
- Upgrades.
- Unit-specific abilities beyond basic Attack / Repair (no Build — orbital pivot).
- AI behavior trees (basic component ticks достатньо у MVP).

## Detailed Worker Unit Rules (GP-0302)

Stage — design only (per [`Claude_Tasks/GP-0302_Worker_Unit`](../Development/Claude_Tasks/GP-0302_Worker_Unit.md)). Поверх `AGP_MobileUnit` + components.

### Role

Worker — економічний primary unit:

- Видобуває raw Ferronite з `AGP_FerroniteDeposit` (resource node — building per ADR-0007 + TDD/07).
- Carries raw Ferronite to MainBase containers via the mining/drop-off cycle (planetary Ferronite, not spendable until launched to orbit).
- Виконує repair (`GP.Ability.Repair` per GP-0301) — repair STAYS in MVP.
- **Does NOT build and does NOT produce.** Per the Orbital Delivery pivot all units/buildings arrive via orbital drop pods; there is no Worker Build ability and no local construction.
- **Не атакує.** Per GDD/04 і GP-0204 — `bAutoAttacks=false`, `AllowedCommands` без `Command_Attack/AttackMove`.
- Soft target для SWARM waves (low HP = strategic vulnerability).

### Class

- `AGP_Worker : AGP_MobileUnit` (BP child `BP_GP_Worker` per TDD/05).
- Native composition (`AGP_Worker` ctor):
  - `UGP_MovementComponent` (inherited from `AGP_MobileUnit`).
  - `UGP_MiningComponent` (worker-specific).
  - `UGP_CargoComponent` (NEW — see Cargo section).
  - `UAbilitySystemComponent` (inherited; `Minimal` replication).
- Granted abilities (per UnitDefinition): `UGP_GA_Repair` only. **No Build ability** (removed by the orbital pivot).

### DataAsset (DA_GP_Unit_Worker)

Per GDD/04 + GP-0301 amendments:

| Field | Type | Value | Notes |
| --- | --- | --- | --- |
| `DisplayName` | `FText` | "Worker" | |
| `UnitTags` | `FGameplayTagContainer` | `{Unit.Type.Worker, Faction.Corporate}` | |
| `CapabilityTags` | `FGameplayTagContainer` | `{Capability.Selectable, Capability.Inspectable, Selection.Type.Unit}` | Per GP-0202. |
| `Cost` | `int32` | TBD | OrbitalFerronite cost of the drop order. |
| `DeliveryTime` | `float` | TBD | Orbital drop descent/telegraph time (no local production). |
| `MaxHealth` | `float` | `50` | |
| `Armor` | `float` | `0` | |
| `MoveSpeed` | `float` | `350` cm/s | |
| `CarryCapacity` | `int32` | `50` | Ferronite per trip. |
| `MineRatePerSecond` | `float` | `10` | Override of `DA_GP_Resource_Ferronite.MineRatePerWorker` (worker-level wins per GDD/04 note — confirm у balance). |
| `MiningRange` | `float` | `200` cm | Distance from deposit center. |
| `AllowedCommands` | `FGameplayTagContainer` | `{Command_Move, Command_Stop, Command_Mine, Command_Repair}` | **No Attack tags. No Build** (orbital pivot). |
| `GrantedAbilities` | `TArray<TSoftClassPtr<UGameplayAbility>>` | `[UGP_GA_Repair]` | Async-loaded by AssetManager at spawn. No Build ability. |
| `bAutoAttacks` | `bool` | `false` | Per GP-0204 — no auto-acquire. |
| `AutoAcquireRangeMultiplier` | `float` | `0` | N/A but set explicitly. |
| `bAutoCycle` | `bool` | `true` | NEW — controls auto-return + auto-retarget (див. нижче). |
| `AutoRetargetSearchRadius` | `float` | `3000` cm | Search range for next deposit when current depletes. |

### Cargo Component

`UGP_CargoComponent` — local server state, mirror minimal to client для UI bar.

```cpp
UCLASS()
class GPRUNTIME_API UGP_CargoComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated)
    int32 CurrentCargo = 0;

    UPROPERTY(EditDefaultsOnly)
    int32 MaxCargo = 50;       // mirrored from UnitDefinition->CarryCapacity at spawn

    UPROPERTY(Replicated)
    FGameplayTag CargoType = FGameplayTag::EmptyTag;   // GP.Resource.Type.Ferronite

    bool IsFull() const { return CurrentCargo >= MaxCargo; }
    int32 AddCargo(int32 Delta, FGameplayTag Type);    // returns leftover (capacity overflow)
    int32 EmptyAll();                                  // returns amount removed
};
```

- Replication: `CurrentCargo` + `CargoType` — `Skip Owner` not needed, full replicate (small).
- Client UI: `WBP_GP_HUD_SelectionPanel` reads через PlayerController->LocalSelection->FirstSelected->Cargo when single Worker selected. Render bar fill = `CurrentCargo / MaxCargo`.

### Mining Loop (server-side)

`UGP_MiningComponent` extended per GDD/06 + decisions:

```
State enum: { Idle, MovingToDeposit, Mining, MovingToBase, DroppingOff, Returning }

On ReceiveCommand(Mine, DepositActor):
  - Set TargetDeposit = DepositActor (TWeakObjectPtr).
  - State = MovingToDeposit.
  - MovementComponent->MoveTo(TargetDeposit.Location, MiningRange).

On Tick (server, 0.25 s):
  switch State:
    MovingToDeposit:
      if InRange(TargetDeposit, MiningRange):
        if !TargetDeposit.IsValid() or TargetDeposit.IsDepleted:
          [AutoRetarget path]
        else:
          State = Mining; StartMineTime = Now.
      else: MovementComponent ticks.

    Mining:
      if Cargo.IsFull():
        State = MovingToBase; NearestBase = FindBaseWithDropOff();
      elif !TargetDeposit.IsValid() or TargetDeposit.IsDepleted:
        [AutoRetarget path]
      elif Now - StartMineTime >= 1.0 / MineRatePerSecond * Chunk:
        ChunkAmount = min(Chunk, MaxCargo - Cargo.CurrentCargo, TargetDeposit.RemainingReserves)
        Cargo.AddCargo(ChunkAmount, GP.Resource.Type.Ferronite)
        TargetDeposit.Mine(ChunkAmount)
        StartMineTime = Now
        Play mining VFX (multicast unreliable, 1Hz)

    MovingToBase:
      if InRange(NearestBase, NearestBase.DropOffRange):
        State = DroppingOff
      else: MovementComponent ticks. If NearestBase.IsAlive==false → re-search.

    DroppingOff:
      Amount = Cargo.EmptyAll()
      NearestBase.StorageComponent.ReceiveDeposit(Amount, Ferronite)
      State = Returning

    Returning:
      if TargetDeposit.IsValid() and !TargetDeposit.IsDepleted:
        State = MovingToDeposit
      else if bAutoCycle:
        [AutoRetarget path]
      else:
        State = Idle

  [AutoRetarget path]:
    if bAutoCycle:
      NextDeposit = FindClosestLiveDeposit(WorkerLoc, AutoRetargetSearchRadius)
      if NextDeposit:
        TargetDeposit = NextDeposit; State = MovingToDeposit
      else:
        State = Idle; broadcast OnIdleWithNoDeposit (HUD alert).
    else:
      State = Idle
```

Notes:

- `Chunk` rate — кожен mining "tick" забирає `Round(MineRatePerSecond * dt)` per 0.25 s tick (≈ 2.5 units per tick). Visual feels like steady stream.
- `FindBaseWithDropOff()` — preferred nearest `AGP_MainBase` із `IsAlive()` AND `bProvidesDropOff`. If none alive: idle with HUD alert.
- `OnIdleWithNoDeposit` delegate — HUD shows "Idle Worker" indicator (per GP-0401).

### Repair Ability (GP.Command.Repair — STAYS in MVP)

`UGP_GA_Repair` (granted to Worker; the Worker's only granted ability beyond movement/mining). Worker does **not** own a Build ability — there is no local construction in the Orbital Delivery model.

- Tag: `GP.Ability.Repair`. Input: `GP.Command.Repair` (RMB on own damaged building/unit when Worker(s) selected).
- Activation: server-only (GAS authority). Server validates: target is own-team, damaged (Health < MaxHealth), and a valid repair target; Worker is in-range (in-range channel).
- Channel: continuous tick while in range AND target damaged. Worker stops the mining loop and stays at the target while repairing (cargo need not be empty).
- Cost: `GE_GP_Cost_RepairTick` per tick → `OrbitalFerronite -= TickCost` (TickCost = TBD placeholder).
- Heal: `GE_GP_Heal(RepairAmountPerTick)` (heal-per-tick = TBD placeholder).
- Cancel conditions: target full HP, target destroyed, Worker out of range, insufficient OrbitalFerronite, new command issued, Worker death.
- Smart-command: own damaged building/unit → `Repair` (if Worker selected). See resolution table below.

### Smart-Command Resolution (Worker subset)

Updates to `UGP_CommandComponent::BuildSmartCommand`:

| Hit target | Worker(s) selected → command |
| --- | --- |
| Ground (NavMesh) | `Move` |
| Own undamaged building | `Move` (to building loc) — guard against accidental repair |
| Own damaged building / unit | `Repair` |
| Ferronite deposit | `Mine` |
| Enemy unit / building | **No command** (worker can't attack). HUD shows "Worker cannot attack" tooltip on hover. |

### Tag Surface

| Tag | Purpose |
| --- | --- |
| `GP.Unit.Type.Worker` | Identity |
| `GP.Unit.State.Mining` | Loose tag set by MiningComponent during Mining state (UI / SFX) |
| `GP.Unit.State.Repairing` | Loose tag during repair channeling |
| `GP.Ability.Repair` | Granted ability |
| `GP.Command.Mine`, `GP.Command.Repair` | Allowed commands |

### Validation Checklist (Stop Condition)

- [x] Worker supports Move (Command_Move + MovementComponent) і Gather (Command_Mine + MiningComponent + CargoComponent).
- [x] Worker role readable у UI: `Selection.Type.Unit` routes to SelectionPanel unit mode; cargo bar; idle alert.
- [x] Worker mines raw Ferronite and carries it to MainBase containers (drop-off cycle); raw Ferronite is planetary, not spendable.
- [x] Worker repairs own damaged buildings/units via `GP.Command.Repair` (`GE_GP_Cost_RepairTick`).
- [x] Worker does NOT build and does NOT produce — no Build ability, no construction (orbital pivot).
- [x] No-attack rule enforced — `AllowedCommands` без Attack tags, `bAutoAttacks=false`.

### Open Questions

1. **Mining VFX rate:** 1 Hz multicast vs continuous emitter. Recommend 1 Hz event + persistent emitter local-only.
2. **Cargo conversion across resource types:** N/A у MVP (only Ferronite). `CargoType` field reserved для post-MVP multi-resource.
3. **Worker idle radius:** "idle" worker biases mid-base — auto-task assignment у post-MVP? Defer.
4. **Repair tick cost / heal rate:** placeholders (TBD) — balance pass.
5. **Two workers on same deposit:** stack at single tile або spread navigation? NavMesh avoidance handles natural spread.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Manual mining | RMB Worker on deposit → MovingToDeposit → Mining → MovingToBase → DroppingOff → Returning → MovingToDeposit. Cargo bar fills/empties. |
| 2 | Cargo full auto-return | At 50/50 cargo → auto MovingToBase regardless of player input. |
| 3 | Deposit deplete mid-mining | Deposit reserves 0 → auto FindClosestLiveDeposit within 3000 cm → continue. |
| 4 | No live deposit | Last deposit dies → Worker idles, HUD shows "Idle worker" alert. |
| 5 | Drop-off blocked | Main Base destroyed → Worker can't deposit; idles with cargo full. |
| 6 | Repair | Own damaged building, RMB → Worker repairs, OrbitalFerronite drains per tick, HP regenerates. |
| 7 | Repair out of orbital | OrbitalFerronite hits 0 mid-repair → repair cancels, Worker stops. |
| 8 | Smart-command priority | Own undamaged building → RMB resolves to Move (guard against accidental repair); damaged → Repair. |
| 9 | Worker vs enemy | Enemy unit visible, A+LMB → nothing (Attack not allowed). HUD shows tooltip on hover. |
| 10 | Multiple workers parallel | Marquee 5 workers, RMB deposit → all 5 fan out, mine, return to base, queue not single-file. |
| 11 | Death mid-mining | Worker killed by SWARM → cargo lost, mining state cleared, OnDeath fires for selection cleanup. |

### Out of MVP (Worker-specific)

- Worker repair without OrbitalFerronite (free repair).
- Worker stance (aggressive workers post-MVP).
- Cargo conversion / dropoff to non-base structures.
- Worker rest / morale.
- Specialization (mining vs repair perks).

## References

- ASC / AttributeSets — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- Commands → unit routing — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md).
- Unit data — [`../GDD/04_Units`](../GDD/04_Units.md).
- Resources — [`07_Resource_Architecture`](07_Resource_Architecture.md) and [`../GDD/06_Resources`](../GDD/06_Resources.md).
- Main Base hook — [`06_Building_Architecture`](06_Building_Architecture.md) §"Detailed Main Base Rules (GP-0301)".
- Worker task — [`../Development/Claude_Tasks/GP-0302_Worker_Unit`](../Development/Claude_Tasks/GP-0302_Worker_Unit.md).
