# Building Architecture

> **Architecture model:** Orbital Delivery (post-pivot, canon per ADR-0009). All non-initial buildings arrive from orbit via drop pods and land already-operational — there is **no local construction phase and no per-building production queue**. The pre-pivot local-production/local-construction architecture (Barracks, Assembly Yard, `UGP_ProductionComponent`, `UGP_ConstructionComponent`, construction sites, rally points) has been **archived** to [`../Archive/PrePivot_Building_Architecture.md`](../Archive/PrePivot_Building_Architecture.md). Do not implement anything from that archive.

## Class Hierarchy

```
AGP_UnitBase
  AGP_BuildingBase                   // static units; owns ASC + UnitAttributeSet
    AGP_MainBase        (Blueprint child)  // initial deployment; container storage + ship-to-orbit
    AGP_LogisticsHub    (Blueprint child)  // orbital drop; +5 MaxUnits (GP-S33C); container cap deferred
    AGP_DefensiveTurret (Blueprint child)  // orbital drop; auto-attack
    AGP_Wall            (Blueprint child)  // orbital drop (drag-build); see Wall System
    AGP_FerroniteDeposit (Blueprint child) // level-placed natural resource node
```

Building — це **stationary unit** з tag `GP.Unit.Type.Building`. Це дозволяє reuse Health / damage flow.

## AGP_BuildingBase

### Composition

- Inherits `AGP_UnitBase`.
- `USceneComponent` root, `UStaticMeshComponent` visual.
- `UGP_StorageComponent` (on `AGP_MainBase` — hosts the Container System; see §Storage Component and TDD/07).
- `UGP_CombatComponent` (on `AGP_DefensiveTurret` / wall-mounted turret — auto-attack server tick).
- `UGP_WallConnectionComponent` (on `AGP_Wall` — 8-dir auto-connect; see §Wall System).

There is **no** `UGP_ProductionComponent` and **no** `UGP_ConstructionComponent` on any building — these are removed by the Orbital Delivery pivot. Buildings are spawned complete by the drop pod; there is no `EGP_ConstructionState` lifecycle.

### Replicated Properties

Inherits from `AGP_UnitBase`. Plus:

- `UPROPERTY(ReplicatedUsing=OnRep_BuildingDefinition) TSoftObjectPtr<UGP_BuildingDefinition> BuildingDefinitionAsset;`
- `UPROPERTY(Replicated) FIntPoint OriginCell;` — grid coordinates (see §Build Grid System).
- `UPROPERTY(Replicated) FIntPoint FootprintSize;` — mirrored from Definition at spawn.

## UGP_BuildingDefinition (Data Asset)

```cpp
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_BuildingDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, Category = "GP|Identity")
    FGameplayTagContainer BuildingTags;

    UPROPERTY(EditAnywhere, Category = "GP|Economy")
    int32 Cost = 0;                                              // OrbitalFerronite cost of the drop order

    UPROPERTY(EditAnywhere, Category = "GP|Vitals")
    float MaxHealth = 500.f;

    UPROPERTY(EditAnywhere, Category = "GP|Effects")
    TArray<TSoftClassPtr<UGameplayEffect>> EffectsOnPlacement;   // async-loaded, applied on pod landing, e.g., GE_GP_UnitCap_Plus5

    UPROPERTY(EditAnywhere, Category = "GP|Abilities")
    TArray<TSoftClassPtr<UGameplayAbility>> GrantedAbilities;    // async-loaded

    UPROPERTY(EditAnywhere, Category = "GP|Visuals")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, Category = "GP|Components")
    TSoftClassPtr<AGP_BuildingBase> SpawnedClass;     // async-loaded by AssetManager
};
```

`EffectsOnPlacement` apply the moment the drop pod lands (no construction wait). There is no `BuildTime`, no `AllowedProductions` — buildings do not build other things; new units/buildings come from the global Order Menu (orbital). Grid fields (`FootprintCells`, `ClearanceCells`, `bMountsOnWall`, `bCanHostWallMount`) and economy fields (`bSellable`, `SellRefundRate`) are documented in §Build Grid System and §Sell + Demolish System.

## Building Lifecycle — Orbital Procurement

**Усе крім initial MainBase прибуває з орбіти.** Player не будує локально.

```
Purchase → GE_GP_SpendOrbital → READY inventory++
Deploy ghost (LMB) → READY-- → DropPod → AGP_BuildingBase operational
Esc/RMB cancel deploy → READY unchanged (no refund; purchase already paid)
```

Units use a **separate** Unit Delivery path to MainBase Unit Drop Zone (manifest + transport slots) — see TDD/14 / GDD/10. Buildings do **not** land on the Unit Drop Zone.

Building deploy still uses grid/FoW validation when those systems exist (TDD/06 grid + TDD/15). EffectsOnPlacement apply on landing.

MVP buildings list:

| Building | Source | Function |
| --- | --- | --- |
| `AGP_MainBase` | Initial deployment (game start, pre-placed per faction StartingBuildings) | Container storage + launch + Worker drop-off + **Unit Drop Zone** + sight |
| `AGP_LogisticsHub` | Orbital purchase → READY → deploy | +5 MaxUnits + expanded container cap (DA-tunable) + sight |
| `AGP_DefensiveTurret` | Orbital purchase → READY → deploy | Auto-attack SWARM/enemy у range + sight |
| `AGP_Wall` / `AGP_WallTurret` | Orbital purchase → READY → deploy (drag later) | Perimeter defense; turret mounts on wall |
| `AGP_FerroniteDeposit` | Level-placed (natural) | Resource node, not player-controlled |

## Storage Component

`UGP_StorageComponent` lives on `AGP_MainBase` and hosts the **Container System** — the two-state Ferronite economy. Workers drop raw planetary Ferronite into containers; full containers are launched to orbit (converting to spendable OrbitalFerronite + cumulative FerroniteScore). See §Container Storage below and TDD/07 for the full container model.

## Damage / Destruction Flow

Same as Unit (через UnitAttributeSet Health). When Health → 0:

- Server applies tag `GP.Unit.State.Dead` to building (loose tag).
- Multicast destruction VFX.
- Schedule destroy з delay.
- Remove any source-bound infinite effects (e.g., `GE_GP_UnitCap_Plus5` on Logistics Hub).
- Якщо building is Main Base — server тригерить win/lose check (annihilation path — see §MainBase).

## Resource Node (FerroniteDeposit)

`AGP_FerroniteDeposit : AGP_BuildingBase` з `BuildingTags = {GP.Unit.Type.Building, GP.Building.Type.FerroniteDeposit}`.

- Level-placed (natural) — not player-controlled, not orbital-dropped.
- No combat, no effects, no orbital order.
- Workers can `GP.Command.Mine` on it.
- Reserve / depletion rules per GP-0303.

## Detailed Main Base Rules (GP-0301)

Stage — design only (per [`Claude_Tasks/GP-0301_Main_Base`](../Development/Claude_Tasks/GP-0301_Main_Base.md)). `AGP_MainBase : AGP_BuildingBase` — economic anchor; pre-deployed at match start. Owns the Container System via `UGP_StorageComponent`. **No local production, no construction, no rally point** (those came from the pre-pivot model — see archive).

