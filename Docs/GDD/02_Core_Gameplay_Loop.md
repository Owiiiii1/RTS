# Core Gameplay Loop

> **Canonical model:** цей документ узгоджений з [ADR-0009 (Orbital Delivery)](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md). Усі non-initial units / buildings / walls прибувають **orbital drop** (no local production / construction). Деталі shipping pipeline — [`10_Orbital_Delivery`](10_Orbital_Delivery.md); two-state Ferronite — [`06_Resources`](06_Resources.md) §Container System; будівлі — [`05_Buildings`](05_Buildings.md). Where GDD/12 §4 надає більш деталізований loop spec, він — verbatim source; цей doc описує той самий loop без суперечностей.

## Canonical Loop (One Sentence Per Step)

```
Land  ->  Scout  ->  Mine  ->  Carry to MainBase containers (raw Planetary Ferronite, +FerroniteThreatValue)
      ->  Container fills  ->  Launch to orbit (-FerroniteThreatValue, +OrbitalFerronite +FerroniteScore)
      ->  Order drops (spend OrbitalFerronite)  ->  Pods deliver units / buildings / walls
      ->  Expand / Defend (SWARM scales with FerroniteThreatValue)
      ->  Win by Delivery Quota OR highest FerroniteScore at timer.
```

Старт: MainBase + 2 Workers pre-deployed; `OrbitalFerronite = 0`.

## Loops Overview

### Second-to-Second

- Select units (LMB / marquee).
- Issue command (move / attack / mine / repair / stop).
- Read feedback (selection ring, command marker, damage numbers, score increment flash on container launch).
- Adjust camera (pan, zoom, rotate).
- Glance at HUD readouts: Match Timer, own FerroniteScore, opponent FerroniteScore, OrbitalFerronite, FerroniteThreatValue (swarm pressure).

### Minute-to-Minute

- Send Workers to Ferronite Deposit (`GP.Command.Mine`).
- Worker mines → carries raw Ferronite → returns до MainBase → drop-off наповнює container (raw Planetary Ferronite, **raises `FerroniteThreatValue`**).
- Container fills → launch to orbit: `-FerroniteThreatValue`, `+OrbitalFerronite`, `+FerroniteScore`.
- Spend `OrbitalFerronite` через Order Menu: order orbital drops (Workers, Salvage Walkers, Logistics Hub, Defensive Turret, Walls).
- Increase unit cap via Logistics Hub drop (`UGP_PlayerAttributeSet.MaxUnits`). Per Pillar 4 — capacity expansion є свідомий strategic spend.
- React to SWARM waves: position Salvage Walker, drop Defensive Turret / Walls біля threatened deposits / buildings; Worker repairs damaged assets (`GP.Command.Repair`).
- Scout opponent (minimap awareness, mid-match push для denied expansion).
- Balance **greed vs safety**: тримати raw Ferronite у containers = вищий `FerroniteThreatValue` = більше swarm pressure; швидко shipping = безпечніше + швидше score.

### Match-to-Match

- У MVP: stand-alone match, без persistence. Match → result → return to lobby/menu.
- Off-world meta upgrades між матчами — у `Backlog`.

## Player Actions (MVP)

| Action | Gameplay Tag (request) | Owner Component | Server-Validated? |
| --- | --- | --- | --- |
| Move | `GP.Command.Move` | `UGP_CommandComponent` | Yes |
| Attack | `GP.Command.Attack` | `UGP_CommandComponent` | Yes |
| Mine | `GP.Command.Mine` | `UGP_CommandComponent` | Yes |
| Repair | `GP.Command.Repair` | `UGP_CommandComponent` → GAS | Yes |
| Stop | `GP.Command.Stop` | `UGP_CommandComponent` | Yes |
| Order Drop | `GP.Command.OrderDrop` | `UGP_CommandComponent` → Order Menu | Yes |
| Sell | `GP.Command.Sell` | `UGP_CommandComponent` | Yes |
| Demolish | `GP.Command.Demolish` | `UGP_CommandComponent` | Yes |

Немає `GP.Command.Build` / `Produce` — local construction / production усунено (orbital delivery canon).

## Flow Diagram

```
Player input (Enhanced Input)
   |
   v
AGP_PlayerController (local)
   |
   +-- selection update --> UGP_SelectionComponent (local-authoritative)
   |
   +-- command issue ----> UGP_CommandComponent (local intent)
                                    |
                                    v
                          Server_RequestCommand RPC
                                    |
                                    v
                          AGP_PlayerState validates ownership
                                    |
                                    v
                          AGP_GameMode / target validates capability
                                    |
                                    v
                          GAS ability activate (if needed)
                                    |
                                    v
                          Unit / subsystem behavior update (server-side)
                                    |
                                    v
                          Replicated state -> clients
                                    |
                                    v
                          UI update (HUD, command marker), VFX (multicast cosmetic)
```

