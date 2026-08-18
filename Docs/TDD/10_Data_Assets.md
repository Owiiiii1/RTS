# Data Assets

## Scope

Цей документ описує Data Asset ownership для MVP. Data Assets є primary tuning surface для gameplay content, але не містять executable gameplay logic.

Per ADR-0002 (Data-Driven First) і Pillar 9 (Technical Pillar — Server-Authoritative, Data-Driven, GAS-First).

## Mandatory Rules

- Balance values не hardcode-яться у C++.
- Data Asset містить immutable config, не runtime state.
- Runtime state живе у replicated actors/components, GAS Attributes або replicated structs.
- Кожний gameplay object має owner Data Asset, якщо має tuning, cost, tags або UI metadata.
- Data Assets зберігаються у `/Game/GrimProtocol/DataAssets/<Category>/`.
- Naming per `/STYLE.md`: `DA_GP_<Category>_<Specifier>`.
- **Усі content references — soft.** `TSoftObjectPtr<...>` / `TSoftClassPtr<...>` для DataAsset, ability, effect, mesh, material, texture, sound, niagara, animation, level. Hard refs до gameplay content — review-blocking (per [`ADR-0002`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md)).
- Loading **тільки через `UAssetManager`** (`LoadPrimaryAsset` / `RequestAsyncLoad`). Жодних `LoadObject` / `StaticLoadObject` / `ConstructorHelpers::FObjectFinder` для content.

## Asset Manager Loading Flow

Native owner: `UGP_MatchAssetLoader` (`UGameInstanceSubsystem`, spawned at game start). Responsibilities:

1. **Preload during `LoadingMatch` state** (per GP-0501):
   - Resolve `Faction.AllowedUnits` / `AllowedBuildings` (TSoftObjectPtr lists).
   - Resolve granted ability classes / effect classes referenced by those definitions.
   - Resolve mesh / icon / VFX / SFX bundles referenced by definitions (`UGP_FeedbackBundle` per system).
   - Issue single `RequestAsyncLoad` із combined paths. Cache returned `TSharedPtr<FStreamableHandle>` у loader.
2. **Match running:** all referenced content is in memory. Spawning units / placing buildings resolves classes/assets synchronously from already-loaded set.
3. **Late demand:** if post-MVP feature вимагає async asset (e.g., unlockable ability), `MatchAssetLoader::LoadOnDemand(SoftRef, Callback)` — caches handle for match duration.
4. **Match end / map travel:** `ReleaseAll()` clears all FStreamableHandles. Garbage collected on next GC pass.
5. **Failure:** if any required asset fails async load → `OnLoadFailed` delegate → `AGP_GameMode::AbortMatch` з error reason.

### Loader API (Design Contract)

```cpp
UCLASS()
class GPRUNTIME_API UGP_MatchAssetLoader : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    /** Preload everything player A and player B factions need. Called by GameMode pre-match. */
    void PreloadForMatch(
        TSoftObjectPtr<UGP_FactionDefinition> FactionA,
        TSoftObjectPtr<UGP_FactionDefinition> FactionB,
        TSoftObjectPtr<UWorld> Map,
        FOnPreloadComplete OnComplete);

    /** Resolve a soft ref synchronously; assumes content is loaded. Asserts otherwise. */
    template<typename T>
    T* Resolve(const TSoftObjectPtr<T>& Ref) const;

    template<typename T>
    TSubclassOf<T> ResolveClass(const TSoftClassPtr<T>& Ref) const;

    /** On-demand single load for post-MVP content. */
    void LoadOnDemand(const TArray<FSoftObjectPath>& Paths, FStreamableDelegate OnDone);

    /** Release all cached handles. Called by GameMode on EndPlay / map travel. */
    void ReleaseAll();
};
```

### Anti-Patterns (Review-Blocking)

