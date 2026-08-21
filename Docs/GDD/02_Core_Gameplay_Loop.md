# Core Gameplay Loop

> **Canonical model:** узгоджений з [ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md) (+ 2026-08-08 procurement refinement + 2026-08-18 GP-0305R + 2026-08-21 ADR-0010). Non-initial units / READY buildings / Wall **material** / **Foundation Slab material** originate **from orbit**. READY buildings are **completed orbital assets** (no Worker construction after landing). Field engineering (level terrain, install/repair foundation, construct Walls) is **local Worker labor** on planned jobs. Units land at MainBase **Unit Drop Zone**. Buildings: **Purchase → READY inventory → Deploy** onto **leveled terrain + intact per-cell foundation**. Walls: **Buy Wall Package (5) → one rocket to MainBase → plan Build Wall → Workers construct on terrain (no Foundation required)**. `AGP_Wall` segments are **not** DropPod payloads. Деталі — [`10_Orbital_Delivery`](10_Orbital_Delivery.md), [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md); Ferronite — [`06_Resources`](06_Resources.md); будівлі — [`05_Buildings`](05_Buildings.md).

```
Mine → Containers → Launch → OrbitalFerronite (+ Score)
  → Unit Order (manifest / slots) → DropPod → Unit Drop Zone → control units
  → Level terrain (Worker job) → plan foundation install → Workers progressively install cells
  → Building Purchase → READY → Deploy ghost onto intact foundation → DropPod → building
  → Buy Wall Package → one rocket to MainBase → plan Build Wall → Workers construct on terrain
  → Buy Foundation Slab package → one delivery to MainBase → Worker install job (stock consume moment TBD)
```

Старт: MainBase + 2 Workers pre-deployed; `OrbitalFerronite = 0`.

## Canonical Loop (One Sentence Per Step)

```
Land  ->  Scout  ->  Mine  ->  Carry to MainBase containers (raw Planetary Ferronite, +FerroniteThreatValue)
      ->  Container fills  ->  Launch to orbit (-FerroniteThreatValue, +OrbitalFerronite +FerroniteScore)
      ->  Units: manifest order (transport slots) -> DropPod -> MainBase Unit Drop Zone
      ->  Site prep: plan Level Terrain / Foundation jobs, assign Workers, progressive labor
      ->  Buildings: Purchase -> READY inventory -> Deploy ghost onto intact foundation -> DropPod -> placed building
      ->  Walls: Buy Wall Package (5) -> rocket to MainBase -> plan Build Wall -> Workers construct (no Foundation)
      ->  Foundation: Buy Foundation Slab package -> delivery to MainBase -> Worker install job
      ->  Expand / Defend (SWARM scales with FerroniteThreatValue)
      ->  Win by Delivery Quota OR highest FerroniteScore at timer.
```

## Loops Overview

### Second-to-Second

- Select units (LMB / marquee).
- Issue command (move / attack / mine / repair / stop; later: Level Terrain / foundation install / Wall construction assignment — exact tags TBD).
- Read feedback (selection ring, command marker, damage numbers, score increment flash on container launch).
- Adjust camera (pan, zoom, rotate).
- Glance at HUD readouts: Match Timer, own FerroniteScore, opponent FerroniteScore, OrbitalFerronite, FerroniteThreatValue (swarm pressure).

### Minute-to-Minute

- Send Workers to Ferronite Deposit (`GP.Command.Mine`).
- Worker mines → carries raw Ferronite → returns до MainBase → drop-off наповнює container (raw Planetary Ferronite, **raises `FerroniteThreatValue`**).
- Container fills → launch to orbit: `-FerroniteThreatValue`, `+OrbitalFerronite`, `+FerroniteScore`.
- Spend `OrbitalFerronite` via orbital procurement:
  - **Units:** build transport-slot manifest → Confirm → DropPod to MainBase Unit Drop Zone.
  - **Buildings:** Purchase → READY inventory → later Deploy (ghost) → DropPod to chosen location (no second spend).
  - **Walls:** Buy Wall Package → one rocket to MainBase (stock 5, max 5) → later **plan Build Wall** (no READY, no second spend, no per-segment rocket, **no Foundation**). Workers must physically construct the planned segments.
- Prepare construction sites: plan **Level Terrain** / **Foundation install** jobs (grey = already level, yellow = needs work). Workers travel to the job; work progresses only while they are present. Worker still does **not** construct the READY building. See [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md).
- Increase unit cap via Logistics Hub (purchase/deploy onto prepared foundation). Per Pillar 4 — capacity expansion є свідомий strategic spend.
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
| Level Terrain / site prep | **Future** — exact tag/class TBD | Planned local-engineering job | Yes (when implemented) |
| Install / repair foundation | **Future** — exact tag/class TBD | Planned job; Worker labor; stock consume moment TBD | Yes (when implemented) |
| Assign Worker to engineering job | **Future** — exact tag/class TBD | Required for local construction progress | Yes (when implemented) |

Немає `GP.Command.Build` / `Produce` як Barracks / local READY-building production (orbital delivery canon, ADR-0009). Worker **does** later perform local engineering (level, install/repair foundation, construct Walls). That is **not** constructing the READY building.

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
8. Player витрачає `OrbitalFerronite` через `GE_GP_SpendOrbital`:
   - Unit Confirm (manifest total), or
  - Building Purchase (READY inventory), or
  - Wall Package purchase (one rocket to MainBase; stock 5), or
  - Foundation Slab package purchase (one delivery to MainBase; **quantity/cost TBD**, not automatically 5).
   Deploy of READY buildings, **Build Wall**, and foundation **installation** from inventory do **not** spend Orbital again.

