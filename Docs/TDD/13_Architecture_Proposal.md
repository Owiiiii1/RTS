# Architecture Proposal

## Scope

Consolidated **implementation-time** proposal для GrimProtocol MVP — мінімальний C++ class list, responsibility breakdown, owner modules, RPC inventory, Data Assets, Tags, Attributes, risks, implementation order. Цей документ — single source перед початком C++ work (per [`Claude_Tasks/GP-0601_Architecture_Proposal`](../Development/Claude_Tasks/GP-0601_Architecture_Proposal.md)).

**Stop rule:** після цього документа Claude чекає human approval перед написанням C++ gameplay коду (per [`Claude_Task_Backlog`](../Development/Claude_Task_Backlog.md) Global Stop Rule).

Consolidates GP-0201..GP-0501. Не дублює detail — посилається на existing TDD sections.

## Module Plan

Three runtime modules — confirmed per [`01_Module_Architecture`](01_Module_Architecture.md) і [`ADR-0006`](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md).

```
GPUIRuntime  ─► GPRuntime  ─► GPGASRuntime
                  │                ▲
                  └────────────────┘
```

| Module | Owns |
| --- | --- |
| `GPGASRuntime` | ASC subclass, AttributeSets, GameplayAbilities, GameplayEffects, native Tag registry, `UGP_DamageCalculation`. |
| `GPRuntime` | GameMode/State, PlayerController/State, AIController, CameraPawn, Units, Buildings (as pawns), Components, Session subsystem, MatchAssetLoader, Commands, Spawning, Movement, Combat, Mining, Cargo, Targeting, `UGP_OrbitalDeliverySubsystem`, `UGP_BuildGridSubsystem`, `UGP_StorageComponent` (MainBase containers), resource/threat tracking on GameState. |
| `GPUIRuntime` | HUD widgets (Common UI), ViewModels (MVVM), VM Adapters, MinimapSubsystem, NotificationVM, Order Menu, EndOfMatch, Lobby UI. |

Module deps already declared у `*.Build.cs` (`GPRuntime` додає `CommonUI`/`ModelViewViewModel` reference тільки опосередковано — pure UI plugins linked у `GPUIRuntime`).

## Class List — Minimal MVP

### GPGASRuntime

| Class | Base | Replication | Purpose |
| --- | --- | --- | --- |
| `UGP_AbilitySystemComponent` | `UAbilitySystemComponent` | Engine | Project-wide ASC subclass. Override `Mixed` mode per actor at init. |
| `UGP_PlayerAttributeSet` | `UAttributeSet` | OwnerOnly + All-clients для Score | OrbitalFerronite (COND_OwnerOnly), FerroniteScore (COND_None), MaxUnits (COND_OwnerOnly), CurrentUnits (COND_OwnerOnly). |
| `UGP_UnitAttributeSet` | `UAttributeSet` | Mixed | Health, MaxHealth, Armor, DamageResistance, AttackCooldown. |
| `UGP_DamageCalculation` | `UGameplayModMagnitudeCalculation` | — | Source.Damage − target Armor / DamageResistance multiplier. |
| `FGPGameplayTags` | static struct | — | Native tag registry, single source of `GP.*` tags. |

GameplayEffects + Abilities — `.uasset` (BP), не C++ class explosion. C++ provides bases:

- `UGP_GA_ActorTargeted` — base ability that reads target from `FGameplayEventData.Target`.
- `UGP_GA_LocationTargeted` — base ability що читає FVector з event.

Concrete abilities у Content (`/Game/GrimProtocol/Abilities/GA_GP_Repair`, `GA_GP_Attack` if upgraded post-MVP). Worker repair STAYS in MVP (`GP.Command.Repair`); Worker has NO Build ability. Units and READY buildings arrive via orbital DropPod. Wall **material** arrives as a Wall Package DropPod to MainBase; `AGP_Wall` segments are placed from inventory (not Worker-built, not per-segment pods).

### GPRuntime — Match Flow