- ❌ `TObjectPtr<U..._Definition>` у DataAsset field.
- ❌ `TSubclassOf<UGameplayAbility>` у DataAsset field (use `TSoftClassPtr`).
- ❌ `LoadObject<...>` / `StaticLoadObject` у gameplay path.
- ❌ `ConstructorHelpers::FObjectFinder` для content (engine-internal hard ref).
- ❌ Direct content load у actor `BeginPlay` без caching (re-loads on respawn).
- ❌ Caching loaded `UObject*` у persistent UPROPERTY без `Transient` flag (replication / serialization noise).

### Allowed Forms

- Engine base classes (e.g., `AActor`, `UPrimaryDataAsset`, `UAbilitySystemComponent`) — hard refs OK.
- `Transient` `TObjectPtr<...>` for runtime-resolved cached pointer (set after async load).
- `TSubclassOf<...>` у RPC payload / runtime parameter (after class is loaded by MatchAssetLoader).

## MVP Asset Types

| Asset Type | Prefix | Owner Module | Purpose |
| --- | --- | --- | --- |
| Orbital Drop Definition | `DA_GP_OrbitalDrop_*` | `GPRuntime` | **Building acquisition.** `Cost`, `DropTags`, soft `BuildingDefinition`, `DeliveryDescentSeconds`, `PayloadDeployDelaySeconds`. |
| Orbital Unit Drop Definition | `DA_GP_OrbitalUnitDrop_*` | `GPRuntime` | **Unit acquisition.** `Cost`, `TransportSlotCost`, soft `UnitDefinition` / `PayloadClass`, unit descent / deploy delay. PrimaryAssetType `GPOrbitalUnitDropDefinition`. |
| Unit Definition | `DA_GP_Unit_*` | `GPRuntime` | Intrinsic unit/building gameplay + `CargoCapacity`. **No acquisition cost.** |
| Building Definition | `DA_GP_Building_*` | `GPRuntime` | Identity, tags, `SpawnedClass`, footprint, `UnitDefinition`, **storage**, **`UnitCapBonus`**. **No acquisition cost.** |
| Resource Definition | `DA_GP_Resource_*` | `GPRuntime` | Ferronite mining / conversion / threat **+ `DepositMaxAmount` / `MaxConcurrentMiners`**. |
| Session Config | `DA_GP_Session_*` | `GPRuntime` | Match-level tuning: delivery quota, match duration, win flags, SWARM threat→wave curves. |
| Faction Definition | `DA_GP_Faction_*` | `GPRuntime` | Starting units/buildings, visual team identity, allowed orbital drops, faction tags. |
| SWARM Definition | `DA_GP_Swarm_*` | `GPRuntime` | SWARM unit identity, base stats, AI behavior reference. |
| Ability Definition | `DA_GP_Ability_*` | `GPGASRuntime` | Ability-facing config when C++ ability needs designer tuning. |
| AI Behavior Definition | `DA_GP_AIBehavior_*` | `GPRuntime` | AI opponent state machine thresholds — worker target, roster size, attack trigger. AI = `AGP_AIController : AAIController` (per `ADR-0008`). |

> **Acquisition model (orbital canon).** Cost of acquiring any unit/building = `UGP_OrbitalDropDefinition.Cost` (spent in **OrbitalFerronite**), NOT a local build/produce cost on the Unit/Building DA. Removed pre-pivot types: `UGP_ProductionComponent`, `UGP_ConstructionComponent`, `AGP_ConstructionSite`, `AGP_GhostBuilding`. Drops are ordered through the Logistics Hub / Order Menu via `UGP_OrbitalDeliverySubsystem`.

## First Playable Data (Canonical MVP Set)

Per [`../GDD/02_Core_Gameplay_Loop`](../GDD/02_Core_Gameplay_Loop.md) Data Asset Requirements table:

### Orbital Drops (Acquisition Catalog)

Per [`14_Orbital_Delivery`](14_Orbital_Delivery.md). Each `DA_GP_OrbitalDrop_*` is the **single source of acquisition cost** for its payload.