### Identity

- Class: `AGP_MainBase : AGP_BuildingBase` (BP child of native abstract).
- DataAsset: `DA_GP_Building_MainBase` (per GDD/05).
- Tags: `GP.Unit.Type.Building`, `GP.Building.Role.Command`, `GP.Building.Type.MainBase`, `GP.Faction.Corporate`.
- Capability tags (per GP-0202): `{Selectable, Inspectable, Selection.Type.Building}`.
- AllowedCommands: `{Command_Stop, Command_LaunchContainer}` — static + non-combat. **No** `Command_Move`, `Command_Attack`, `Command_AttackMove`, and **no** rally/production commands (those are pre-pivot — see archive).

### DataAsset Schema (DA_GP_Building_MainBase)

`UGP_BuildingDefinition`-derived fields:

| Field | Type | Value / Notes |
| --- | --- | --- |
| `DisplayName` | `FText` | "Main Base" |
| `BuildingTags` | `FGameplayTagContainer` | per Identity above |
| `MaxHealth` | `float` | TBD balance |
| `Armor` | `float` | TBD balance |
| `Cost` | `int32` | `0` (not orderable — initial deployment) |
| `bProvidesDropOff` | `bool` | `true` |
| `DropOffRange` | `float` | TBD cm (per GDD/05) |
| `BaseMaxContainerCount` | `int32` | TBD — initial container slots on the Storage Component |
| `bSelfRegen` | `bool` | `false` (per decision — only Worker Repair) |
| `RepairAbility` | `TSoftClassPtr<UGameplayAbility>` | `UGP_GA_Repair` (на Worker side; async-loaded) |
| `Mesh` | `TSoftObjectPtr<UStaticMesh>` | per asset pass |
| `Icon` | `TSoftObjectPtr<UTexture2D>` | per asset pass |
| `Materials` | `TArray<TSoftObjectPtr<UMaterialInterface>>` | per asset pass |

> No `BuildTime`, `AllowedProductions`, `MaxProductionQueue`, `bSupportsRallyPoint`, `DefaultRallyOffset`, `bCanProduce`. MainBase does **not** produce units — Workers and all buildings/units come from the orbital Order Menu.

### Container Storage

`UGP_StorageComponent` on MainBase hosts the Container System (full model in TDD/07):

- Workers drop raw planetary Ferronite into the nearest open container; planetary Ferronite is **not spendable**.
- A full (or player-launched) container is sent to orbit: on launch, server applies `GE_GP_AddOrbital` (spendable `OrbitalFerronite += amount`) **and** `GE_GP_AddScore` (`FerroniteScore += amount`, cumulative victory score) to the owning PlayerState ASC.
- Filling a container is a Storage state mutation (not a player GE). Launching a container is the only point where a GE is applied to the player.
- `EffectiveMaxContainerCount = BaseMaxContainerCount + sum of alive Logistics Hub bonuses`. See §Storage Component (Containers).

### Drop-Off Flow

(per GDD/05 + TDD/07 `UGP_StorageComponent`):

- Worker з повним cargo рухається до nearest `MainBase` із `bProvidesDropOff=true`.
- Within `DropOffRange` and within sphere overlap — `UGP_MiningComponent::TryDropOff()`:
  - `MainBase->StorageComponent->ReceiveDeposit(Amount, Ferronite)` — raw Ferronite enters a container (planetary, not spendable).
  - This raises the GameState `FerroniteThreatValue` (raw stored-at-base stock) — see §Swarm Threat Coupling. No player GE is applied here.
  - Worker resumes mining cycle.
- Drop-off blocked if `MainBase->IsAlive() == false`.

### Swarm Threat Coupling

`FerroniteThreatValue` (GameState) = raw Ferronite currently stored in MainBase containers. Drop-off raises it; launching a container to orbit lowers it. Swarm wave size/frequency scale from `FerroniteThreatValue` via DA curves (`ThreatToWaveSize`, `ThreatToWaveFrequency`). `FerroniteScore` and `OrbitalFerronite` do **not** drive Swarm pressure. This is the greed-vs-safety loop: hoarding raw Ferronite at base for a big launch attracts heavier waves. See TDD Swarm spec + memory rule `project_swarm_escalation` for the full coupling.

### Worker Repair Ability

`UGP_GA_Repair` (on Worker):