Деталі — [`06_Resources`](06_Resources.md), [`10_Orbital_Delivery`](10_Orbital_Delivery.md).

## Acquisition Loop (Orbital Procurement — MVP)

Заміняє pre-pivot Barracks / factory production. Local **READY-building production** усунено. Local **field engineering** (level, foundation install/repair, Wall construction) is **in** MVP design (ADR-0010) as planned Worker jobs — not a Barracks queue. See [`10_Orbital_Delivery`](10_Orbital_Delivery.md), [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md).

### Units

1. Open Unit Order UI; fill manifest (transport slots; Worker example 1, Salvage Walker 2; pod capacity example 4).
2. Server validates Orbital total, slots, MaxUnits (reject full manifest if over), MainBase Unit Drop Zone.
3. `GE_GP_SpendOrbital` once → DropPod → Unit Drop Zone → multi-unit offsets → selectable.

### Buildings

1. Purchase → `GE_GP_SpendOrbital` once → READY inventory++.
2. Prepare site (when the Terrain stage exists): plan Level Terrain / Foundation install jobs; assigned Workers progressively complete cells. Foundation does not appear instantly from a click.
3. Deploy mode: ghost; Esc/RMB cancels (READY stays).
4. LMB valid (including leveled + intact foundation for normal buildings) → consume one READY → DropPod → building (**no second spend**).

### Foundation Slab packages (not READY buildings)

1. **Buy Foundation Slab package** (Wall Package philosophy: spend once, one delivery to MainBase inventory). Cost / quantity / slab footprint = **TBD**.
2. Player **plans** a Foundation install job. Assigned Workers travel and progressively install cells. No second Orbital spend. Stock consume/reserve moment = **DESIGN REQUIRED**. Per-cell coverage, not one all-or-nothing slab actor.

### Walls (not READY)

1. **Buy Wall Package** (stock 0..4, no in-flight) → `GE_GP_SpendOrbital` full package Cost once → one rocket to MainBase UnitDropZone.
2. Arrival: stock = min(5, current + 5). Excess wasted. Cannot buy at 5.
3. **Build Wall** (stock > 0) → plan/draw path ≤ inventory → planned job exists → assign Workers → segments become operational on completion. **No Foundation required.** Terrain suitability TBD. Stock consume/reserve moment = **DESIGN REQUIRED**.

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
| **Defense investment** | Перед swarm escalation. | Drop Turret / Buy Wall Package vs drop more Workers (mining throughput). |
| **Worker count** | Continuously. | More workers = більше mining + швидше росте threat при hoarding. |
| **Combat acquisition** | Mid-match. | Drop Salvage Walker (spend OrbitalFerronite, +1 cap usage) vs more workers. |
| **Expansion to remote deposits** | При near-base depletion. | Risk vulnerability під час transit vs deposit yield. |
| **Quota rush vs safe tempo** | Continuously. | Push до quota швидше (ризикованіше) vs стабільний safe shipping. |

Усі decisions converge у **delivery race**. Жодного decision без opportunity cost. Per Pillar 3 (One Resource, Many Tradeoffs).

## Resource Spend Rule

Усі Orbital spends — через server-applied Instant `UGameplayEffect` (`GE_GP_SpendOrbital`). Direct attribute mutate banned.

Spend moments:

| Action | Spend? |
| --- | --- |
| Unit manifest Confirm | Yes — total cost once |
| Building Purchase | Yes — purchase cost once → READY++ |
| Building Deploy (READY) | **No** — consume READY only |
| Wall Package purchase | Yes — package Cost once → rocket to MainBase |
| Build Wall (from inventory) | **No** — consume Wall stock only |
| Foundation Slab package purchase | Yes — package Cost once → delivery to MainBase (**quantity TBD**) |
| Install foundation (from inventory) | **No** — consume foundation stock only |
| Cancel ghost placement / wall preview | **No** |

Unit Confirm also validates transport slots + MaxUnits (reject whole manifest if over) + Unit Drop Zone. Building Deploy validates READY + placement, including **leveled terrain + intact per-cell foundation** for normal orbital buildings (initial MainBase excepted). **Wall Foundation Rule — RESOLVED:** Walls do not require Foundation.

При insufficient Orbital — order fails; UI "Insufficient Orbital Ferronite" (per [`09_UI_UX`](09_UI_UX.md)).

## Cap Increase Rule

Per Pillar 4 (Capacity Is Strategy). `MaxUnits` зростає **тільки** через Logistics Hub orbital **purchase** (READY) then **deploy**:

```
Purchase Logistics Hub → GE_GP_SpendOrbital → READY++
Deploy READY → DropPod → On landing: GE_GP_UnitCap_Plus5 (+MaxUnits) + container cap bonus
On Hub destroyed/sold → remove Infinite GE → MaxUnits/cap bonuses reverse
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
| `DA_GP_Building_Wall` | Wall config — health, footprint, clearance, auto-connect. Package cost lives on `DA_GP_WallPackage`. | [`05_Buildings`](05_Buildings.md) |
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
GP.Drop.Type.{Unit, Building, WallPackage}
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
- Local production / Worker-constructed **READY buildings** (усунено per ADR_0009). Worker leveling, foundation install/repair, and Wall construction are **not** this exclusion.

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
- Terrain / foundations — [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md), [`../TDD/16_Voxel_Terrain_And_Foundations`](../TDD/16_Voxel_Terrain_And_Foundations.md), [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md).
- Orbital Delivery ADR — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md).