**Building-drop ownership (GP-S35B, resolved):**

| Asset | Owns |
| --- | --- |
| `UGP_BuildingDefinition` | Intrinsic identity/grid/payload + storage + `UnitCapBonus`. `MaxHealth` compatibility-only. |
| `UGP_UnitDefinition` | Canonical initial MaxHealth / combat / sight / facing / cargo (GP-S38D / GP-S39E). Runtime remains GAS. |
| `UGP_OrbitalDropDefinition` | Building acquisition: `Cost`, `DropTags`, soft `BuildingDefinition`, descent / deploy delay |
| `UGP_OrbitalUnitDropDefinition` | Unit acquisition: `Cost`, slots, payload, descent / deploy delay |
| `UGP_OrbitalDeliverySettings` | Global transport tunables + **soft refs only** to authored unit/building drop DataAssets. No balance values on those refs. |

Do **not** duplicate `SpawnedClass` / `FootprintCells` / MaxHealth onto the DropDef as a second SoT. Canonical MaxHealth after GP-S38D is `UnitDefinition.MaxHealth`. Per-purchase delivery timing lives on drop definitions; settings remain fallback. Spawn altitude / cleanup / placement radius stay global.

Building catalog names (content, later): `DA_GP_Building_LogisticsHub` / `DefensiveTurret` / `Wall` / `WallTurret` and matching `DA_GP_OrbitalDrop_*`. GP-S35B ships a native bootstrap catalog with those identities; authored `.uasset` files are not required for architecture validation.

- `DA_GP_OrbitalUnitDrop_Worker` — Cost 25, TransportSlotCost 1, descent 2.5, deploy 1.25. Native catalog; authored `.uasset` optional.
- `DA_GP_OrbitalUnitDrop_SalvageWalker` — Cost 50, TransportSlotCost 2, same unit delivery timing.
- `DA_GP_OrbitalDrop_LogisticsHub` — `DropTags: GP.Drop.Type.Building`; `Cost` from DropDef (operator Hub bridge may sync deprecated settings cost); BuildingDefinition `FootprintCells`: 4×4.
- `DA_GP_OrbitalDrop_DefensiveTurret` — `DropTags: GP.Drop.Type.Building`; identity only in GP-S35B (no turret combat).
- `DA_GP_OrbitalDrop_Wall` — `DropTags: GP.Drop.Type.Wall`; identity only (no wall gameplay / drag-build).
- `DA_GP_OrbitalDrop_WallTurret` — payload classified `GP.Drop.Type.Building`; identity only (no wall mounting).

`GP.Drop.Type.Module` — reserved post-MVP; no active `DA_GP_OrbitalDrop_*` ships in MVP.

### Factions

- `DA_GP_Faction_Default` — corporate colonial faction.
  - `DisplayName`: "Corporate Colonial" (placeholder; final naming TBD per `Lore_Setting`).
  - `StartingUnits`: `[DA_GP_Unit_Worker x2]`.
  - `StartingBuildings`: `[DA_GP_Building_MainBase x1]`.
  - `StartingOrbitalFerronite`: TBD (starting spendable balance; balance pass).
  - `AllowedUnits`: `[DA_GP_Unit_Worker, DA_GP_Unit_SalvageWalker]`.
  - `AllowedBuildings`: `[DA_GP_Building_MainBase, DA_GP_Building_LogisticsHub, DA_GP_Building_DefensiveTurret, DA_GP_Building_Wall, DA_GP_Building_WallTurret]`.
  - `AllowedOrbitalDrops`: `[DA_GP_OrbitalDrop_Worker, DA_GP_OrbitalDrop_SalvageWalker, DA_GP_OrbitalDrop_LogisticsHub, DA_GP_OrbitalDrop_DefensiveTurret, DA_GP_OrbitalDrop_Wall, DA_GP_OrbitalDrop_WallTurret]` — soft list; the acquisition catalog presented in the Order Menu.
  - `FactionTags`: `{GP.Faction.Corporate}`.