- Tag: `GP.Ability.Repair`.
- Input: `GP.Command.Repair` (RMB on own damaged building).
- Activation: server-only (GAS authority).
- Channel: continuous tick while in range AND target damaged AND Worker has cargo space (repair doesn't need empty cargo).
- Cost: `GE_GP_Cost_RepairTick` per tick — `OrbitalFerronite -= TickCost` (TickCost = TBD placeholder).
- Heal: `GE_GP_Heal(RepairAmountPerTick)` (heal-per-tick = TBD placeholder).
- Cancel conditions: Target full HP, target destroyed, Worker out of range, insufficient OrbitalFerronite, new command issued, Worker death.
- Range: `200 cm` from building bounds.
- Visual: repair tool VFX on Worker, sparks on building.
- New tag: `GP.Command.Repair`, `GP.Ability.Repair`.

### Destruction Flow (per GDD/08 — annihilation path)

```
GE_GP_Damage_Basic on MainBase
  → Health <= 0
  → AGP_BuildingBase::OnHealthZero (existing)
    1. Set tag GP.Unit.State.Dead.
    2. Disable input listeners (no drop-off, no container launch).
    3. UGP_StorageComponent: all planetary Containers destroyed (raw Ferronite lost); any active launch aborts.
    4. Multicast destruction VFX + sound.
    5. Delay → Destroy() (allow rubble pass).
  → AGP_GameMode::OnUnitDied(MainBase, KillerPlayerState)
    - Building has tag GP.Building.Type.MainBase.
    - Win/Lose check: if bAnnihilationCountsAsWin, opponent wins (WinReason GP.Match.WinReason.Annihilation).
  → Player A loses ability to:
    - Receive Ferronite drop-off (no container to store into).
    - Launch containers to orbit (no path to OrbitalFerronite / FerroniteScore → no path to victory).
    - Repair (no OrbitalFerronite refill source).
```

**Why annihilation matters (pivot change):** without a MainBase the player can no longer ship containers to orbit, so there is no path to reach `DeliveryQuotaFerroniteScore`. MainBase loss is therefore decisive (annihilation win for the opponent when `bAnnihilationCountsAsWin`), not a mere consequence as in the pre-pivot model.

### State on `AGP_MainBase`

```cpp
UPROPERTY(VisibleAnywhere)
TObjectPtr<UGP_StorageComponent>   StorageComponent;   // Container System host

// OriginCell / FootprintSize replicated on AGP_BuildingBase (grid).
```

Health, MaxHealth, Armor — через `UGP_UnitAttributeSet` (per TDD/02 / TDD/05). No production component, no rally state.

### Tag Surface

| Tag | Owner | Purpose |
| --- | --- | --- |
| `GP.Building.Type.MainBase` | DataAsset | Identity / match-end query |
| `GP.Building.Role.Command` | DataAsset | UI grouping, HUD readout |
| `GP.Command.Repair` | native | Worker repair trigger |
| `GP.Ability.Repair` | native | Worker ability tag |

### UI Surface (cross-ref до GP-0401)

- Selection → WBP_GP_HUD_SelectionPanel building mode:
  - HP bar, name, faction tint.
  - Container status: filled / total containers, "Launch" button per full container (sends to orbit).
  - Drop-off zone indicator (decal, fade при deselect).
- Ordering Workers / buildings is done from the **global Order Menu** (orbital), not from MainBase selection.

### Validation Checklist (Stop Condition)

- [x] Base receives damage — through `UGP_UnitAttributeSet.Health` via `GE_GP_Damage_Basic`.
- [x] Base destruction affects match — drop-off + launch stop; annihilation win check per GDD/08.
- [x] Health server-authoritative — GAS replication, client read-only.
- [x] DataAsset fields enumerated, tags listed, ownership clear.

### Open Questions

1. **Annihilation toggle:** `bAnnihilationCountsAsWin` default true у MVP; confirm against GDD/08.
2. **Container launch trigger:** auto-launch on full vs manual player launch (greed lever). Recommend manual launch so player chooses when to lower `FerroniteThreatValue`.
3. **Drop-off range visualization:** debug-only ring у dev builds, hidden у shipping?
4. **Starting Workers:** how many Workers pre-deployed per Faction StartingUnits at match start?

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Drop-off into container | Worker з cargo → walks to base, deposits → planetary container fills; `FerroniteThreatValue` rises. No player attribute changes yet. |
| 2 | Launch container | Player launches full container → `OrbitalFerronite += amount` (GE_GP_AddOrbital), `FerroniteScore += amount` (GE_GP_AddScore), `FerroniteThreatValue` drops. |
| 3 | Container cap | Containers all full → no more drop-off until a container is launched. |
| 4 | Repair under fire | Worker repairs MainBase while it takes damage → repair fights damage; if dps > heal, base dies. |
| 5 | Repair cancel on death | Worker dies mid-repair → ability ends cleanly, no stuck VFX. |
| 6 | Base destruction | Health 0 → containers destroyed, drop-off + launch blocked, annihilation win check fires. |
| 7 | Threat-driven waves | Hoard raw Ferronite (high `FerroniteThreatValue`) → Swarm waves intensify per `ThreatToWaveSize`/`ThreatToWaveFrequency`. |
| 8 | Concurrent damage from two attackers | Damage stacks correctly, Health monotone decreases. |

### Out of MVP (Main Base)

- Multiple bases per player (only 1 MainBase у MVP).
- Base upgrades (level 2/3).
- Base mobility / repositioning.
- Score-on-destroy bonus.
- Container auto-prioritization / sorting.

## References

- Building data — [`../GDD/05_Buildings`](../GDD/05_Buildings.md).
- Resource flow — [`07_Resource_Architecture`](07_Resource_Architecture.md).
- Unit base — [`05_Unit_Architecture`](05_Unit_Architecture.md).
- GAS effects — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- Win/Lose conditions — [`../GDD/08_Win_Lose_Conditions`](../GDD/08_Win_Lose_Conditions.md).
- Commands integration — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md).
- Orbital Delivery GDD / TDD — [`../GDD/10_Orbital_Delivery`](../GDD/10_Orbital_Delivery.md), [`14_Orbital_Delivery`](14_Orbital_Delivery.md).
- Pre-pivot architecture (archived) — [`../Archive/PrePivot_Building_Architecture.md`](../Archive/PrePivot_Building_Architecture.md).
- Main Base task — [`../Development/Claude_Tasks/GP-0301_Main_Base`](../Development/Claude_Tasks/GP-0301_Main_Base.md).

## Detailed Logistics Hub Rules (GP-0304)

Stage — design only (task file rename pending GP-0802: `GP-0304_Barracks.md` → `GP-0304_Logistics_Hub.md`). Building name: **Logistics Hub** (was "Barracks" / "Assembly Yard" in the pre-pivot model). `AGP_LogisticsHub : AGP_BuildingBase`. Arrives **only** via orbital drop — there is no local build path, no construction phase, and no production queue.

### Identity

- Class: `AGP_LogisticsHub : AGP_BuildingBase` (BP child of native abstract).
- DataAsset: `DA_GP_Building_LogisticsHub` (per GDD/05).
- Tags: `GP.Unit.Type.Building`, `GP.Building.Role.Support`, `GP.Building.Type.LogisticsHub`, `GP.Faction.Corporate`, `GP.Capability.GrantsVision`.
- Capability tags: `{Selectable, Inspectable, Selection.Type.Building}`.
- AllowedCommands: `{Command_Stop, Command_Sell}` — passive support building; no rally, no production.

### DataAsset Schema (DA_GP_Building_LogisticsHub)

All numerics are placeholders pending balance pass.

| Field | Type | Placeholder | Notes |
| --- | --- | --- | --- |
| `DisplayName` | `FText` | "Logistics Hub" | |
| `BuildingTags` | `FGameplayTagContainer` | per Identity | |
| `Cost` | `int32` | TBD | OrbitalFerronite cost of the drop order. |
| `MaxHealth` | `float` | TBD | |
| `Armor` | `float` | TBD | |
| `UnitCapContribution` | `int32` | `+5` | Applied via `GE_GP_UnitCap_Plus5` on landing. |
| `ContainerCapContribution` | `int32` | `+N` (TBD) | Added to owning MainBase `EffectiveMaxContainerCount` on landing. |
| `bProvidesDropOff` | `bool` | `false` | No drop-off at Logistics Hub. |
| `EffectsOnPlacement` | `TArray<TSoftClassPtr<UGameplayEffect>>` | `[GE_GP_UnitCap_Plus5]` | Applied on pod landing (no construction). |
| `bSellable` | `bool` | `true` | Per Sell + Demolish system. |
| `SellRefundRate` | `float` | `0.5` | |
| `RepairAbility` | `TSoftClassPtr<UGameplayAbility>` | `UGP_GA_Repair` | Worker-side; async-loaded. |
| `Mesh` / `Materials` / `Icon` | art refs | per asset pass | |

### Placement Effects (On Drop-Pod Landing)

The Logistics Hub is purely passive — no production, no construction. On the moment the drop pod lands:

1. Spawn final `AGP_LogisticsHub` (already operational).
2. Apply `GE_GP_UnitCap_Plus5` (Infinite duration, source-bound to this building) → `MaxUnits += UnitCapContribution`.
3. Add `ContainerCapContribution` to the owning player's MainBase `EffectiveMaxContainerCount` (additive storage cap; recomputed on hub place/destroy). **GP-S33C deferred:** N is still TBD — do not invent a container-cap bonus. This slice implements **only** +5 MaxUnits.
4. Register as a vision source (FoW sight, per TDD/15).

### Unit Cap Clamp — Building Destroyed

Decision: existing units **survive** when MaxUnits clamps below CurrentUnits; new orders block until natural attrition.

