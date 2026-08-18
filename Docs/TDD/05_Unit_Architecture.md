# Unit Architecture

## Class Hierarchy

```
AActor
  APawn
    AGP_UnitBase                    // abstract common ancestor
      AGP_MobileUnit                // owns UGP_MovementComponent
        AGP_Unit                    // concrete generic unit (Capsule + UnitVisual)
          AGP_SalvageWalker         // canonical MVP combat unit (GDD/04)
        AGP_Worker                  // MobileUnit child; own presentation/resource composition
      AGP_BuildingBase              // static buildings (e.g. MainBase)
```

`AGP_UnitBase` — common ancestor. Buildings may carry `GP.Unit.Type.Building` capability tags for uniform selection/GAS Health handling.

**Not BuildingBase:** `AGP_ResourceNode` is a separate `AActor` (resource deposit), not under `AGP_BuildingBase`.

**Salvage Walker:** `AGP_SalvageWalker : AGP_Unit` is the native playable combat class. Operator creates `BP_SalvageWalker` manually. Do not treat `AGP_Unit` InfantryMelee cosmetic archetype as Salvage Walker.

## Player unit cap (GP-S33C)

GP-S33C is **implementation + operator FINAL PASS complete** on `feature/gp-s33c-unit-cap-logistics-hub`. **NOT MERGED** (not on `main` yet).

Player `MaxUnits` / `CurrentUnits` live on `UGP_PlayerAttributeSet` (OwnerOnly). Capacity is **not** DropPod Transport Slots.

- Base `MaxUnits = 5` via infinite native `UGP_GE_UnitCap_Base5`, applied once per PlayerState ASC (authority).
- `CurrentUnits` counts living player-controllable **Worker** and **Salvage Walker** only. Buildings, DropPods, ghosts, nodes, SWARM do not count.
- Register exactly once when the unit becomes live/owned; unregister exactly once on death (`HandleDeathInternal`) or EndPlay. Never negative.
- Owner is `RequestingPlayerState` for orbital payload, or team lookup on GameState `PlayerArray` for preplaced units. Unresolved owner: warning, do not increment the wrong player.
- Orbital orders reserve entity count on the PlayerState (`PendingOrbitalUnitCount`). Validate `Current + Pending + ManifestCount <= MaxUnits`. Failed/incomplete payload releases leftover reservation. Transport-slot cost is independent (Walker is 1 CurrentUnit, 2 slots).
- Each living deployed `AGP_LogisticsHub` applies infinite `UGP_GE_UnitCap_Plus5` with **SetByCaller** magnitude from `UGP_BuildingDefinition.UnitCapBonus` (Hub baseline +5). Empty BuildingDefinition keeps class fallback +5. Destroying a Hub removes that effect once. If Current > new Max, units stay alive; new orders reject `UnitCapReached` until attrition.

## AGP_UnitBase

### Composition (current)

- `UGP_UnitCommandComponent` — Held command + Attack executor/FSM (and other command orchestration).
- `UGP_CombatPresentationComponent` — cosmetic combat presentation channel.
- `UGP_TeamPresentationComponent` — TeamId → team color presentation.
- `UGP_HealthBarComponent` — world/screen health bar presentation (GAS Health/MaxHealth bind).
- `UGP_AbilitySystemComponent` — unit-scoped ASC (project Mixed replication mode).
- `UGP_UnitAttributeSet` — owned alongside ASC.
- `TeamId` (replicated) + `CapabilityTags` (class defaults / interim selection facts).
- Compatibility fallback combat defaults (`DefaultMaxHealth`, `DefaultHealth`, `DefaultDamage`, `DefaultArmor`, `DefaultDamageResistance`, `DefaultAttackCooldown`, `DefaultAttackRange`) used only when `UnitDefinitionAsset` is empty.

**Not current:** UnitBase does **not** create a Sphere root and does **not** own StaticMesh/SkeletalMesh presentation directly. Capsule roots and visual ownership live on concrete children (`AGP_Unit`, `AGP_Worker`, buildings).

### Replicated Properties (current core)

- `TeamId` (ReplicatedUsing OnRep).
- Death flag / related death presentation fields on UnitBase.
- CapabilityTags are EditDefaultsOnly class facts (not a live replicated UnitTags snapshot from UnitDefinition in current code).

> GP-S38D: soft `UnitDefinitionAsset` (`TSoftObjectPtr<UGP_UnitDefinition>`) is the designer-facing initial/base source. Resolver is already-loaded only (no `LoadSynchronous`). Empty ref keeps the `Default*` / component CDO fallback.

### Lifecycle (simplified current)