| Class | Base | Replication | Purpose |
| --- | --- | --- | --- |
| `AGP_GameMode` | `AGameModeBase` | Server only | Lobby start, ServerTravel, PostLogin, OnUnitDied, MatchAssetLoader gate, EndMatch. |
| `AGP_GameState` | `AGameStateBase` | All | MatchState (tag), MatchTimeRemaining, FerroniteThreatValue, DeliveryQuotaFerroniteScore, bAnnihilationCountsAsWin, MatchSeed, `FGP_MatchResult` (WinnerTeamId, WinnerReason, MatchDuration, FinalScores array) plus compatibility WinnerTeamId / WinReasonTag getters. |
| `AGP_LobbyState` | `AInfo` | All | Players[], bAllReady. Lifecycle: spawn before lobby map travel. |
| `AGP_PlayerController` | `APlayerController` | OwnerOnly state | Input orchestrator, hosts SelectionComponent + CommandComponent + PlayerUIComponent. Owns VM adapters. |
| `AGP_AIController` | `AAIController` (decided per ADR-0008) | Server-only | Singleplayer opponent. Has a normal `AGP_PlayerState` with ASC. State machine (`Explore / Mine / Ship / Order / Defend`), decision tick 2-5 s, reads own `OrbitalFerronite` / `FerroniteScore` / map state. Does NOT use the client `Server_RequestCommand` RPC — invokes server-side command-execution helpers DIRECTLY. **MVP feature.** |
| `AGP_PlayerState` | `APlayerState` | All (per-attribute conditions) | Owns ASC + `UGP_PlayerAttributeSet`. TeamId, FactionRef (soft). |
| `UGP_SessionSubsystem` | `UGameInstanceSubsystem` | — | Steam Online Session API wrapper. Single-subsystem rule (ADR-0006). |
| `UGP_MatchAssetLoader` | `UGameInstanceSubsystem` | — | Asset Manager async preload, soft-ref resolution (per ADR-0002 update). |
| `UGP_BuildGridSubsystem` | `UWorldSubsystem` | Server (state) + client mirror | Grid cell occupancy, footprint validation, A* pathfinding for wall drag-build. Cell size DA-driven. |
| `UGP_OrbitalDeliverySubsystem` | `UWorldSubsystem` | Server-only | Drop order queue, drop validation (grid-aware), `AGP_DropPod` lifecycle + payload spawn. Owns **unit**, **READY building**, and **Wall Package** delivery. Does **not** own per-segment `AGP_Wall` placement (GP-S42C / BuildGrid). |

### GPRuntime — Player Camera

| Class | Base | Replication | Purpose |
| --- | --- | --- | --- |
| `AGP_CameraPawn` | `APawn` | None (`bReplicates=false`) | Local pan/zoom/rotate/edge-scroll. Reads `TSoftObjectPtr<UGP_CameraConfigDataAsset>`. |
| `AGP_CameraBoundsVolume` | `AActor` | None | Optional level-placed box for map bounds. |

(per [`11_RTS_Camera`](11_RTS_Camera.md))

### GPRuntime — Units

| Class | Base | Replication | Purpose |
| --- | --- | --- | --- |
| `AGP_UnitBase` (abstract) | `APawn` | Yes | Common ancestor: ASC, UnitAttributeSet, TeamId, OwningPlayerState (weak), UnitTags, UnitDefinition soft ref, OnDeath delegate, MID highlight, ReceiveCommand routing. |
| `AGP_MobileUnit` | `AGP_UnitBase` | Yes | MovementComponent base, AttackMoveDestination state. |
| `AGP_Worker` | `AGP_MobileUnit` | Yes | Adds `UGP_MiningComponent`, `UGP_CargoComponent`. `bAutoAttacks=false`. |
| `AGP_CombatUnit` (post-MVP if needed) | `AGP_MobileUnit` | Yes | Adds `UGP_CombatComponent`, `UGP_TargetingComponent`. У MVP — BP child `BP_GP_SalvageWalker`. |

Composition default — BP children add components. Optional data-driven composition `ComponentsToAdd : TArray<TSoftClassPtr<UActorComponent>>` deferred.

### GPRuntime — Buildings & Orbital Delivery

Buildings are pawns (static units). No building owns a Production or Construction component — there is no local production or local Worker construction. Soft refs only.

**Orbital vs surface Wall (GP-0305R):**
- Normal units and READY buildings keep their existing orbital paths (`UGP_OrbitalDeliverySubsystem` + `AGP_DropPod`).
- Wall **Package** is delivered from orbit to MainBase (one DropPod; subsystem-owned).
- `AGP_Wall` segments are **not** individually delivered by DropPod. They are instantiated on the surface from authoritative MainBase WallSegment inventory by **Build Wall**.
- GP-S42C owns/routes surface wall placement through the wall / BuildGrid authority path.

| Class | Base | Replication | Purpose |
| --- | --- | --- | --- |
| `AGP_BuildingBase` (abstract) | `AGP_UnitBase` | Yes | Static unit ancestor (a pawn). Owns optional `UGP_StorageComponent` based on Definition. No Production/Construction component. |
| `AGP_MainBase` | `AGP_BuildingBase` | Yes | Initial base. Hosts `UGP_StorageComponent` (Ferronite) and `UGP_WallSegmentInventoryComponent` (Wall stock 0..5 + depot presentation). Drop-zone marker. Non-sellable. |
| `AGP_LogisticsHub` | `AGP_BuildingBase` | Yes | Orbital order point. Surfaces Order Menu, applies `GE_GP_UnitCap_Plus5` + storage-cap bonus while alive. Orbital drops only — no production. |
| `AGP_DropPod` | `APawn` | Yes | Orbital descent vehicle. Spawned by `UGP_OrbitalDeliverySubsystem` on accepted order; descends to grid-validated location or MainBase (units / READY buildings / Wall Package). Carries `GP.State.PodInFlight`. |
| `AGP_FerroniteDeposit` | `AGP_BuildingBase` | Yes | Resource node, CurrentCapacity, ActiveMiners / WaitingMiners queue. `TeamId=0`, `bDamageable=false`. 3×3 footprint. |
| `AGP_Wall` | `AGP_BuildingBase` | Yes | Defensive wall segment, 2×2 footprint. Hosts `UGP_WallConnectionComponent` для 8-dir auto-connect. Mountable surface для `WallTurret`. Instantiated from MainBase Wall inventory by Build Wall — **not** a DropPod payload. |
| `AGP_WallTurret` | `AGP_BuildingBase` (or BP variant of `AGP_DefensiveTurret`) | Yes | Constrained 2×2 turret mounted on `AGP_Wall`. Lower HP / shorter range than 4×4 free-standing Turret. |
| `AGP_DropReticle` | `AActor` | None | Local-only drop-targeting preview (free-standing drop targets). Material parameter tint. Grid-snapped. |
| `AGP_GhostWallSegment` | `AActor` | None | Local-only wall drag-build preview (ghosts along A* path; length limited by Wall inventory). |
| `AGP_CinematicCameraPawn` (reserved name) | `APawn` | None | Reserved for post-MVP cinematic / end-of-match. Не implementuet у MVP. |