1. Logistics Hub destroyed (combat or sell) → `RemoveActiveGameplayEffectBySourceEffect(GE_GP_UnitCap_Plus5, source=this)`.
2. `MaxUnits -= UnitCapContribution`. `EffectiveMaxContainerCount -= ContainerCapContribution` on MainBase.
3. `CurrentUnits` unchanged → may temporarily exceed `MaxUnits` (coherent, no auto-kill).
4. Order Menu validation rejects new Unit drops while `CurrentUnits + 1 > MaxUnits` (`EReason::UnitCapReached`).
5. Cap unblocks on natural death (CurrentUnits decrement) OR a new Logistics Hub drop.

HUD displays "Cap: X/Y (over)" з червоним tint доки `CurrentUnits > MaxUnits`.

### Damage / Destruction

Reuses `AGP_BuildingBase` standard:

- Health → `UGP_UnitAttributeSet.Health`; damage via `GE_GP_Damage_Basic`.
- Destruction → tag `GP.Unit.State.Dead` → multicast destruction VFX → delay → destroy actor.
- `RemoveActiveGameplayEffectBySourceEffect(GE_GP_UnitCap_Plus5)` + container cap recompute per Cap Clamp rule above.

### Smart-Command Resolution (Logistics Hub subset)

| Hit target | Logistics Hub selected → command |
| --- | --- |
| Ground | No command (passive building, no rally). |
| Own building | No command. |
| Enemy | No command. Tooltip "Logistics Hub cannot attack". |

### Tag Surface

| Tag | Purpose |
| --- | --- |
| `GP.Building.Type.LogisticsHub` | Identity |
| `GP.Building.Role.Support` | UI grouping |
| `GP.Capability.GrantsVision` | FoW sight source marker |
| `GP.Effect.Source.UnitCapBuilding` | Marks `GE_GP_UnitCap_Plus5` source family (для query) |

### Validation Checklist (Stop Condition)

- [x] Cap / container contribution not hardcoded — DA-driven (`UnitCapContribution`, `ContainerCapContribution`).
- **GP-S33C factual:** native `UGP_GE_UnitCap_Plus5` Additive +5 is live (C++ GE, not BuildingDefinition DA). Container-cap bonus remains deferred (N TBD).
- [x] Arrives via orbital drop only — no `Server_BuildAt`, no construction component.
- [x] UI shows cap increase — `MaxUnits` attribute change delegate → HUD ResourceReadout.
- [x] Cap removal source-bound — `RemoveActiveGameplayEffectBySourceEffect` on destroy.

### Open Questions

1. **ContainerCapContribution value:** placeholder; balance pass.
2. **Multiple Logistics Hubs:** cap + container bonuses stack additively. Confirm no soft cap у MVP.
3. **Vision radius:** medium sight source value TBD.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Order Logistics Hub | Order from menu → OrbitalFerronite spent (GE_GP_SpendOrbital), pod descends, lands, hub operational. |
| 2 | UnitCap rise | Hub lands → `MaxUnits +5` → HUD readout flashes. |
| 3 | Container cap rise | Hub lands → MainBase `EffectiveMaxContainerCount` increases per DA. |
| 4 | Cap clamp on destroy | At cap, hub destroyed → MaxUnits drops; existing units survive; new unit drops reject. |
| 5 | Cap unblock on attrition | Units die → CurrentUnits ≤ MaxUnits → unit drops resume. |
| 6 | Sell hub | Sell → partial OrbitalFerronite refund; cap + container bonus removed. |

### Out of MVP (Logistics Hub-specific)

- Tech tree / upgrades.
- Production of any kind (orbital model — no local production).
- Researchable buffs.

## Detailed Defensive Turret Rules (GP-0305)

`AGP_DefensiveTurret : AGP_BuildingBase`:

- Source: orbital drop only (no local build path).
- Auto-attack enemies / SWARM у range (uses standard `UGP_CombatComponent` server-tick).
- Sight source (medium) — `GP.Capability.GrantsVision`.
- Tags: `GP.Unit.Type.Building`, `GP.Building.Type.DefensiveTurret`, `GP.Capability.GrantsVision`.
- `bSellable = true`, `SellRefundRate = 0.5` (per Sell + Demolish system).
- Detail spec — separate task, not yet drafted (GP-0305).

## Storage Component (Containers)

`UGP_StorageComponent` lives on MainBase and hosts the Container System (full model in TDD/07):

```cpp
UPROPERTY(ReplicatedUsing=OnRep_Containers)
TArray<FGP_Container> Containers;

UPROPERTY(EditDefaultsOnly)
int32 BaseMaxContainerCount = 5;             // from MainBase DA (placeholder)

UPROPERTY(ReplicatedUsing=OnRep_StorageCap)
int32 EffectiveMaxContainerCount = 5;        // Base + sum of all alive Logistics Hub bonuses
```

`EffectiveMaxContainerCount` recalculated on Logistics Hub place / destroy events. Stored on MainBase (player has one MainBase у MVP). Raw Ferronite is **planetary** (in containers, not spendable) until a container is launched to orbit; on launch `GE_GP_AddOrbital` + `GE_GP_AddScore` apply (see §Container Storage on MainBase).

## Building Damage / Destruction by Type

- **MainBase** destroyed → `UGP_StorageComponent.Containers` all destroyed (planetary Ferronite lost forever); active launch aborts; drop-off blocked. Annihilation win check (per GDD/08).
- **Logistics Hub** destroyed → `MaxUnits -= UnitCapContribution`, `EffectiveMaxContainerCount -= ContainerCapContribution`. Existing containers stay until launched / lost.
- **DefensiveTurret** destroyed → standard `AGP_BuildingBase` destruction.

## Building Tag Surface

| Tag | Purpose |
| --- | --- |
| `GP.Building.Type.MainBase` | MainBase identity / match-end query |
| `GP.Building.Type.LogisticsHub` | Logistics Hub identity |
| `GP.Building.Type.DefensiveTurret` | Defensive Turret identity |
| `GP.Building.Type.Wall` / `GP.Building.Type.WallTurret` | Wall + wall-mounted turret (see §Wall System) |
| `GP.Building.Type.FerroniteDeposit` | Level-placed resource node |
| `GP.Command.OrderDrop` | Orbital order intent |
| `GP.Command.Repair` / `GP.Ability.Repair` | Worker repair (см. TDD/05) |
| `GP.Command.Sell` / `GP.Command.Demolish` | Sell building / demolish wall (see §Sell + Demolish) |
| `GP.State.PodInFlight` | Drop pod transient state |
| `GP.Capability.GrantsVision` | FoW sight source marker |
| `GP.Effect.Source.UnitCapBuilding` | Marks `GE_GP_UnitCap_Plus5` source family (for query) |

Deprecated (pre-pivot, do not use): `GP.Building.Type.Barracks`, `GP.Building.Type.AssemblyYard`, `GP.Command.QueueProduction`, `GP.Command.CancelProduction`, `GP.Command.SetRallyPoint`, `GP.Command.Build`.

## Build Grid System (2026-05-16 — adds Walls + grid mechanic)