### Resource

- `DA_GP_Resource_Ferronite` — universal exchange currency metadata.
  - `DisplayName`: "Ferronite".
  - `ResourceTag`: `GP.Resource.Type.Ferronite`.
  - `ScoreConversionRate`: 1.0 (1 unit shipped → 1 unit `FerroniteScore` at orbit launch; drives delivery quota).
  - `ThreatPerStoredUnit`: TBD (balance pass; recommended starting 0.5). Multiplier from raw stored-at-base stock (`FerroniteThreatValue` on GameState) to SWARM pressure. **Replaces** the deprecated `AggressionPerUnitMined` / `AggressionPerUnitShipped` fields — threat is driven by stock currently stored at base, not by cumulative mined/shipped totals.
  - `MineRatePerWorker`: 10.0 units/sec.
  - `Tint`: teal-blue glow color.
  - `Icon`: `TSoftObjectPtr<UTexture2D>` for UI.

> **Deprecated (pre-pivot)** — do not author: `AggressionPerUnitMined`, `AggressionPerUnitShipped`. SWARM pressure is now keyed on `FerroniteThreatValue` (raw stock stored at base — up on Worker drop-off, down on orbital launch) via `ThreatPerStoredUnit` + the Session-level threat→wave curves.

### Session

- `DA_GP_Session_Default` — match-level tuning and win configuration (not authored yet).
  - GP-S34W hosts the same defaults on `AGP_GameMode` (`EditDefaultsOnly`) and replicates them on `AGP_GameState`: `DeliveryQuotaFerroniteScore` 5000, `bAnnihilationCountsAsWin` true, `MatchDurationSeconds` 600, plus a server `MatchSeed` for the final tie-break.
  - `DeliveryQuotaFerroniteScore`: 5000 (placeholder; primary win condition — first to `FerroniteScore >= quota` wins).
  - `bAnnihilationCountsAsWin`: true.
  - `MatchDurationSeconds`: 600 (timer; on expiry highest `FerroniteScore` wins — `GP.Match.WinReason.TimerScore`).
  - `ThreatToWaveSize`: `TSoftObjectPtr<UCurveFloat>` — keyed on `FerroniteThreatValue` → SWARM wave unit count.
  - `ThreatToWaveFrequency`: `TSoftObjectPtr<UCurveFloat>` — keyed on `FerroniteThreatValue` → seconds between waves.
  - Win reasons surfaced via `GP.Match.WinReason.*` (`DeliveryQuota`, `TimerScore`, `Annihilation`, `OpponentDisconnect`).

### Buildings

> Building Definitions carry **no local build/produce cost**. Acquisition cost lives on the matching `DA_GP_OrbitalDrop_*` (`Cost` in OrbitalFerronite). Building DAs hold identity, footprint, components, and gameplay-effect hooks only.

- `DA_GP_Building_MainBase` — landing capsule / command rig.
  - `Acquisition`: not orderable (starting building).
  - `FootprintCells`: 5×5 (per Build Grid spec).
  - `MaxHealth`: 1000.
  - `bSellable`: false (MainBase non-sellable).
  - `StorageComponent`: enabled (accept Ferronite drop-off → increases `FerroniteThreatValue`; orbital launch → `FerroniteScore`).
  - `DropOffRange`: 400 cm.
  - `UnitTags`: `{GP.Unit.Type.Building, GP.Building.Type.MainBase, GP.Building.Role.Command, GP.Faction.Corporate}`.