### GPRuntime — Components

| Class | Owner | Server-only tick? | Purpose |
| --- | --- | --- | --- |
| `UGP_SelectionComponent` | `AGP_PlayerController` | Local-only | `SelectedUnits`, `InspectedTarget`, control groups, marquee, smart selection. |
| `UGP_CommandComponent` | `AGP_PlayerController` | Local | Smart command resolution, sends `Server_RequestCommand`. |
| `UGP_PlayerUIComponent` | `AGP_PlayerController` | Local | Owns VM adapters lifecycle, HUD root spawn. |
| `UGP_MovementComponent` | `AGP_MobileUnit` | Server | Pawn movement component + NavMesh path. |
| `UGP_MiningComponent` | `AGP_Worker` | Server | Mining state machine (per GP-0302). |
| `UGP_CargoComponent` | `AGP_Worker` | Server (replicates state) | Cargo bar. |
| `UGP_CombatComponent` | combat units | Server | Engage, LOS multi-trace, fire damage. |
| `UGP_TargetingComponent` | combat units | Server | Auto-acquire scan. |
| `UGP_StorageComponent` | `AGP_MainBase` (containers) | Server | Holds Planetary Ferronite (raw, not spendable). Worker drop-off = Storage state mutation (NOT a player-attribute GE) and raises GameState `FerroniteThreatValue`. Container launch applies `GE_GP_AddOrbital` (+OrbitalFerronite) and `GE_GP_AddScore` (+FerroniteScore) and lowers `FerroniteThreatValue`. |
| `UGP_WallConnectionComponent` | `AGP_Wall` | Server (replicated bitfield) | 8-neighbor query, ConnectionBitfield replication, visual state mapping. |
| `UGP_WallSegmentInventoryComponent` | `AGP_MainBase` | Server (replicated count) | Wall stock 0..5, package-pending, `WallInventoryChanged`. Not Ferronite storage. |

Жодних extra components у MVP. Якщо новий потрібен — окремий ticket з justification per ADR-0006.

### GPUIRuntime — UI

| Class | Base | Purpose |
| --- | --- | --- |
| `UGP_UserWidgetBase` | `UCommonUserWidget` | Project-wide widget base. |
| `UGP_HUDWidget` | `UGP_UserWidgetBase` | HUD root (BP child `WBP_GP_HUD_Match`). |
| `UGP_ActivatableScreenBase` | `UCommonActivatableWidget` | Base для modal screens (Order Menu, EndOfMatch, Pause, Lobby). |
| `UGP_ResourceVM` | `UMVVMViewModelBase` | OrbitalFerronite / FerroniteScore / Cap mirror. |
| `UGP_MatchVM` | `UMVVMViewModelBase` | MatchState / Timer / FerroniteThreatValue / Winner / WinReason. |
| `UGP_SelectionVM` | `UMVVMViewModelBase` | Mode, selected unit / building VMs, inspected target. |
| `UGP_OrderMenuVM` | `UMVVMViewModelBase` | Available orbital drops (`UGP_OrbitalDropDefinition`), can-afford flags (vs OrbitalFerronite). |
| `UGP_StorageVM` | `UMVVMViewModelBase` | MainBase container fill (Planetary Ferronite stored), launch-ready state. |
| `UGP_CargoVM` | `UMVVMViewModelBase` | Selected single Worker cargo bar. |
| `UGP_NotificationVM` | `UMVVMViewModelBase` | Active toast queue. |
| `UGP_MinimapVM` | `UMVVMViewModelBase` | Minimap snapshot blips. |
| `UGP_LobbyVM` | `UMVVMViewModelBase` | Lobby players, ready states, can-start. |
| `UGP_*VMAdapter` (one per VM) | `UObject` (subobject of PC) | Subscribes to source delegates, mirrors into VM. |
| `UGP_MinimapSubsystem` | `UWorldSubsystem` | 5 Hz snapshot of actors → blip list. |

(per [`12_UI_Architecture`](12_UI_Architecture.md))

## Data Assets Inventory

Per [`10_Data_Assets`](10_Data_Assets.md) + [`ADR-0002`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md) (soft refs only).