**All buildings snap to а tile grid.** Replaces free-form world-coordinate placement. Per [`../GDD/05_Buildings`](../GDD/05_Buildings.md) §Build Grid System і memory rule `project_build_grid_and_walls`.

### Grid Definition

- **Cell size:** 200 cm × 200 cm (default; DA-driven `UGP_BuildGridConfig.CellSize`).
- **Grid origin:** projected на NavMesh від world (0,0).
- **Coordinates:** `FIntPoint Cell{X, Y}`. World location of cell center = `Origin + (Cell.X * CellSize, Cell.Y * CellSize, GroundZ)`.

### UGP_BuildGridSubsystem (UWorldSubsystem)

```cpp
UCLASS()
class GPRUNTIME_API UGP_BuildGridSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Cell size from config, set at BeginPlay. */
    UPROPERTY(EditDefaultsOnly)
    float CellSize = 200.f;

    /** World-to-cell. */
    FORCEINLINE FIntPoint WorldToCell(FVector World) const;

    /** Cell-to-world (center of cell). */
    FORCEINLINE FVector CellToWorld(FIntPoint Cell) const;

    /** Server: register actor's footprint cells. */
    void RegisterFootprint(AActor* Building, FIntPoint OriginCell, FIntPoint FootprintSize);

    /** Server: unregister on destroy. */
    void UnregisterFootprint(AActor* Building);

    /** Server query: is cell occupied? Optionally exclude an actor. */
    bool IsCellOccupied(FIntPoint Cell, AActor* IgnoreActor = nullptr) const;

    /** Server query: would footprint placement be valid? */
    bool CanPlaceFootprint(FIntPoint OriginCell, FIntPoint FootprintSize, int32 ClearanceCells, AActor* OwnerActor, EGP_GridRejectReason& OutReason) const;

    /** A* pathfind for wall drag-build. Returns list of cells. */
    bool PathfindFreeCells(FIntPoint Start, FIntPoint End, int32 FootprintSize, int32 ClearanceCells, TArray<FIntPoint>& OutPath) const;

    /** Get actor at cell (or nullptr). */
    AActor* GetActorAtCell(FIntPoint Cell) const;

protected:
    /** Cell occupancy map: cell → actor. Server-only. */
    TMap<FIntPoint, TWeakObjectPtr<AActor>> CellOccupancy;

    /** Reverse lookup: actor → its cells. */
    TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>> ActorCells;
};

UENUM()
enum class EGP_GridRejectReason : uint8
{
    Free,
    CellOccupied,
    ClearanceViolation,
    OutOfBounds,
    NotNavigable
};
```

**Replication strategy:** subsystem itself не replicated. Cell state inferred client-side from replicated building actor footprints (own actor `BuildingDefinition.FootprintCells` + actor location → cells). Client mirrors authoritative server cells implicitly. Robust + cheap.

### UGP_BuildingDefinition Update

```cpp
// Additions to existing UGP_BuildingDefinition:
UPROPERTY(EditAnywhere, Category = "GP|Grid")
FIntPoint FootprintCells = FIntPoint(1, 1);

UPROPERTY(EditAnywhere, Category = "GP|Grid")
int32 ClearanceCells = 0;            // exclusion radius from other structures

UPROPERTY(EditAnywhere, Category = "GP|Grid")
bool bMountsOnWall = false;          // for wall-mounted variants

UPROPERTY(EditAnywhere, Category = "GP|Grid")
bool bCanHostWallMount = false;      // for Wall — accepts mount of bMountsOnWall buildings
```

### AGP_BuildingBase Update

```cpp
// New replicated state:
UPROPERTY(Replicated)
FIntPoint OriginCell;                // grid coordinates

UPROPERTY(Replicated)
FIntPoint FootprintSize;             // mirrored from Definition at spawn

virtual void BeginPlay() override
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        UGP_BuildGridSubsystem* Grid = GetWorld()->GetSubsystem<UGP_BuildGridSubsystem>();
        Grid->RegisterFootprint(this, OriginCell, FootprintSize);
    }
}

virtual void EndPlay(const EEndPlayReason::Type Reason) override
{
    if (HasAuthority())
    {
        if (auto* Grid = GetWorld()->GetSubsystem<UGP_BuildGridSubsystem>())
            Grid->UnregisterFootprint(this);
    }
    Super::EndPlay(Reason);
}
```

### Drop Pod Validation Update (per TDD/14)

`UGP_OrbitalDeliverySubsystem::ValidateDropZone` тепер uses grid:

1. Convert `LandingLocation` → `OriginCell` через grid.
2. Read `DropDef.PayloadFootprint` (mirrored from BuildingDefinition).
3. Call `BuildGrid->CanPlaceFootprint(OriginCell, Footprint, Clearance, nullptr, OutReason)`.
4. If false → reject specific reason.

Reticle on client similarly grid-snaps + queries `BuildGrid` для valid/invalid feedback.

## Wall System (GP-0305)

`AGP_Wall : AGP_BuildingBase`. Custom logic on top of base + `UGP_WallConnectionComponent`.

### UGP_WallConnectionComponent

```cpp
UCLASS()
class GPRUNTIME_API UGP_WallConnectionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    /** Bitfield: 8 bits = N, NE, E, SE, S, SW, W, NW neighbor presence. */
    UPROPERTY(ReplicatedUsing=OnRep_ConnectionBitfield)
    uint8 ConnectionBitfield = 0;

    /** Recompute neighbors and update bitfield. Server-only. */
    void RecomputeFromNeighbors();

    /** Called when grid neighborhood changes (wall placed/destroyed nearby). */
    void OnNeighborChanged();

protected:
    UFUNCTION()
    void OnRep_ConnectionBitfield();     // client: triggers visual state update

    void UpdateVisualState();             // client + server: maps bitfield → mesh/material
};
```

**Neighbor query:** at `OriginCell`, scan 8 directions у grid. Якщо cell occupied by `AGP_Wall` of same team → bit set. AI-team walls also count (own perimeter shared із own AI ally if multi-team, але post-MVP).

**Bitfield → visual mapping:** material parameter `ConnectionMask : Scalar` (compressed 8 bits як float, decoded у shader OR 8 scalar params OR direct mesh-swap). Recommended MVP: **mesh-swap** для clarity + perf. 16 mesh variants cover all combinations (after rotation reuse):

- 0x00 — Standalone (no neighbors).
- 0x01..0x80 (each direction alone) — End piece, 8 rotations.
- Straight pairs → Straight horizontal / vertical (2 base meshes).
- 90° pairs → Corner (4 rotations).
- 3-neighbor → T-junction (4 rotations).
- 4-neighbor → Cross.
- Higher-order combinations (e.g., 5+ neighbors з diagonals) — fold у nearest visual approximation.

Mesh resolution per bitfield — lookup table. Implementation у `UGP_WallConnectionComponent::UpdateVisualState()`.

### Wall Drag-Build Flow