- `DA_GP_Building_LogisticsHub` — orbital order surface (replaces fabrication building).
  - `Acquisition`: via `DA_GP_OrbitalDrop_LogisticsHub` (Cost in OrbitalFerronite — TBD).
  - `FootprintCells`: 4×4 (per Build Grid spec).
  - `MaxHealth`: 500.
  - `GameplayEffects.OnLanded`: `[GE_GP_UnitCap_Plus5]` (Infinite duration, +5 MaxUnits).
  - `GameplayEffects.OnDestroyed`: remove `GE_GP_UnitCap_Plus5`.
  - `bSellable`: true (partial OrbitalFerronite refund per Sell formula).
  - `UnitTags`: `{GP.Unit.Type.Building, GP.Building.Type.LogisticsHub, GP.Building.Role.Logistics, GP.Faction.Corporate}`.

- `DA_GP_Building_DefensiveTurret` — perimeter defense node.
  - `Acquisition`: via `DA_GP_OrbitalDrop_DefensiveTurret` (Cost in OrbitalFerronite — TBD).
  - `FootprintCells`: 4×4 (per Build Grid spec).
  - `MaxHealth`: 300.
  - `AttackRange`: 900 cm.
  - `Damage`: 15.
  - `AttackSpeed`: 1.5 hit/sec.
  - `TargetingPriority`: SWARM > enemy units > enemy buildings.
  - `bSellable`: true.
  - `UnitTags`: `{GP.Unit.Type.Building, GP.Building.Type.DefensiveTurret, GP.Building.Role.Defense, GP.Faction.Corporate}`.

- `DA_GP_Building_Wall` — perimeter wall segment (2×2 cell, 8-dir auto-connect).
  - `Acquisition`: via `DA_GP_OrbitalDrop_Wall` (Cost in OrbitalFerronite — TBD; drag-build cost = segments × per-segment cost).
  - `FootprintCells`: 2×2.
  - `MaxHealth`: TBD.
  - `bSellable`: false — walls are **DEMOLISHED** permanently (no refund) via `GP.Command.Demolish`.
  - `UnitTags`: `{GP.Unit.Type.Building, GP.Building.Type.Wall, GP.Faction.Corporate}`.

- `DA_GP_Building_WallTurret` — wall-mounted turret variant (2×2, mounts on a Wall cell).
  - `Acquisition`: via `DA_GP_OrbitalDrop_WallTurret` (Cost in OrbitalFerronite — TBD).
  - `FootprintCells`: 2×2.
  - `bMountsOnWall`: true (one-mount-per-wall rule).
  - `MaxHealth`: TBD.
  - `Damage` / `AttackRange` / `AttackSpeed`: TBD.
  - `bSellable`: true.
  - `UnitTags`: `{GP.Unit.Type.Building, GP.Building.Type.WallTurret, GP.Building.Role.Defense, GP.Faction.Corporate}`.

- `DA_GP_Building_FerroniteDeposit` — environment resource node actor.
  - `Cost`: not buildable (map placement only).
  - `MaxCapacity`: 1000–2000 (per-instance per map placement).
  - `MineRatePerWorker`: 10.0 units/sec (узгоджено з `DA_GP_Resource_Ferronite.MineRatePerWorker`).
  - `MaxConcurrentWorkers`: 4.
  - `DepletedBehavior`: `Disable` (deposit залишається на map як empty husk).
  - `UnitTags`: `{GP.Resource.Node, GP.Resource.Type.Ferronite}`.
  - References `DA_GP_Resource_Ferronite` для type identity (anti-duplication).

### Units

> Unit Definitions carry **no local produce cost**. Acquisition cost lives on the matching `DA_GP_OrbitalDrop_*` (`Cost` in OrbitalFerronite). Unit DAs hold identity, stats, allowed commands, and granted abilities only.

- `DA_GP_Unit_Worker` — industrial mining drone.
  - `Acquisition`: via `DA_GP_OrbitalDrop_Worker` (Cost in OrbitalFerronite — TBD).
  - `MaxHealth`: 50.
  - `MoveSpeed`: 350 cm/s.
  - `CarryCapacity`: 50 Ferronite.
  - `MineRatePerSecond`: 10 (синхронізовано з deposit config).
  - `AllowedCommands`: `{GP.Command.Move, GP.Command.Stop, GP.Command.Mine, GP.Command.Repair}` (Repair ACTIVE у MVP).
  - `GrantedAbilities`: `{GP.Ability.Repair}`.
  - `UnitTags`: `{GP.Unit.Type.Worker, GP.Faction.Corporate}`.