| Class | Purpose | Module |
| --- | --- | --- |
| `UGP_UnitDefinition` | Worker / SalvageWalker tunable | GPRuntime |
| `UGP_BuildingDefinition` | MainBase / LogisticsHub / DefensiveTurret / Wall / WallTurret / FerroniteDeposit | GPRuntime |
| `UGP_ResourceDefinition` | Ferronite metadata (two-state: Planetary stored vs Orbital spendable, conversion rates) | GPRuntime |
| `UGP_FactionDefinition` | StartingUnits, AllowedUnits, AllowedBuildings (all soft) | GPRuntime |
| `UGP_AbilityDefinition` (optional) | Designer tuning shell for abilities (cost, cooldown, FX bundle ref) | GPGASRuntime |
| `UGP_CameraConfigDataAsset` | Camera tuning | GPRuntime |
| `UGP_FeedbackBundle_*` | Per-system VFX/SFX/anim soft refs | GPUIRuntime / GPRuntime split TBD |
| `UGP_NotificationConfig` | Toast metadata per tag | GPUIRuntime |
| `UGP_SwarmDefinition` (post-MVP) | SWARM unit identity | GPRuntime |
| `UGP_AIBehaviorDefinition` | AI thresholds, decision tick rate, action probabilities | GPRuntime |
| `UGP_OrbitalDropDefinition` (family `DA_GP_OrbitalDrop_*`) | Building READY purchase: cost, tags, BuildingDefinition, timing. Not Wall Package. | GPRuntime |
| `UGP_WallPackageDefinition` (`DA_GP_WallPackage`) | Wall Package: DisplayName, Icon, Cost, SegmentCount=5, delivery timing. Not READY. | GPRuntime |
| `UGP_BuildGridConfig` | Grid cell size (200 cm), pathfinding iteration cap, A* heuristic flavor | GPRuntime |
| `UGP_SwarmThreatCurves` | `ThreatToWaveSize` / `ThreatToWaveFrequency` curves over `FerroniteThreatValue`; `ThreatPerStoredUnit` (default 1.0) | GPRuntime |

DataAssets зберігаються у `/Game/GrimProtocol/DataAssets/<Category>/`. PrimaryAssetType per category для `UAssetManager` registration.

## Gameplay Tags

Native registry у `GPGASRuntime/Public/Tags/GPGameplayTags.h` (`FGPGameplayTags`, `AddNativeGameplayTag` at `GPGASRuntime` startup).

**Authoritative list:** [`09_Gameplay_Tags.md`](09_Gameplay_Tags.md). This section is a synchronized summary only — do not diverge.

```
GP.Match.State.{Loading, WaitingForPlayers, Playing, Paused, Finished}
GP.Match.WinReason.{DeliveryQuota, TimerScore, Annihilation, OpponentDisconnect}

GP.Unit.Type.{Worker, SalvageWalker, Combat, Support, Building}
GP.Unit.State.{Moving, Mining, Repairing, Attacking, AttackCooldown, Dead, Stunned}

GP.Building.Type.{MainBase, LogisticsHub, DefensiveTurret, Wall, WallTurret, FerroniteDeposit}
GP.Building.Role.{Command, Logistics, Defense, Resource}

GP.Resource.Type.Ferronite
GP.Resource.Node

GP.Command.{Move, Stop, Attack, AttackMove, Mine, Repair, Sell, Demolish, OrderDrop, CancelOrder}

GP.Drop.Type.{Unit, Building, WallPackage, Module}
GP.State.PodInFlight

GP.Ability.Repair
GP.Ability.State.Channeling

GP.Capability.{Selectable, Inspectable}
GP.Selection.Type.{Unit, Building}

GP.Faction.Corporate

GP.Team.{Neutral, Player.One, Player.Two}   // NOT Team.A / Team.B

GP.Effect.Source.UnitCapBuilding

GP.Notify.{InsufficientOrbitalFerronite, UnitCapReached, WorkerIdle,
           BaseUnderAttack, DropRejected, MatchEndingSoon, CommandRejected}
// NOT GP.Notify.InsufficientOrbital
```

Жодних magic-string tags у коді (review-blocking per CONTRIBUTING).

## Attributes

### `UGP_PlayerAttributeSet`

```
OrbitalFerronite (spendable currency; default 0; COND_OwnerOnly;
                  += on container launch via GE_GP_AddOrbital, -= on accepted drop via GE_GP_SpendOrbital)
FerroniteScore   (cumulative shipped value = victory score; monotonic; COND_None;
                  NOT spendable — spending OrbitalFerronite does NOT reduce it)
CurrentUnits     (COND_OwnerOnly)
MaxUnits         (COND_OwnerOnly)
```

Note — Planetary Ferronite (raw, stored in MainBase containers) is NOT a player attribute. It lives in `UGP_StorageComponent` and is mirrored on GameState as `FerroniteThreatValue` (a fluctuating stock). Deprecated/removed: `Ferronite` (single spendable pool), `MaxFerronite`.

### `UGP_UnitAttributeSet`