```
Player opens Order Menu → selects Wall → enters wall-drag mode (special variant of drop-targeting):
  Client side:
    - On mouse-move: snap cursor to grid cell.
    - If LMB-pressed (drag mode): record `DragStart`, draw preview.
    - On drag: request `Server_PreviewWallPath(DragStart, CurrentCursor)` (rate-limited 4 Hz).
       - Server runs A* on free cells with clearance OK.
       - Returns path cell list.
    - HUD draws ghost wall segments along path (transient AGP_GhostWallSegment actors, local-only).
  On LMB-release:
    - Client sends Server_BuildWallPath(DragStart, DragEnd).
    - Server re-validates path (anti-cheat).
    - Computes Cost = PathLength × WallSegmentCost.
    - Validates OrbitalFerronite >= Cost.
    - Spawns drop pods for each path cell sequentially (0.2 s stagger).
    - Each wall lands → RegisterFootprint → triggers OnNeighborChanged on adjacent walls.
  On RMB / Esc:
    - Cancel mode, no spend.
```

### Server_PreviewWallPath / Server_BuildWallPath

```cpp
UFUNCTION(Server, Unreliable, WithValidation)
void Server_PreviewWallPath(FIntPoint Start, FIntPoint End);
// Server response via Client_DeliverPathPreview RPC

UFUNCTION(Server, Reliable, WithValidation)
void Server_BuildWallPath(FIntPoint Start, FIntPoint End);
// Validates spend + path, drops walls
```

Preview RPC unreliable (per-frame OK to drop). Build RPC reliable (one-shot, не drop).

### A* Pathfinding on Grid

```cpp
bool UGP_BuildGridSubsystem::PathfindFreeCells(
    FIntPoint Start, FIntPoint End, int32 FootprintSize, int32 ClearanceCells,
    TArray<FIntPoint>& OutPath) const
{
    // Standard A* on grid.
    // Open set: priority queue by f-score.
    // Closed set: visited.
    // Heuristic: Chebyshev distance (allows diagonal movement).
    // For each candidate neighbor (8-connected):
    //   - Within map bounds?
    //   - All FootprintSize cells free?
    //   - Clearance OK around footprint?
    //   - NavMesh navigable (cached query)?
    // If yes: add to open with g + heuristic.
    // Return path on End reached, or false if no path.

    // Bound iterations: max 200 nodes explored (prevents long-pause spike).
    // Max path length: 50 cells (prevents map-spanning wall orders).
}
```

Pathfinding tuning DA-driven: `UGP_BuildGridConfig.MaxPathfindIterations`, `.MaxPathLength`.

### Wall-Mounted Turret Validation

`UGP_OrbitalDeliverySubsystem::ValidateDropZone` augmented:

```
If DropDef.bMountsOnWall:
    Cell = WorldToCell(LandingLoc)
    ExistingActor = BuildGrid.GetActorAtCell(Cell)
    If ExistingActor is not AGP_Wall OR ExistingActor's Definition.bCanHostWallMount == false:
        return EReason::NotOnWall
    If wall already hosts a wall-mounted turret:
        return EReason::WallSlotOccupied
    proceed
```

Wall-mounted turret spawns at wall's cell. Wall's own footprint remains registered. Wall-Turret registers separately із same cell coordinate (allowed because `bMountsOnWall`).

When wall destroyed → wall-mounted turret destroyed cascade (`OnDestroyed` delegate fires `OwnerCellDestroyed` to mounted variants → they self-destruct).

### Tag Surface (Wall)

| Tag | Purpose |
| --- | --- |
| `GP.Building.Type.Wall` | Wall identity |
| `GP.Building.Type.WallTurret` | Wall-mounted turret identity |
| `GP.Capability.WallMountable` | DropDef flag — must mount on wall |
| `GP.Capability.HostsWallMount` | Wall capability — accepts mounts |
| `GP.Drop.Type.Wall` | Drop classification (special drag-build pipeline) |

### Performance Budget

- Grid cell occupancy: hash map lookup O(1). Cheap.
- Wall neighbor recompute on placement: O(8 cells) — negligible per event.
- A* pathfinding: bounded 200 iterations, 4 Hz client preview rate → ~800 evaluations/sec server-side under active drag. Acceptable.
- Replication: per-wall `ConnectionBitfield` is 1 byte; replicated only on change. Negligible bandwidth.
- Grid overlay rendering у HUD: tile texture overlay (decal або material), only visible у drop-targeting mode. Negligible GPU.

### Anti-Patterns

- ❌ Free-form world-coord placement bypassing grid — banned.
- ❌ Client-side cell occupancy decisions (cheating surface).
- ❌ Wall pathfinding на client — must be server (anti-cheat).
- ❌ Walls overlapping existing buildings — server rejects.
- ❌ Wall-mounted turret without wall present — server rejects.
- ❌ Hardcoded grid cell size або footprint values у C++ — DA-driven.

### Playtest Scenarios (Wall + Grid)

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Single wall drop | Click on free cell з clearance OK → wall lands, ConnectionBitfield=0, Standalone mesh. |
| 2 | Wall adjacent to existing wall | Drop 2nd wall N of 1st → both update mesh: 1st bitfield S=1 (end pointing S), 2nd bitfield N=1 (end pointing N). Or both Straight if forming line. |
| 3 | Wall corner | Drop walls forming L → corner mesh on join. |
| 4 | Wall T-junction | Drop 3 walls у T → middle wall T-junction mesh. |
| 5 | Wall cross | Drop 4 walls forming + → middle wall Cross mesh. |
| 6 | Drag-build straight | Drag from A to B (5 cells apart, no obstacles) → 5 walls placed forming straight line. |
| 7 | Drag-build around obstacle | Drag з existing turret on path → snake-path routes around, walls placed avoiding cells too close to turret. |
| 8 | Drag-build з insufficient orbital | Drag 10-cell path, OrbitalFerronite < 10×Cost → reject all, no walls placed. |
| 9 | Clearance violation | Try place wall within 2 cells of MainBase → red reticle, reject. |
| 10 | Wall-mounted turret placement | Drop WallTurret on existing wall cell → mounted, visual sits on top of wall. |
| 11 | WallTurret on free cell | Try drop WallTurret on grass (no wall under) → reject NotOnWall. |
| 12 | Wall destroyed з mounted turret | Damage wall to 0 HP → wall destroyed, mounted turret also destroyed (cascade). |
| 13 | Pathfinding no-path case | Try drag-build through closed-off area (no valid corridor) → server returns empty path, HUD red "No path possible". |
| 14 | Adjacent walls own-team-only | Wall placed near enemy wall — own team's wall bitfield не treats enemy as neighbor (visual stays End). |
| 15 | Cell occupancy after destroy | Wall destroyed → cell freed → new building can be dropped там. |

### Out of MVP (Wall + Grid-specific)

- Wall rotation handles (player-rotated wall segments).
- Multi-tier wall types (light / heavy / reinforced).
- Wall upgrades (in-place).
- Wall gates (passable wall cell).
- Diagonal wall segments (only orthogonal connect у MVP).
- Wall-mounted variants beyond Turret (e.g., wall-mounted sensor, wall-mounted scanner).
- Player-team allied wall sharing (no allies у MVP).
- Wall as cover для unit positioning bonus (combat modifier — deferred).
- Grid editor utility (manual grid override per map) — DA-only for now.