- `DA_GP_Unit_SalvageWalker` — industrial defender (retrofitted mining rig з combat conversion).
  - `Acquisition`: via `DA_GP_OrbitalDrop_SalvageWalker` (Cost in OrbitalFerronite — TBD).
  - `MaxHealth`: 200.
  - `Armor`: 0.
  - `MoveSpeed`: 250 cm/s.
  - `Damage`: 20.
  - `AttackRange`: 600 cm.
  - `AttackSpeed`: 1.0 hit/sec.
  - `AllowedCommands`: `{GP.Command.Move, GP.Command.Stop, GP.Command.Attack, GP.Command.Patrol, GP.Command.AttackMove}`.
  - `UnitTags`: `{GP.Unit.Type.SalvageWalker, GP.Faction.Corporate}`.

### SWARM

- `DA_GP_Swarm_Grunt` — baseline SWARM unit (MVP only tier).
  - Не покупається, не виробляється; спавн через `AGP_GameMode::SpawnSwarmWave` server-side.
  - `MaxHealth`: TBD (recommended starting 30; SWARM trivially loses to Salvage Walker single attack).
  - `Damage`: TBD (recommended starting 5; lethal до worker за кілька ticks).
  - `AttackRange`: TBD (mix melee 150 і ranged 600 у наступних tiers; MVP — single tier).
  - `MoveSpeed`: TBD.
  - `AggroRadius`: TBD.
  - `SpawnWeight`: 1.0 (for wave composition rolling — MVP all-Grunt).
  - `UnitTags`: `{GP.Unit.Type.Swarm, GP.Faction.Swarm, GP.Mob.Tier.Grunt}`.

### AI Behavior

- `DA_GP_AIBehavior_Default` — primitive AI opponent thresholds. Consumed by `AGP_AIController : AAIController` (per [`ADR-0008`](../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md)).
  - `WorkerCountTarget`: 4.
  - `LogisticsHubCount`: 1 (ordered from orbit — replaces pre-pivot AssemblyYard count).
  - `DefensiveTurretCount`: 2.
  - `SalvageWalkerRosterTarget`: 5.
  - `AttackTriggerThreshold`: 5 (виходить на attack, коли roster >= 5).
  - `DecisionTickInterval`: 3.0 sec.
  - `States`: `{EarlyBuildUp, MidEconomy, RosterBuildUp, Attack, Defend}` (per [`../GDD/03_Factions`](../GDD/03_Factions.md)).

## Folder Convention

```
/Game/GrimProtocol/DataAssets/
  Factions/
    DA_GP_Faction_Default
  Resources/
    DA_GP_Resource_Ferronite
  Session/
    DA_GP_Session_Default
  OrbitalDrops/
    DA_GP_OrbitalDrop_Worker
    DA_GP_OrbitalDrop_SalvageWalker
    DA_GP_OrbitalDrop_LogisticsHub
    DA_GP_OrbitalDrop_DefensiveTurret
    DA_GP_OrbitalDrop_Wall
    DA_GP_OrbitalDrop_WallTurret
  Buildings/
    DA_GP_Building_MainBase
    DA_GP_Building_LogisticsHub
    DA_GP_Building_DefensiveTurret
    DA_GP_Building_Wall
    DA_GP_Building_WallTurret
    DA_GP_Building_FerroniteDeposit
  Units/
    DA_GP_Unit_Worker
    DA_GP_Unit_SalvageWalker
  Swarm/
    DA_GP_Swarm_Grunt
  AI/
    DA_GP_AIBehavior_Default
  Abilities/
    DA_GP_Ability_* (поки немає GA-driven abilities, що потребують designer tuning)
```