```
Health           (clamp 0..MaxHealth)
MaxHealth
Armor
DamageResistance
AttackCooldown   (cooldown base — duration GE reads це)
Damage           (source-side; used by UGP_DamageCalculation)
AttackRange      (used by CombatComponent)
AttackSpeed      (used by cooldown duration calc)
MoveSpeed        (read by UGP_MovementComponent)
CarriedFerronite (Worker only; capped at WorkerCarryCapacity)
```

Attribute initialization — `GE_GP_Init_<UnitDef>` Instant on spawn, magnitudes з UnitDefinition.

## GameplayEffects (Content, not C++)

| Effect | Duration | Modifier | Source |
| --- | --- | --- | --- |
| `GE_GP_Init_<Unit>` | Instant | All base attributes | Spawn |
| `GE_GP_Damage_Basic` | Instant | Health −Magnitude (via DamageCalc) | Combat tick |
| `GE_GP_Cooldown_Attack` | Duration (1/AttackSpeed) | Tag `AttackCooldown` | Combat tick |
| `GE_GP_AddOrbital` | Instant | OrbitalFerronite += Volume × OrbitalConversionRate | Container launch |
| `GE_GP_AddScore` | Instant | FerroniteScore += Volume × ScoreConversionRate | Container launch |
| `GE_GP_SpendOrbital` | Instant | OrbitalFerronite −= Cost | Accepted drop order |
| `GE_GP_RefundOrbital` | Instant | OrbitalFerronite += N | Post-MVP (cancel/refund) |
| `GE_GP_Cost_RepairTick` | Instant | OrbitalFerronite −= RepairCost (TBD) | Worker repair tick (MVP) |
| `GE_GP_UnitCap_Plus5` | Infinite | MaxUnits +UnitCapContribution | LogisticsHub alive |
| `GE_GP_Heal` | Instant | Health +N | Repair tick |
| `GE_GP_AttackHit` | Instant | (Aggregator wrapper) | Attack apply |

Worker container fill on drop-off is a `UGP_StorageComponent` state mutation (raises `FerroniteThreatValue`), **NOT** a player-attribute GE. Removed/deprecated effects: `GE_GP_AddFerronite`, `GE_GP_SendToOrbit`, `GE_GP_SpendFerronite`, `GE_GP_RefundFerronite`.

Effects живуть у `/Game/GrimProtocol/Effects/`. `TSoftClassPtr` refs у DataAssets.

## SWARM Threat Model

SWARM wave intensity / frequency / targeting scale from `FerroniteThreatValue` — the raw Planetary Ferronite **currently stored** in MainBase containers (a fluctuating stock on GameState), via DA curves `ThreatToWaveSize` and `ThreatToWaveFrequency` (`UGP_SwarmThreatCurves`).

- Worker drop-off **raises** `FerroniteThreatValue` → more pressure (greed has a cost).
- Container launch-to-orbit **lowers** it → less pressure (shipping = relief / safety).
- `FerroniteScore` and `OrbitalFerronite` do **NOT** drive SWARM pressure.
- Optional per-unit scalar `ThreatPerStoredUnit` (default 1.0).

Deprecated/removed: `SwarmAggressionLevel` (monotonic shipped/mined accumulator), `AggressionPerUnitShipped`, `AggressionPerUnitMined`. There is no monotonic aggression accumulator — pressure tracks the live stored stock.

## RPC Inventory

Authority — server only unless noted. All `Server_*` мають `Server, Reliable, WithValidation`. All `Multicast_*` — `Unreliable` cosmetic.

| RPC | Caller | Receiver | Purpose |
| --- | --- | --- | --- |
| `Server_RequestCommand(FGP_CommandRequest)` | Client PC | Server PC | Standard command pipeline (Move/Attack/Mine/Repair/Stop). AI does NOT use this — it calls the server-side command-execution helpers directly. |
| `Server_RequestOrbitalDrop(UGP_OrbitalDropDefinition*, FVector, FRotator)` | Client PC | Server PC | Order orbital drop. Validates OrbitalFerronite + grid footprint, applies `GE_GP_SpendOrbital`, enqueues into `UGP_OrbitalDeliverySubsystem`. Rejection via `Client_NotifyCommandRejected`. |
| `Server_SetReady(bool)` | Client PC | Server (LobbyState) | Lobby ready toggle. |
| `Server_StartMatch()` | Host PC | Server | Trigger ServerTravel. |
| `Client_NotifyCommandRejected(FGameplayTag CmdTag, uint8 Reason)` | Server | Owning client | Rejection UX. |
| `Client_HUDNotification(FGameplayTag Kind, FText Body)` | Server | Owning client | Targeted notification. |
| `Multicast_PlayAttackVFX(FVector TargetLoc, AActor* TargetActor)` | Server | All clients | Combat cosmetic. |
| `Multicast_PlayDeathVFX()` | Server | All clients | Death cosmetic. |
| `Multicast_PlayDropOffFX()` | Server | All clients | Worker container drop-off cosmetic. |
| `Multicast_PlayPodDescentFX()` | Server | All clients | Drop pod descent cosmetic. |
| `Multicast_PlayPodLandFX()` | Server | All clients | Drop pod landing / payload reveal cosmetic. |
| `Multicast_PlayLaunchFX()` | Server | All clients | Container launch-to-orbit cosmetic. |