## Economy Loop (MVP)

1. Worker `GP.Command.Mine` на `Ferronite Deposit`.
2. Worker рухається до deposit (`UGP_MovementComponent`).
3. Worker mining tick (`UGP_MiningComponent`) інкрементує локальний `CarriedFerronite` (worker-level).
4. Worker auto-returns до MainBase при capacity hit або deposit depletion.
5. На MainBase drop-off (Container System per [`06_Resources`](06_Resources.md)):
   - Container Storage state mutation: raw Planetary Ferronite додається у MainBase containers (NOT spendable).
   - `AGP_GameState.FerroniteThreatValue += dropped` — stored-at-base stock зростає → SWARM pressure зростає.
6. Worker auto-chain повертається до deposit (якщо deposit alive).
7. Коли container full → **launch to orbit**:
   - `GE_GP_AddOrbital` → `OrbitalFerronite += converted` (spendable).
   - `GE_GP_AddScore` → `FerroniteScore += converted` (monotonic victory score).
   - `FerroniteThreatValue -= launched` — shipping relieves SWARM pressure.
8. Player витрачає `OrbitalFerronite` через `GE_GP_SpendOrbital` при order drop (Order Menu).

Деталі — [`06_Resources`](06_Resources.md), [`10_Orbital_Delivery`](10_Orbital_Delivery.md).

## Acquisition Loop (Orbital Drops — MVP)

Заміняє pre-pivot production + construction loops. Local production / construction усунено.

1. Player opens Order Menu (`GP.Command.OrderDrop`).
2. Selects drop type: `GP.Drop.Type.{Unit, Building, Wall}` (Worker, Salvage Walker, Logistics Hub, Defensive Turret, Wall, Wall Turret).
3. Server validates: `OrbitalFerronite >= UGP_OrbitalDropDefinition.Cost`, drop zone valid (actively-visible FoW, cell occupancy, clearance), `CurrentUnits < MaxUnits` для unit drops.
4. `GE_GP_SpendOrbital` applied (-Cost `OrbitalFerronite`).
5. `UGP_OrbitalDeliverySubsystem` спавнить `AGP_DropPod` (2-3 s telegraph descent, visible усім + SWARM).
6. Pod lands → unit / building / wall deployed **immediately operational** (no construction phase).

Деталі — [`10_Orbital_Delivery`](10_Orbital_Delivery.md), [`05_Buildings`](05_Buildings.md), [`04_Units`](04_Units.md).

## Repair Loop (MVP)

1. Player виділяє Worker, issues `GP.Command.Repair` на own-team damaged building / unit.
2. Server validates: target own-team, target damaged (`Health < MaxHealth`), worker у range.
3. Worker рухається до target, repair tick застосовує heal + `GE_GP_Cost_RepairTick` (repair cost — TBD).
4. На full heal / target loss / player cancel — repair завершується.

Деталі — [`04_Units`](04_Units.md) §Worker.

## Combat Loop (MVP)

1. Player issues `GP.Command.Attack` з target (SWARM unit, enemy unit, enemy building).
2. Server validates: атакуючий — combat-type unit / turret, target alive, target ворожої team або SWARM.
3. Combat unit входить у attack range (`UGP_MovementComponent` + `UGP_TargetingComponent`).
4. `UGP_CombatComponent` тригерить attack cooldown.
5. На кожному attack tick — damage `UGameplayEffect` до target `UGP_UnitAttributeSet.Health`.
6. Damage — deterministic HP-based (`DamageDealt = Attacker.Damage`).
7. Якщо `Health <= 0` — server тригерить death state (tag `GP.Unit.State.Dead`), Multicast death VFX.

Деталі — [`04_Units`](04_Units.md), [`05_Buildings`](05_Buildings.md).

## SWARM Pressure Loop (FerroniteThreatValue-Driven)

1. Server `AGP_GameMode` тікає `WaveTimer`. На expiry → trigger SWARM wave.
2. Wave size — function of `AGP_GameState.FerroniteThreatValue` = **raw Ferronite, що зараз зберігається у MainBase containers**.
3. Drop-off ↑ `FerroniteThreatValue` (більше зберігаєш на базі = небезпечніше); launch до орбіти ↓ `FerroniteThreatValue` (shipping = безпека).
4. `FerroniteScore` / `OrbitalFerronite` **не** впливають на swarm pressure — лише stored-at-base stock.
5. SWARM units спавняться у `WaveSpawnPoints`, рухаються до найближчого player asset.
6. Player Salvage Walker, Turret, Wall engage / block SWARM.
7. Якщо undefended — SWARM знищує worker, building, або сам MainBase (annihilation loss).