```
Server spawn concrete AGP_UnitBase child
   |
   v
BeginPlay
   - Init ASC ActorInfo
   - UnitDefinition: empty ref → immediate Default* fallback; already-loaded → apply now;
     valid unloaded soft ref → RequestAsyncLoad (no LoadSynchronous), then apply on callback
   - AutoAcquire stays off until definition init completes, then RefreshCombatAutoAcquireTimer
   - Initialize combat attributes from UnitDefinition, else Default* (authority)
   - Presentation components bind / attach as implemented per component
```

### Authority Helpers

```cpp
FORCEINLINE bool AGP_UnitBase::IsAuthority() const { return HasAuthority(); }
FORCEINLINE int32 AGP_UnitBase::GetTeamId() const { return TeamId; }
```

## UGP_UnitDefinition (Data Asset)

**GP-S38D ownership (canonical):** `UGP_UnitDefinition` is the intrinsic unit/building **gameplay stat** definition. It initializes GAS base values and command/movement tuning. It does **not** replace runtime GAS state.

Do **not** confuse with:

| Asset | Owns |
| --- | --- |
| `UGP_UnitDefinition` | Intrinsic vitals / combat / sight / facing / MoveSpeed / RetaliationPursuitSeconds **+ `CargoCapacity`** |
| `UGP_BuildingDefinition` | Building identity / icon / tags / `SpawnedClass` / footprint + storage + `UnitCapBonus` + soft UnitDefinition |
| `UGP_OrbitalUnitDropDefinition` | Unit purchase cost, transport slots, payload class, per-unit descent / deploy delay |
| `UGP_OrbitalDropDefinition` | Building purchase cost, DropTags, building delivery timing |

CapabilityTags stay on `AGP_UnitBase` (not migrated in S38D). Nav/repath/separation stay on `UGP_MovementComponent`.

```cpp
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_UnitDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Category = "GP|Vitals")
    float MaxHealth = 100.f;
    float InitialHealth = 100.f;
    float Armor = 0.f;
    float DamageResistance = 0.f;

    UPROPERTY(EditAnywhere, Category = "GP|Combat")
    float Damage = 0.f;
    float AttackRangeCm = 250.f;
    float AttackCooldownSeconds = 1.f;
    float SightRangeCm = 900.f;
    float AutoAcquireScanIntervalSeconds = 0.35f;
    float AttackFacingRotationSpeedDegreesPerSecond = 360.f;

    UPROPERTY(EditAnywhere, Category = "GP|Movement")
    float MoveSpeedCmPerSecond = 0.f;   // 0 = do not write movement (buildings)

    UPROPERTY(EditAnywhere, Category = "GP|Behavior|Retaliation")
    float RetaliationPursuitSeconds = 5.f; // DATA ONLY until GP-S40R; 0 = disabled

    UPROPERTY(EditAnywhere, Category = "GP|Logistics|Cargo")
    float CargoCapacity = 0.f; // Worker 50; 0 = no cargo
};
```

`PrimaryAssetType` = `GPUnitDefinition`. Native bootstrap catalog (`UGP_UnitDefinitionCatalog`) provides Worker / Salvage Walker / Defensive Turret / MainBase / LogisticsHub. Authored `DA_GP_Unit_*` assets are operator-side and not required for contracts.

`UGP_CargoComponent` keeps runtime cargo state. Loaded `UnitDefinition.CargoCapacity` configures capacity; empty definition keeps the component fallback (50).

`AGP_UnitBase::UnitDefinitionAsset` is a soft ref. Precedence: loaded definition → existing actor/component defaults.

## AGP_MobileUnit

### Composition (current)

- Inherits `AGP_UnitBase`.
- Owns exactly one `UGP_MovementComponent` (custom RTS NavMesh path + separation backend on `UActorComponent` — **not** CharacterMovement; GP-S33M).
- Commands: `AGP_UnitBase::ReceiveCommand` → `UGP_UnitCommandComponent` (no separate CommandReceiverComponent).
- Attack execution / LOS fire gate currently live in `UGP_UnitCommandComponent` Attack FSM. `UGP_CombatComponent` / `UGP_TargetingComponent` are **not** current required composition (deferred Slice 7 roadmap items).
- Worker-specific components are owned by `AGP_Worker`, not by MobileUnit itself.

Actor Class Defaults pointer category: `GP|Components|Movement`. Tunables on the component remain `GP|Movement`.

## AGP_Unit

Concrete Blueprintable generic mobile unit layer:

- `UCapsuleComponent` root (`GP|Components`).
- `UGP_UnitVisualComponent` (`GP|Components|Visual` pointer; component settings `GP|Visual`).
- Inherits the single `UGP_MovementComponent` from `AGP_MobileUnit`.
- Default visual mode remains `NativeFallback` for generic diagnostics/tests.

## AGP_SalvageWalker

`AGP_SalvageWalker : AGP_Unit` — canonical MVP combat unit (GDD/04).

- Reuses UnitBase Attack FSM / LOS / GAS damage / HealthBar / TeamPresentation / CombatPresentation.
- Canonical baseline (UnitDefinition / ctor fallback): MaxHealth/Health 200, Damage 20, AttackCooldown 1.0, AttackRange 600, Sight 900, MoveSpeed 250.
- `UGP_MovementComponent::MoveSpeed` is initialized from UnitDefinition `MoveSpeedCmPerSecond` when a definition is loaded; ctor 250 remains the empty-ref fallback.
- `UnitVisualComponent.VisualSourceMode = AuthoredComponents` so operator BP visuals are not stacked with NativeFallback InfantryMelee.
- No Cargo/Mining; no CombatComponent / TargetingComponent.

## AGP_Worker

`AGP_Worker : AGP_MobileUnit` — **not** a child of `AGP_Unit`.

Own presentation/resource composition: Capsule root, `PresentationRoot` / cargo & mining anchors, `UGP_CargoComponent`, `UGP_MiningComponent`. Does not use `UGP_UnitVisualComponent` / InfantryMelee archetype path.

## Components

### UGP_MovementComponent

- Owned by `AGP_MobileUnit` (one instance).
- Authority-only `RequestMove` / `StopMove` / `OnMovementResult` used by UnitCommand (Move, Attack approach, AttackMove, Mine, Haul).
- **GP-S33M:** NavMesh path follow via `UNavigationSystemV1` when the unit is on navigable ground; rate-limited repath; lightweight Pawn-overlap separation steering.
- Terminal results: `Reached` / `Cancelled` / **`Failed`** (e.g. PathNotFound, DestinationOffNav, Blocked). Sync reject mirrors nav failures when applicable.
- Missing NavData or unit outside nav coverage → straight-line fallback (diagnostic / isolation safe).
- Not CharacterMovement; not MassAI; no AIController-per-unit requirement.
- Component categories: `GP|Movement`, `GP|Movement|Navigation`, `GP|Movement|Separation`.

### UGP_UnitVisualComponent

- Cosmetic presentation ownership for `AGP_Unit`.
- `NativeFallback` — generates transient Engine basic-shape meshes (`RF_Transient`); cleared on mode switch / EndPlay.
- `AuthoredComponents` — clears generated parts; Blueprint/SCS meshes own presentation.
- `VisualArchetype` (e.g. InfantryMelee) is **cosmetic prototype only** — not a gameplay classification and not Salvage Walker.

### UGP_MiningComponent

- Server-only tick.
- `BeginMining(AActor* Node)` → stores target, starts tick.
- On tick:
  - If in range of the deposit: extract raw Ferronite into the Worker's `UGP_CargoComponent` (planetary, not spendable). When cargo is full, route to the nearest MainBase and drop off into its container `UGP_StorageComponent`.
  - Else: route to MovementComponent::MoveTo(Node->GetActorLocation()).
- No player income GE is applied by mining or drop-off. Spendable `OrbitalFerronite` (+ `FerroniteScore`) is granted only when a full container is launched to orbit (`GE_GP_AddOrbital` + `GE_GP_AddScore` on MainBase). Full cargo→container flow in §Mining Loop below and TDD/06 §Drop-Off Flow.

### UGP_CombatComponent

> **Deferred / not current.** Attack damage + cadence + LOS fire gate are implemented via `UGP_UnitCommandComponent` Attack FSM (GP-S24/S25/S29R path). Do not treat `UGP_CombatComponent` as required composition today.

Historical sketch (roadmap):

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

> **Deferred / not current.** Auto-acquire / TargetingComponent not implemented. Attack requires explicit command target.

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
     - AGP_UnitBase death path (HandleDeathInternal, before LifeSpan): AGP_PlayerState::NotifyPlayerUnitDied decrements CurrentUnits exactly once for Worker / Salvage Walker.
   |
   v
Client: OnRep on Health attribute -> presentation
        Multicast_PlayDeathVFX -> spawn VFX, animation
```

## Replication Hot-Path

Movement — custom `UGP_MovementComponent` (authority path follow + separation; actor transform replication; not CharacterMovement replication path).

Health — through GAS attribute replication.

Position correction — project-specific as implemented by movement/command layers.

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