Multicast cap target ≤ 30/s server-wide per [`12_UI_Architecture` §Multiplayer Notes](12_UI_Architecture.md). Не використовувати multicast для gameplay state.

Жодного `Server_ApplyDamage`, `Server_SetHealth`, `Server_SpawnUnit` як client-callable — це server-internal flow.

## Replication Conditions

| Field / Attribute | Condition | Notes |
| --- | --- | --- |
| `UGP_PlayerAttributeSet.OrbitalFerronite, CurrentUnits, MaxUnits` | `COND_OwnerOnly` | Private. |
| `UGP_PlayerAttributeSet.FerroniteScore` | `COND_None` | Score race visible to all. |
| `UGP_UnitAttributeSet.*` | Mixed (per ASC mode) | Standard GAS replication. |
| `UGP_StorageComponent.StoredVolume` | `COND_OwnerOnly` | Container fill (Planetary Ferronite); private. |
| `UGP_CargoComponent.CurrentCargo` | `COND_None` | Cargo over-head indicator. |
| `AGP_GameState.{MatchState, MatchTimeRemaining, FerroniteThreatValue, WinnerTeamId, WinReason, DeliveryQuotaFerroniteScore, bAnnihilationCountsAsWin, MatchSeed, MatchResult}` | `COND_None` | Universal. `MatchResult.FinalScores` is a replicated array snapshot taken at finish. |
| `AGP_LobbyState.Players, bAllReady` | `COND_None` | Lobby. |
| `AGP_DropPod.PayloadDefRef, DescentProgress01` | `COND_None` | Visible drop pod in flight. |
| `AGP_FerroniteDeposit.CurrentCapacity, ActiveMiners.Num()` | `COND_None` | Visible. |

ASC replication modes:

- `AGP_PlayerState.ASC` — `Mixed` (player gameplay shown to all for score; some attrs OwnerOnly).
- `AGP_UnitBase.ASC` — `Minimal` (per [`02_GAS_Architecture`](02_GAS_Architecture.md)).

## Authority Boundaries

- **Server only:** Spawning, damage, attribute writes, GE application, ability execution, command validation, mining tick, combat tick, drop-order validation + `AGP_DropPod` lifecycle + payload spawn, container drop-off / launch, score conversion, `FerroniteThreatValue` updates, win condition.
- **Client only:** Input intent, selection, camera, drop-targeting reticle / ghost wall preview, HUD VM updates from replicated state, cosmetic decals/sounds, notification toast scheduling.
- **Shared (replicated state):** Unit transform via PawnMovementComponent standard; GAS attributes; replicated component state (StorageComponent StoredVolume, CargoComponent CurrentCargo, DropPod DescentProgress01).

Жоден client-direct gameplay write. Жодних `Multicast` для gameplay state. Жодного `LocalPredicted` ability мode у MVP unless explicit performance need.

## Asset Loading

Per [`ADR-0002 §Asset Manager Loading Flow`](../Architecture_Decisions/ADR_0002_Data_Driven_First.md):

- `UGP_MatchAssetLoader::PreloadForMatch(FactionA, FactionB, Map)` гате до `MatchState=Playing`.
- Soft refs resolved via `Resolve<T>()` / `ResolveClass<T>()` after preload.
- On-demand load `LoadOnDemand()` для post-MVP unlocks.
- `ReleaseAll()` на map travel / match end.

## Risks

| Risk | Likelihood | Impact | Mitigation |
| --- | --- | --- | --- |
| Soft-ref handle leak (forgot ReleaseAll) | M | Memory bloat | Test fixture: assert handle count == 0 post-travel; CI gate. |
| Multicast spam у dense combat | M | Net usage | Bundle damage events по threshold, cap ≤ 30/s/server. |
| GAS attribute write outside server | L | Cheat surface | Code review + `HasAuthority()` guards; CI grep for `Set*Attribute` outside server-only paths. |
| `UGP_SelectionComponent` ref leak from `OnDeath` delegates | M | Stale UI | Strict unbind у component destroy; tests. |
| Hard ref slipping into DataAsset field | M | Memory bloat, plugin cascade | Lint pass: grep `TObjectPtr<U..._Definition>` / `TSubclassOf<U` у `*.h` що містять `UPrimaryDataAsset`. |
| NavMesh re-path on 24-worker move | L | CPU spike | Profile у GP-0203A; consider shared path cache post-MVP. |
| Steam Spacewar dev AppID у shipping | L | Ship break | Build-time `#if !UE_BUILD_SHIPPING` assert. |
| Common UI activation stack conflict із Enhanced Input | M | Input misroute | Test fixture: modal open → gameplay IMC suspended; close → restored. |
| 24-worker group move NavMesh repath cost | L | CPU spike | GP-0203A profile; cache or shared path. |
| MID per unit memory creep | L | Memory | Cap pool size; reuse MID base when material params identical. |
| Drop-order spam / invalid drop spam | M | Net + strategic | Server validates OrbitalFerronite + grid footprint before enqueue; reject via `Client_NotifyCommandRejected`; per-PC enqueue rate cap. |
| `FerroniteThreatValue` desync between Storage and GameState | M | Wrong SWARM pressure | Single server-authoritative writer (StorageComponent → GameState); test fixture asserts sum-of-containers == GameState value. |
| Late-join missing data | — | — | Late-join out of MVP. No mitigation needed. |
| Listen-server host advantage | L | Latency asymmetry | Acceptable у 2-player MVP. |