**Greed-vs-safety:** hoard raw Ferronite = dangerous (high threat); ship швидко = safe (low threat). Це central tension per Pillar 6. Деталі — [`03_Factions`](03_Factions.md), [`07_Match_Flow`](07_Match_Flow.md).

## Score / Win Pressure Loop

1. Матч триває до 10 хвилин (countdown) АБО до досягнення quota.
2. `FerroniteScore` інкрементується тільки при container launch до орбіти.
3. Score visible у HUD (own + opponent). Постійна tension.
4. Перший до `DeliveryQuotaFerroniteScore` (placeholder 5000) виграє миттєво; інакше на 0:00 — highest `FerroniteScore` wins.

Деталі — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).

## Failure Loops

- Player не керує economy → не може ship → не може order drops → SWARM знищує base → annihilation loss.
- Player hoardить raw Ferronite, не ship → `FerroniteThreatValue` росте → SWARM waves знищують workers → mining stops → програш.
- Player overcommits до defense → `OrbitalFerronite` іде на Turrets/Walls замість швидшого темпу → opponent випереджає у score → програш.

Balance цих failure loops — primary gameplay tension.

## Required Player Decisions (Decision Points)

| Decision | Trigger | Trade-Off |
| --- | --- | --- |
| **Hoard vs Ship** | Кожен container fill. | Тримати raw Ferronite (вищий threat, ризик) vs ship зараз (safe, +score). |
| **Capacity expansion** | При `CurrentUnits == MaxUnits`. | Order Logistics Hub drop (+5 cap, spend OrbitalFerronite) vs тримати tight roster. |
| **Defense investment** | Перед swarm escalation. | Drop Turret / Walls vs drop more Workers (mining throughput). |
| **Worker count** | Continuously. | More workers = більше mining + швидше росте threat при hoarding. |
| **Combat acquisition** | Mid-match. | Drop Salvage Walker (spend OrbitalFerronite, +1 cap usage) vs more workers. |
| **Expansion to remote deposits** | При near-base depletion. | Risk vulnerability під час transit vs deposit yield. |
| **Quota rush vs safe tempo** | Continuously. | Push до quota швидше (ризикованіше) vs стабільний safe shipping. |

Усі decisions converge у **delivery race**. Жодного decision без opportunity cost. Per Pillar 3 (One Resource, Many Tradeoffs).

## Resource Spend Rule

Усі spend transactions — через server-applied `UGameplayEffect`:

```
Player Order Drop (Order Menu)
   |
   v
Server validates:
   - Player owns the order (AGP_PlayerState.TeamId).
   - OrbitalFerronite >= UGP_OrbitalDropDefinition.Cost.
   - CurrentUnits < MaxUnits (для unit drops).
   - Drop zone valid (FoW visible, cell free, clearance OK).
   |
   v
GE_GP_SpendOrbital applied (Instant, -Cost OrbitalFerronite).
   |
   v
UGP_OrbitalDeliverySubsystem spawns AGP_DropPod.
   |
   v
On landing -> unit / building / wall operational -> replicate.
```

При `OrbitalFerronite < Cost` — GE failure, order fails, UI shows "Insufficient Orbital Ferronite" feedback (per [`09_UI_UX`](09_UI_UX.md)).

## Cap Increase Rule

Per Pillar 4 (Capacity Is Strategy). `MaxUnits` зростає **тільки** через explicit `OrbitalFerronite` spend на Logistics Hub drop:

```
Player orders Logistics Hub drop (Order Menu)
   |
   v
Server validates: OrbitalFerronite >= Cost, drop zone valid.
   |
   v
GE_GP_SpendOrbital applied (-Cost OrbitalFerronite).
   |
   v
AGP_DropPod descends -> Logistics Hub lands operational.
   |
   v
On landing:
   - GE_GP_UnitCap_Plus5 applied to owner PlayerState (+5 MaxUnits, Infinite duration).
   - +N MaxContainerCount bonus applied to MainBase Storage.
   - HUD updates CurrentUnits / MaxUnits через RepNotify.
   |
   v
On Logistics Hub destroyed / sold:
   - GE_RemovalOnDestroy removes Infinite GE -> MaxUnits -= 5; container cap bonus removed.
   - HUD updates.
```

**Hard constraint:** жодного auto-cap-growth, time-based cap, або bonus-cap-without-spend. Per A16 (Capacity-Ignoring Drift) у `gp-mechanics-validator`.