## Feel / MVP Fun Maximization — Wall + Grid (Validator Pass)

Wall drag-build must feel **responsive і satisfying** per 5-component rubric:

**Drag-build response (Response + Clarity):**
- Server preview RPC at 4-8 Hz: ghost path updates with mouse — under 250 ms perceived lag (acceptable per RTS standard).
- Each path cell ghost: spawn-anim 100 ms (small "appearing" effect), не teleport.
- Path tail (cursor end): pulse highlight — player knows where wall WILL end if released.
- Invalid cells у path: red highlight + cursor reticle = stop-symbol.
- "No path" case (blocked all routes): full red strikethrough across drag line + "blocked" SFX. Player знає причину.

**Commit cascade (Satisfaction):**
- LMB release on valid path: walls drop sequentially 0.2 s stagger.
- Per-wall pod descent + small impact VFX.
- Audio: rhythmic "thunk-thunk-thunk" as walls land.
- Auto-connect bitfield updates після кожного wall lands → visible reshape (corner → straight → T-junction).
- Final wall lands: soft "completion" chime.

**Auto-connect visual feedback (Clarity + Fit):**
- New wall placed adjacent to existing → connection point flashes briefly (welding-spark VFX 200 ms).
- Existing walls' bitfield updates → their mesh swaps with 50 ms cross-fade transition.
- Player **sees** the perimeter forming, не just appearing.

**Wall-mounted Turret placement (Satisfaction):**
- Drop reticle on wall cell: cell highlights with "mount-able" glow.
- Drop pod descent over wall: smaller pod variant, lands on top of wall mesh.
- Turret unfolds animation 1 s after landing — industrial deployment feel.

**Clearance zone visualization (Clarity):**
- During drop-targeting mode: existing structures' clearance zones (2 cells radius) shown as orange-tinted overlay.
- Wall-vs-wall: no orange (walls touch each other freely).
- Wall-vs-other: orange overlay clearly marks "forbidden" cells.
- Player **never wonders** why placement rejected.

**Pillar 8 Re-Check (Wall):**
- 1-2 sentence: "Drag from A to B, walls auto-route around buildings + auto-connect 8-dir. Mount turrets on walls."
- Fun у v1: confirmed via drag-build flow + cascade landing + auto-connect reshape.
- New decision: perimeter design + chokepoint placement + turret coverage planning.
- Cheap: tilemap neighbor lookup + bounded A* + standard drop pod reuse.
- Scales via content: wall variants (heavy / light) via DA post-MVP.

## Sell + Demolish System (GP-0307)

Per [`../GDD/05_Buildings`](../GDD/05_Buildings.md) §Sell + Demolish і memory rule `project_sell_demolish`. Two distinct operations:

### Operation Matrix

| Op | Applies to | Refund | UX entry |
| --- | --- | --- | --- |
| **Sell** | Buildings with `bSellable=true` (Logistics Hub, Defensive Turret, Wall-mounted Turret) | `Cost × SellRefundRate × HealthFraction` (DA-driven) | `WBP_GP_HUD_SelectionPanel` Sell button |
| **Demolish** | Walls only (`AGP_Wall`) | **0** (permanent) | CommandBar Demolish button + cursor mode |

### UGP_BuildingDefinition Update

```cpp
UPROPERTY(EditAnywhere, Category = "GP|Economy")
bool bSellable = true;            // false для MainBase / FerroniteDeposit / Wall

UPROPERTY(EditAnywhere, Category = "GP|Economy", meta=(EditCondition="bSellable"))
float SellRefundRate = 0.5f;      // base refund multiplier
```

DA defaults per building:
- `DA_GP_Building_MainBase` — `bSellable=false`.
- `DA_GP_Building_LogisticsHub` — `bSellable=true`, `SellRefundRate=0.5`.
- `DA_GP_Building_DefensiveTurret` — `bSellable=true`, `SellRefundRate=0.5`.
- `DA_GP_Building_WallTurret` — `bSellable=true`, `SellRefundRate=0.5`.
- `DA_GP_Building_Wall` — `bSellable=false`.
- `DA_GP_Building_FerroniteDeposit` — `bSellable=false`.

### Sell Flow (Server-Authoritative)

```cpp
UFUNCTION(Server, Reliable, WithValidation)
void Server_SellBuilding(AGP_BuildingBase* Building);
```

```
Validate:
  1. Caller owns Building.
  2. Building.Definition.bSellable == true.
  3. Building.IsAlive() (Health > 0 — can't sell dead).
  4. Server-side anti-spam debounce 50 ms per caller (same pattern as command RPCs).

Compute refund:
  Cost = Building.OrbitalDropDef.Cost (cached on AGP_BuildingBase at spawn)
  HealthFrac = clamp(Building.GetHealth() / Building.GetMaxHealth(), 0, 1)
  Refund = Cost * Building.Definition.SellRefundRate * HealthFrac

Apply:
  GE_GP_RefundOrbital(Refund) on caller PlayerState.ASC
  Building_OnDestroyedBySell:
    - Cascade GE removal (e.g., GE_GP_UnitCap_Plus5 if Logistics Hub).
    - Container cap recompute on owning MainBase.
    - Multicast_PlaySellVFX (salvage sparks / crane lift / fade).
    - Building.Destroy() (delay 0.5 s for VFX).
  HUD on caller: +N flash on OrbitalFerronite + "Building sold" toast.
```

`Building.OrbitalDropDef` — cached `TWeakObjectPtr<UGP_OrbitalDropDefinition>` on building at spawn (set by `UGP_OrbitalDeliverySubsystem::SpawnPayload`). Required для cost lookup at sell time.

### Demolish Flow (Server-Authoritative)

```cpp
UFUNCTION(Server, Reliable, WithValidation)
void Server_DemolishWalls(const TArray<TWeakObjectPtr<AGP_Wall>>& Walls);
```

```
For each Wall у Walls (server, single tick):
  Validate:
    - Wall.IsValid() (not already destroyed).
    - Wall.OwnerTeamId == Caller.TeamId.
    - Wall.Definition.UnitTags.HasTagExact(GP.Building.Type.Wall).
    - Anti-spam: aggregate count <= 200 walls per RPC.

  Per validated wall:
    - Cascade destroy any wall-mounted Turret (uses standard building destroy path — no refund).
    - Trigger 8-neighbor OnNeighborChanged on adjacent walls (bitfield update).
    - Multicast_PlayDemolishVFX (collapse VFX, dust burst).
    - Wall.Destroy() (delay 0.3 s for VFX).
    - BuildGrid.UnregisterFootprint(Wall) — cell freed.

No refund. No GE applied.
```

### Demolish Cursor Mode (Client)

UI mode у HUD — activatable per Common UI per [`12_UI_Architecture`](12_UI_Architecture.md):