## Implementation Order (Suggested Slices)

Per ADR-0006 — one slice at a time. Each slice = small, reviewable, with docs sync + playtest note. Stop after each, await approval per [`Claude_Task_Backlog` Global Stop Rule](../Development/Claude_Task_Backlog.md).

```
Slice 1 — Foundation
  GP-S01  Module scaffolds: GPRuntime / GPGASRuntime / GPUIRuntime Build.cs, empty .Build.cs deps confirmed.
  GP-S02  Native tags registry (FGPGameplayTags) із MVP taxonomy.
  GP-S03  Attribute Sets (UGP_PlayerAttributeSet, UGP_UnitAttributeSet) із replication boilerplate.
  GP-S04  UGP_AbilitySystemComponent base.
  GP-S05  UGP_DamageCalculation MMC stub.

Slice 2 — Match Flow + Asset Loader
  GP-S06  AGP_GameState (MatchState, Timer).
  GP-S07  AGP_GameMode (PostLogin, EndMatch hook).
  GP-S08  AGP_PlayerController scaffold (Possess CameraPawn, ASC linkage on PlayerState).
  GP-S09  AGP_PlayerState (+ASC + AttributeSet).
  GP-S10  UGP_MatchAssetLoader (PreloadForMatch + Resolve API).
  GP-S11  AGP_LobbyState (replicated FGP_LobbyPlayer list).

Slice 3 — Camera (GP-0201A)
  GP-S12  UGP_CameraConfigDataAsset.
  GP-S13  AGP_CameraPawn (Pan/Zoom/Rotate/Edge-scroll, soft-ref Config + AsyncLoad).
  GP-S14  AGP_CameraBoundsVolume.
  GP-S15  IMC_GP_Camera + IA_Camera_* assets, PlayerController binding.

Slice 4 — Selection + Smart Commands (GP-0202A + GP-0203A intent layer)
  GP-S16  UGP_SelectionComponent (SelectedUnits, InspectedTarget, marquee, control groups).
  GP-S17  UGP_CommandComponent (BuildSmartCommand, Server_RequestCommand dispatch).
  GP-S18  AGP_UnitBase abstract + SetSelectionHighlight (MID instance flow).
  GP-S19  FGP_CommandRequest struct + native tag mapping.

Slice 5 — Movement (GP-0203A unit layer)
  GP-S20  UGP_MovementComponent (NavMesh, MoveTo).
  GP-S21  AGP_MobileUnit base.
  GP-S22  Server_RequestCommand routing → unit.ReceiveCommand → MovementComponent.

Slice 6 — Worker + Resources (GP-0302A + GP-0303A)
  GP-S23  UGP_ResourceDefinition (Ferronite metadata).
  GP-S24  AGP_FerroniteDeposit (capacity, soft-cap + queue).
  GP-S25  UGP_CargoComponent.
  GP-S26  UGP_MiningComponent (state machine із auto-cycle).
  GP-S27  AGP_Worker (`bAutoAttacks=false`; mines + carries raw Ferronite, repairs, no Build).
  GP-S28  UGP_StorageComponent drop-off mutation + GameState FerroniteThreatValue write (container fill, no player GE).

Slice 7 — Combat (GP-0204A)
  GP-S29  UGP_CombatComponent (LOS 3-trace, fire loop).
  GP-S30  UGP_TargetingComponent (auto-acquire).
  GP-S31  GE_GP_Damage_Basic, GE_GP_Cooldown_Attack.
  GP-S32  Attack-move state + AttackMoveDestination on AGP_MobileUnit.
  GP-S33  Multicast_PlayAttackVFX scaffold.

Slice 8 — Buildings + Orbital Drops + Wall + Grid (post-pivot)
  GP-S34  AGP_BuildingBase abstract + UGP_BuildingDefinition (FootprintCells, ClearanceCells, bMountsOnWall, bCanHostWallMount).
  GP-S35  UGP_BuildGridSubsystem (cell occupancy, CanPlaceFootprint, A* PathfindFreeCells).
  GP-S36  UGP_StorageComponent launch side (container ship-to-orbit → GE_GP_AddOrbital + GE_GP_AddScore, lower FerroniteThreatValue) per TDD/07 Container Update.
  GP-S37  UGP_OrbitalDeliverySubsystem (Server_RequestOrbitalDrop, GE_GP_SpendOrbital, TryEnqueueOrder, grid-aware validation, AGP_DropPod lifecycle).
  GP-S38  AGP_DropPod (descent, payload spawn) + UGP_OrbitalDropDefinition DataAsset.
  GP-S39  AGP_MainBase (BP child з Storage + drop-zone marker, no production).
  GP-S40  AGP_LogisticsHub (orbital drop only; GE_GP_UnitCap_Plus5 + storage cap bonus).
  GP-S41  AGP_DefensiveTurret free-standing 4×4 (TargetingComponent + CombatComponent).
  GP-S42A Wall Package Data + MainBase Wall Inventory (purchase, one rocket, stock 0..5, depot event, Build Wall availability). **Next implementation after GP-S41M.**
  GP-S42B AGP_Wall + UGP_WallConnectionComponent (2×2, 8-dir bitfield, local neighbor refresh; no player drag).
  GP-S42C Wall Drag Placement (Build Wall mode, inventory-limited preview, atomic consume + spawn via BuildGrid / wall authority — **not** OrbitalDeliverySubsystem / DropPod).
  GP-S43  AGP_WallTurret variant (2×2 wall-mounted, later).
  GP-S44  AGP_DropReticle (local building-deploy reticle). Ghost wall preview lives in S42C.
  GP-S45  **SUPERSEDED by GP-S42C** — old “sequential pod cascade / PathLength × cost” must not be implemented.
  GP-S46  UGP_GA_Repair (Worker channel ability — retained from pre-pivot for damage repair).
  GP-S46A Sell + Demolish (GP-0307): Server_SellBuilding / Server_DemolishWalls RPCs, UGP_BuildingDefinition.bSellable/SellRefundRate fields, GE_GP_RefundOrbital, demolish cursor mode UI integration.

Slice 9 — UI Foundation + FoW (GP-0401A skeleton + TDD/15 hookup)
  GP-S47  Required Project Setup: enable stock plugins CommonUI + ModelViewViewModel in GP.uproject (CommonInput as module dep under Common UI; EnhancedInput for gameplay IMC). Do NOT enable CommonGame / Lyra. Matching deps in GPUIRuntime.Build.cs (CommonUI + MVVM are a HARD requirement). Then UGP_UserWidgetBase, UGP_ActivatableScreenBase. Note: plugin enablement for blank baseline is owned by GP-S01; S47 focuses on widget bases once plugins exist.
  GP-S48  UGP_FogOfWarComponent (server grid + sight tick) + UGP_LocalFoWComponent (client mirror).
  GP-S49  ViewModels (UGP_ResourceVM, UGP_MatchVM, UGP_SelectionVM, UGP_OrderMenuVM, UGP_CargoVM, UGP_NotificationVM, UGP_MinimapVM, UGP_LobbyVM, UGP_FoWVM).
  GP-S50  VM Adapters (UGP_*VMAdapter) як PC subobjects.
  GP-S51  UGP_HUDWidget + WBP_GP_HUD_Match BP composition.
  GP-S52  UGP_MinimapSubsystem (5 Hz snapshot з 3-layer FoW render).
  GP-S53  WBP_GP_OrderMenu (Common UI Activatable) + drop-targeting mode wiring.

Slice 10 — AI Opponent (GP-0702 + per ADR-0008)
  GP-S54  AGP_AIController : AAIController (server-only, state machine).
  GP-S55  UGP_AIBehaviorDefinition DataAsset (thresholds, decision interval).
  GP-S56  State implementations (Explore / Mine / Ship / Order / Defend) — invokes server-side command-execution helpers DIRECTLY (no client Server_RequestCommand / Server_RequestOrbitalDrop RPC).

Slice 11 — Feedback Pass (GP-0402A)
  GP-S57  UGP_FeedbackBundle_* DataAssets (Worker, Building per type, Resource, Command, Notification, Drop).
  GP-S58  Notification system (UGP_NotificationVM + Server→Client_HUDNotification wiring).
  GP-S59  Decal pool / per-PC spawner.
  GP-S60  Damage flash MID parameter wiring у AGP_UnitBase.

Slice 12 — Steam MVP (GP-0501A)
  GP-S61  UGP_SessionSubsystem (Host / Find / Join / Destroy).
  GP-S62  Lobby flow → ServerTravel.
  GP-S63  Disconnect / failure handling per failure matrix.
  GP-S64  Main Menu BP wiring.

Slice 13 — Match End + Polish
  GP-S65  Win condition wiring (delivered early as **GP-S34W**): primary first-to-DeliveryQuotaFerroniteScore (placeholder 5000) → GP.Match.WinReason.DeliveryQuota; fallback at 10:00 highest FerroniteScore → TimerScore; secondary MainBase destroyed → Annihilation (bAnnihilationCountsAsWin=true); tie-break FerroniteScore → OrbitalFerronite → CurrentUnits → MatchSeed. OrbitalFerronite is NEVER the victory score. OpponentDisconnect / Spectating / polished end screen remain deferred.
  GP-S66  EndOfMatch activatable screen.
  GP-S67  Stress test: 2-player + AI full match end-to-end.
```

Each `GP-S##` slice — окрема code task; не починається без зеленого світла.

## Pillar 8 — MVP Production Rule (Cross-Slice Gate)

Per [`../GDD/01_Game_Pillars`](../GDD/01_Game_Pillars.md) §Pillar 8 (Simple Core, Combinatorial Depth) — додано 2026-05-16.

Будь-яка mechanic-introducing