## Data Asset Requirements (MVP Canonical)

Усі балансові значення живуть у Data Assets. Жодного hardcoded balance у C++ (per Pillar 8 і ADR_0002).

| Data Asset | Purpose | Owner GDD Page |
| --- | --- | --- |
| `DA_GP_Faction_Default` | Corporate Colonial faction config — starting units (MainBase + 2 Workers), allowed drops, faction tags. | [`03_Factions`](03_Factions.md) |
| `DA_GP_Resource_Ferronite` | Resource metadata — DisplayName, tag, score conversion rate, threat-per-unit-stored, visual tint, icon. | [`06_Resources`](06_Resources.md) |
| `DA_GP_Building_MainBase` | Main Base config — health, container storage + launch range, drop-off zone, sight. | [`05_Buildings`](05_Buildings.md) |
| `DA_GP_Building_LogisticsHub` | Logistics Hub config — drop cost, health, `GE_GP_UnitCap_Plus5`, container cap bonus. | [`05_Buildings`](05_Buildings.md) |
| `DA_GP_Building_DefensiveTurret` | Defensive Turret config — drop cost, health, attack range, damage, attack speed, targeting priority. | [`05_Buildings`](05_Buildings.md) |
| `DA_GP_Building_Wall` | Wall config — drop cost per segment, health, footprint, clearance, auto-connect. | [`05_Buildings`](05_Buildings.md) |
| `DA_GP_Building_FerroniteDeposit` | Deposit properties — capacity, MineRatePerWorker, MaxConcurrentWorkers, depleted behavior. | [`05_Buildings`](05_Buildings.md), [`06_Resources`](06_Resources.md) |
| `DA_GP_Unit_Worker` | Worker config — drop cost, health, move speed, carry capacity, mine rate, allowed commands, granted abilities (Repair), tags. | [`04_Units`](04_Units.md) |
| `DA_GP_Unit_SalvageWalker` | Salvage Walker config — drop cost, health, move speed, damage, attack range, attack speed, allowed commands, tags. | [`04_Units`](04_Units.md) |
| `DA_GP_Swarm_Grunt` | SWARM Grunt config — health, damage, attack range, move speed, aggro radius, spawn weight. | [`03_Factions`](03_Factions.md) |
| `DA_GP_OrbitalDrop_*` | Per-drop `UGP_OrbitalDropDefinition` — drop type tag, payload, OrbitalFerronite Cost, telegraph time. | [`10_Orbital_Delivery`](10_Orbital_Delivery.md) |
| `DA_GP_AIBehavior_Default` | AI opponent state machine thresholds — worker target, roster target, ship/order/defend triggers, tick frequency. | [`03_Factions`](03_Factions.md) |

## Tags List (MVP Canonical)

Native-registered у `FGPGameplayTags` (per [`../TDD/09_Gameplay_Tags`](../TDD/09_Gameplay_Tags.md)). Magic-strings заборонені.

```
GP.Match.State.{Loading, WaitingForPlayers, Playing, Paused, Spectating, Finished}
GP.Match.WinReason.{DeliveryQuota, TimerScore, Annihilation, OpponentDisconnect}
GP.Faction.{Corporate, Swarm}
GP.Unit.Type.{Worker, SalvageWalker}
GP.Unit.State.{Idle, Moving, Mining, Carrying, Attacking, Repairing, Dead}
GP.Building.Type.{MainBase, LogisticsHub, DefensiveTurret, Wall, WallTurret, FerroniteDeposit}
GP.Drop.Type.{Unit, Building, Wall}
GP.Resource.Node
GP.Resource.Type.Ferronite
GP.Command.{OrderDrop, Repair, Move, Stop, Attack, Mine, Sell, Demolish}
GP.Team.{Neutral, Player.One, Player.Two}
```

Deprecated (do NOT use): `GP.Command.{Build, QueueProduction, CancelProduction, SetRallyPoint}`, `GP.Building.Type.{Barracks, AssemblyYard}`. Tag taxonomy фіксована. Розширення — через ADR або TDD update, не ad-hoc.

## Attributes List (MVP Canonical)

GAS-managed, per [`../TDD/02_GAS_Architecture`](../TDD/02_GAS_Architecture.md).

**`UGP_PlayerAttributeSet`** (на `AGP_PlayerState.ASC`):