## Runtime Flow

```
Data Asset (immutable config)
  -> spawn/init actor / component
  -> grant tags / initial attributes / allowed commands / granted abilities
  -> server validates runtime changes (commands, costs, capacity)
  -> GAS applies Effects (income, cost, cap, score)
  -> replicated state updates clients
```

Data Asset — read-only after load. Runtime mutation — заборонено. Якщо потрібно tunable runtime — це state (replicated property або GAS Attribute), не Data Asset.

## Capacity Rule (Pillar 4)

Per [`../GDD/02_Core_Gameplay_Loop`](../GDD/02_Core_Gameplay_Loop.md) Cap Increase Rule. `MaxUnits` зростає виключно через `GE_GP_UnitCap_Plus5` applied on **Logistics Hub** landing (orbital drop complete):

- One Logistics Hub landed → +5 MaxUnits (Infinite GE).
- Logistics Hub destroyed → -5 MaxUnits (GE removed).
- No other mechanism для cap growth у MVP. Per A16 (Capacity-Ignoring Drift).

## Single Resource Rule (Pillar 3)

Per Cost rule у Loop Spec. Усі покупки — у **OrbitalFerronite** (spendable). Жодних alternate currencies, soft surrogates, research points, military credits. `FerroniteScore` (cumulative shipped victory score) і `FerroniteThreatValue` (raw stored-at-base stock, drives SWARM) — це окремі state surfaces, не spendable currencies. Planetary Ferronite — container state (not a player attribute). Multi-resource — review-blocking без pillar amendment. Per A15 (Multi-Resource Drift).

## Deprecated (Pre-Pivot) — Do Not Author

Ці DataAssets / fields належать pre-orbital-pivot моделі (local production/construction) і **не присутні в active catalog**. Залишені тут лише для traceability під час cleanup; не створювати у new content.

- `DA_GP_Building_AssemblyYard` — local Salvage Walker fabrication building. **Removed.** Units now arrive via `DA_GP_OrbitalDrop_SalvageWalker`; +5 MaxUnits moved to `DA_GP_Building_LogisticsHub` landing.
- `DA_GP_Building_Barracks` — local unit production building. **Removed.** No local production exists.
- Any `Cost` / `BuildTime` / `ProductionTime` / `AllowedProductions` field framed as a **local build/produce cost** on Unit or Building DAs. **Removed** — acquisition cost lives on `UGP_OrbitalDropDefinition.Cost` (OrbitalFerronite).
- `AggressionPerUnitMined` / `AggressionPerUnitShipped` on the Resource DA. **Removed** — replaced by `ThreatPerStoredUnit` keyed on `FerroniteThreatValue`.

## Validation Discipline

- Кожен новий Data Asset проходить feature validation через [`gp-mechanics-validator`](../../SKILLS/gp-mechanics-validator/SKILL.md) — 22 categories + 17 anti-patterns.
- Pillar Violation verdict — review-blocking.

## References

- Unit architecture — [`05_Unit_Architecture.md`](05_Unit_Architecture.md).
- Building architecture — [`06_Building_Architecture.md`](06_Building_Architecture.md).
- Resource architecture — [`07_Resource_Architecture.md`](07_Resource_Architecture.md).
- Gameplay Tags — [`09_Gameplay_Tags.md`](09_Gameplay_Tags.md).
- GAS attributes — [`02_GAS_Architecture.md`](02_GAS_Architecture.md).
- Core gameplay loop spec — [`../GDD/02_Core_Gameplay_Loop`](../GDD/02_Core_Gameplay_Loop.md).
- Pillars — [`../GDD/01_Game_Pillars`](../GDD/01_Game_Pillars.md).
- ADR-0002 Data-Driven First — [`../Architecture_Decisions/ADR_0002_Data_Driven_First`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md).