```
Entry:
  - CommandBar Demolish button (when wall selected) OR `Del` / `X` hotkey OR command-bar global "Demolish Mode".
  - HUD overlay: persistent banner "DEMOLISH MODE — click walls to demolish, Esc to exit".
  - Cursor texture override (red demolition icon).
  - `IMC_GP_Commands` partially suspended; new `IMC_GP_Demolish` activated.

While active:
  - On cursor hover over wall cell:
    - Tint cell red.
    - Tooltip "Demolish wall".
  - On cursor over non-wall cell:
    - Neutral cursor, no highlight.
  - LMB-press → record start cell, enter drag mode.
  - During drag:
    - Each cell under cursor: mark for demolish (visual: red overlay accumulates).
    - Local-only — server sees only on release.
  - LMB-release:
    - Collect marked walls (TArray).
    - If > 0 → Server_DemolishWalls(walls).
    - Otherwise → no-op.
  - Single click (no drag): equivalent to LMB-press-release on one cell.
  - RMB / Esc → exit mode, clear marks, no demolish.

Exit:
  - Cursor restored.
  - Banner hidden.
  - IMC restored.
```

### ViewModel + UI

`UGP_SelectionVM` — add field:

```cpp
UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
float SellPreviewAmount;          // computed client-side from cached Definition + Health
```

Adapter recomputes on selection / health change. Widget shows preview on Sell button hover.

`WBP_GP_HUD_SelectionPanel` building mode adds Sell button row. Disabled if `bSellable == false` with tooltip explaining ("MainBase cannot be sold", "Walls must be demolished", etc.).

`WBP_GP_HUD_CommandBar` adds Demolish button when wall selected.

### Tag Surface

| Tag | Purpose |
| --- | --- |
| `GP.Command.Sell` | Command intent (sell building). |
| `GP.Command.Demolish` | Command intent (enter demolish mode). |
| `GP.UI.Mode.Demolish` | UI state tag — demolish mode active. |
| `GP.Building.Capability.Sellable` | DataAsset-derived capability tag. |
| `GP.Notify.BuildingSold` | Notification toast. |
| `GP.Notify.WallDemolished` | Notification toast. |

### GameplayEffects

- `GE_GP_RefundOrbital` (Instant) — `OrbitalFerronite += SetByCaller(Magnitude)`. Re-used з orbital flow.

No new effects needed for demolish (no refund).

### Replication

- Sell: standard actor destruction replicates. Refund GE applied server-side, attribute replicates per existing rules.
- Demolish: same — actor destroys replicate. Adjacent walls' bitfield updates via `OnRep_ConnectionBitfield`.
- No special replication challenges.

### Anti-Patterns (Sell/Demolish-Specific)

- ❌ Client computes refund (must be server-side).
- ❌ Refund > Cost (sets `SellRefundRate <= 1.0` enforced via `IsDataValid`).
- ❌ Sell while construction phase active (irrelevant у orbital model, but guard for wall-mounted Turret descending).
- ❌ Wall refund on demolish.
- ❌ MainBase becomes sellable у future PR без explicit ADR.

### Feel / MVP Fun Maximization

**Sell — sense of recovery (Satisfaction):**
- Sell click → salvage VFX (sparks / crane-lift / particle burst over building).
- Building mesh fades out 0.5 s з emissive collapse.
- Audio: "credits-in" tone (warm, satisfying) + structural fail thud.
- HUD: `OrbitalFerronite +N` flash, "Building sold for X" toast.
- Player feels "I recovered something" rather than "I lost it".

**Demolish — sense of commitment (Fit + Clarity):**
- Demolish mode entry: subtle "tools out" UI overlay transition (warning-red border on HUD edges).
- Cursor change: demolition hammer / red X (visceral).
- Per-cell hover: red highlight + cell shake animation.
- LMB-drag: cascading red marks (visual feedback player знає що selected).
- LMB-release: walls collapse sequentially з 0.15 s stagger, dust + debris VFX.
- Audio: structural fail sounds, "thud" cascade. No celebratory chime — це commitment, не reward.
- No HUD reward flash — emphasizes "this is gone".

**Pillar 8 5-Question Gate:**
- 1-2 sentence: "Sell building for partial refund via button; demolish walls permanently via separate cursor mode."
- Fun у v1: yes — sell gives relief, demolish gives strategic flow.
- New decision: when to sell vs let die; when to demolish wall to free chokepoint.
- Cheap: 2 RPCs, 1 GE reuse, 2 tags, 1 UI mode.
- Scales: `SellRefundRate` per-building DataAsset; future wall-blueprint feature builds on demolish base.

Passes.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Sell undamaged Defensive Turret | Refund == Cost × 0.5 × 1.0 = Cost × 0.5. HUD flash. |
| 2 | Sell heavily damaged Turret (20% HP) | Refund == Cost × 0.5 × 0.2 = Cost × 0.1. Much less. |
| 3 | Sell MainBase | Button disabled, tooltip "Cannot sell MainBase". |
| 4 | Sell Wall | Button hidden / not on wall mode. |
| 5 | Sell Logistics Hub | Refund applied, +5 MaxUnits removed, container cap shrinks. |
| 6 | Sell Wall-mounted Turret | Turret sold, wall stays unchanged. |
| 7 | Demolish single wall | Wall destroyed. Adjacent walls reshape (auto-connect bitfield updates). |
| 8 | Demolish drag-batch (5 walls) | All 5 walls destroyed sequentially з cascade VFX. |
| 9 | Demolish wall з mounted turret | Wall destroyed, turret cascaded (no refund per cascade). |
| 10 | Demolish enemy wall | Server reject — not own team. |
| 11 | Exit demolish mode без actions | No state change. |
| 12 | Sell during combat | Allowed у MVP. (Decision: defer combat-lockout to playtest.) |
| 13 | Drag-demolish across NOT-walls | Non-wall cells ignored. Only walls in drag get destroyed. |
| 14 | Spam-sell RPC | 50 ms server debounce — second RPC rejected silently. |

### Out of MVP (Sell/Demolish-Specific)

- Move building (relocate without sell+rebuy) — friction-reducer, post-MVP.
- Batch sell (sell all of selected type).
- Combat-lockout (no sell during nearby enemy aggro). Defer to playtest.
- Salvage bonus при near-death sell (lose-less mechanic, design lever, post-MVP).
- Wall blueprint save/redeploy.
- Refund-on-cancel-drop-mid-flight (already partial-MVP at TDD/14 OpenQuestions).

### References (Sell + Demolish)

- GDD spec — [`../GDD/05_Buildings`](../GDD/05_Buildings.md) §Sell + Demolish.
- Memory rule — `project_sell_demolish`.
- Task — [`../Development/Claude_Tasks/GP-0307_Sell_Demolish.md`](../Development/Claude_Tasks/GP-0307_Sell_Demolish.md).

## Wall System References

- Wall GDD spec — [`../GDD/05_Buildings`](../GDD/05_Buildings.md) §Wall.
- Build Grid GDD — [`../GDD/05_Buildings`](../GDD/05_Buildings.md) §Build Grid System.
- Orbital drop integration — [`14_Orbital_Delivery`](14_Orbital_Delivery.md) (validation grid-aware).
- Wall task — [`../Development/Claude_Tasks/GP-0305_Wall.md`](../Development/Claude_Tasks/GP-0305_Wall.md).