| Attribute | Replication | Note |
| --- | --- | --- |
| `OrbitalFerronite` | `COND_OwnerOnly` | Spendable resource (orbital). Spent на drops. |
| `FerroniteScore` | `COND_None` | Cumulative shipped victory score, monotonic, visible усім. NEVER spendable. |
| `MaxUnits` | `COND_OwnerOnly` | Capacity ceiling. Default 5, +5 per Logistics Hub. |
| `CurrentUnits` | `COND_OwnerOnly` | Active unit count. Incremented on drop-arrival, decremented on death. |

REMOVED (pre-pivot): `Ferronite` pool, `MaxFerronite`.

**`UGP_UnitAttributeSet`** (на `AGP_UnitBase.ASC`):

| Attribute | Replication | Note |
| --- | --- | --- |
| `Health` | Replicated | Current HP. |
| `MaxHealth` | Replicated | Cap. |
| `Damage` | Replicated | Per-hit damage. |
| `Armor` | Replicated | MVP — 0 (без armor system). |
| `AttackRange` | Replicated | cm. |
| `AttackSpeed` | Replicated | hits/sec. |
| `MoveSpeed` | Replicated | cm/s. |
| `CarriedFerronite` | Replicated | Worker only; carry capacity tracking. |

**`AGP_GameState`** (global, не AttributeSet — replicated UPROPERTY):

| Property | Type | Note |
| --- | --- | --- |
| `MatchState` | `FGameplayTag` | Current `GP.Match.State.*`. |
| `MatchTimeRemaining` | `float` | Countdown from 600.0. |
| `FerroniteThreatValue` | `float` | Raw Ferronite stored-at-base. ↑ on drop-off, ↓ on launch. Drives SWARM. |
| `DeliveryQuotaFerroniteScore` | `float` | Win threshold (placeholder 5000). |
| `MatchResult` | `FGP_MatchResult` | Populated on Finished. |

## UI Requirements (Cross-Ref)

HUD bindings per [`09_UI_UX`](09_UI_UX.md) (CommonUI + MVVM — server updates ViewModels, widgets bind to VMs):

- Match Timer ← `AGP_GameState.MatchTimeRemaining`.
- Orbital Ferronite ← `UGP_PlayerAttributeSet.OrbitalFerronite` (owner-only).
- Own Score ← `UGP_PlayerAttributeSet.FerroniteScore`.
- Opponent Score ← opponent's `UGP_PlayerAttributeSet.FerroniteScore`.
- Unit Cap ← `UGP_PlayerAttributeSet.CurrentUnits / MaxUnits`.
- SWARM Pressure ← `AGP_GameState.FerroniteThreatValue`.
- Selection Panel ← `UGP_SelectionComponent.SelectedUnits` (local).
- Minimap ← world state (own / opponent units, buildings, deposits, SWARM spawn pulses).
- End-of-Match screen ← `AGP_GameState.MatchResult` via RepNotify.

Widget bindings — через ViewModel change delegates, **не tick polling** (per A9 anti-pattern).

## Excluded From MVP Loop (Out of Scope)

Усе нижче явно **не** входить у canonical MVP loop. Зміна — через ADR / pillar amendment.

- Research / tech tree mechanics.
- Multi-resource economy.
- Specialist combat units (sniper, siege, anti-armor).
- Worker self-defense / attack.
- Hero units.
- Off-world meta upgrades (persistent між матчами — Backlog).
- Player control over SWARM.
- Surrender / forfeit UI.
- Pings / map markers.
- Local production / construction (усунено per ADR_0009).

Per Pillars 1–9 і `gp-mechanics-validator` anti-patterns — будь-яке порушення цих exclusions review-blocking без pillar amendment.

## References

- Selection / command technical flow — [`../TDD/04_RTS_Selection_And_Commands`](../TDD/04_RTS_Selection_And_Commands.md).
- Match phases — [`07_Match_Flow`](07_Match_Flow.md).
- Win/lose — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- Resource flow technical — [`../TDD/07_Resource_Architecture`](../TDD/07_Resource_Architecture.md).
- Resource flow design + Container System — [`06_Resources`](06_Resources.md).
- Orbital Delivery — [`10_Orbital_Delivery`](10_Orbital_Delivery.md).
- SWARM faction — [`03_Factions`](03_Factions.md).
- HUD readouts — [`09_UI_UX`](09_UI_UX.md).
- Data Asset ownership — [`../TDD/10_Data_Assets`](../TDD/10_Data_Assets.md).
- GAS attributes — [`../TDD/02_GAS_Architecture`](../TDD/02_GAS_Architecture.md).
- Gameplay Tags taxonomy — [`../TDD/09_Gameplay_Tags`](../TDD/09_Gameplay_Tags.md).
- Pillars — [`01_Game_Pillars`](01_Game_Pillars.md).
- Orbital Delivery ADR — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md).
