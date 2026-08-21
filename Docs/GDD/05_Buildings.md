# Buildings

## MVP Building Roster

| Building | Source | Role | Footprint | Data Asset |
| --- | --- | --- | --- | --- |
| Main Base | Initial deployment (pre-placed at match start) | Container storage + ship-to-orbit launch + worker drop-off zone + **Unit Drop Zone** (authored unit landing pad) + sight source | 5×5 | `DA_GP_Building_MainBase` |
| Logistics Hub | **Orbital drop** | +5 MaxUnits + expanded container cap + sight source | 4×4 | `DA_GP_Building_LogisticsHub` |
| Defensive Turret (free-standing) | **Orbital drop** | Static defense vs SWARM / enemy units + sight source | 4×4 | `DA_GP_Building_DefensiveTurret` |
| Wall segment | **Wall Package → MainBase inventory → Build Wall** | Defensive perimeter — auto-connects 8-dir; hosts wall-mounted turret | 2×2 | `DA_GP_Building_Wall` |
| Wall-mounted Turret | Built on Wall segment | Constrained-footprint defense node mounted on wall | 2×2 | `DA_GP_Building_WallTurret` |
| Ferronite Deposit (environment) | Map placement (level-spawned) | Mining source — natural resource node | 3×3 | `DA_GP_Building_FerroniteDeposit` |

Шість building entity-типів у MVP (4 player-orderable + 1 wall-mounted variant + 1 environment). Без research, без supply, без upgrade tiers, без faction-unique buildings.

**Pivot note (2026-05-16):** усе крім Main Base і Ferronite Deposit **originates from orbit as material or completed assets** (per ADR-0009 і [`10_Orbital_Delivery`](10_Orbital_Delivery.md)). Worker не **конструює** READY будівлю локально. Worker **does** later perform local engineering: level terrain, install/repair foundation, construct Walls ([`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md)). "Assembly Yard" → "Logistics Hub". Wall **Package** uses DropPod; `AGP_Wall` segments are field-constructed from MainBase inventory by Workers (GP-0305R + ADR-0010).

**Owner refinement (2026-08-08):** Most buildings are **Purchased** into **orbital READY inventory** (Orbital spend at purchase), then **Deployed** later via ghost placement (DropPod to confirmed location; **no second spend**). Buildings do **not** land in the MainBase Unit Drop Zone (that pad is for **units** only). Shared DropPod/rocket visual with unit deliveries.

**Owner refinement (2026-08-18, GP-0305R):** Wall is **not** a READY building. Player buys a **Wall Package of 5** (one rocket to MainBase). **Build Wall** then places already-delivered segments from MainBase inventory (max 5, no stacking). No per-segment rocket. No second Orbital spend. See [`10_Orbital_Delivery`](10_Orbital_Delivery.md) and [`../Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md`](../Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md).

**Owner refinement (2026-08-21, ADR-0010):** Normal player-deployed buildings require **leveled terrain + intact per-cell foundation** before DropPod deploy. Foundation stock uses Wall Package philosophy (orbit → MainBase inventory → Worker install job; quantity/cost/footprint **TBD**, not automatically 5). Foundation does not appear instantly on click. Initial MainBase is the authored starter-site exception. **Wall Foundation Rule — RESOLVED:** Wall segments do **not** require Foundation. They are locally constructed by Workers on terrain. Terrain suitability remains TBD.

Per Pillar 2 (Engineer, Not Soldier) і Pillar 7 (Simple Machines, Strong Readability) з [`01_Game_Pillars`](01_Game_Pillars.md). Усі buildings — industrial / engineering visual identity. Жодних military bunkers, fortress towers, або command centers з військовою aesthetic.

## Build Grid System

**Усі будівлі snap-аються до сітки.** Це не free-form placement.

| Property | Value |
| --- | --- |
| Cell size | 2 m (200 cm у world) |
| Reference | StarCraft 2 tile equivalent |
| Snap | axis-aligned (no rotation, except wall auto-orient) |
| Origin | world (0,0) projected на NavMesh |

### Footprint Reference (SC2-Calibrated)

Розміри побудовані по аналогії з StarCraft 2:

| Entity | Cells (footprint) | SC2 equivalent | Why this size |
| --- | --- | --- | --- |
| Unit (Worker / Salvage Walker) | 1×1 | SCV ~0.625 — округлено до 1×1 для grid | Найменша одиниця |
| Wall segment | 2×2 | Supply Depot 2×2 | Wall-able choke структура |
| Wall-mounted Turret | 2×2 | (constrained до wall cell) | Має сидіти на wall segment |
| Defensive Turret (free-standing) | 4×4 | Більше за SC2 Missile Turret 2×2 / Bunker 3×3 — у GP це **substantial fortification**, не дешева scout structure | Якщо ставиш окремо, це повноцінна вогнева точка |
| Ferronite Deposit | 3×3 | Vespene Geyser 3×3 | Природний resource node |
| Logistics Hub | 4×4 | Hatchery 4×4 | Logistics expansion node |
| Main Base | 5×5 | Command Center / Nexus 5×5 | Головна expedition pod |

Footprint у DA: `UGP_BuildingDefinition.FootprintCells : FIntPoint`.

### Placement Validation

Кожна спроба drop-нути **нормальну** orbital будівлю проходить cell-by-cell validation на сервері:

1. **Cell occupancy:** усі N × M cells вільні (не зайняті іншою будівлею).
2. **Clearance:** для structures з `ClearanceCells > 0` — навколо footprint у `ClearanceCells` радіусі (Chebyshev distance) має бути free space.
3. **NavMesh navigable:** під footprint є walkable surface (dynamic terrain nav strategy is later TECH-SPIKE).
4. **FoW visible:** drop target у Actively Visible зоні (per [`11_Fog_of_War`](11_Fog_of_War.md)).
5. **Leveled terrain:** кожна footprint cell sufficiently level (tolerance **TBD**; grey/yellow site-prep language in [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md)).
6. **Intact foundation:** кожна footprint cell has installed, undestroyed foundation coverage. Foundation is **per BuildGrid cell**, not one all-or-nothing slab actor.
7. **Resources OK:** player має READY inventory (Orbital already spent at Purchase).

Initial MainBase is **not** gated on player-leveled foundation (authored starter site; implementation deferred).

**Wall Foundation Rule — RESOLVED:** Wall segments do **not** use this foundation rule. Wall-mounted Turret follows Wall and does not independently require ground Foundation. Wall still needs terrain-suitability validation (slope / visual adapt / auto-level vs manual / voxel base interaction = **TBD**).

Cells, що знаходяться під будівлями — **blocked для unit pathing** (NavMesh modifier).

### Grid Visualization

Коли player у drop-targeting mode:
- HUD показує semi-transparent grid overlay у области cursor.
- Footprint reticle (зелений valid / червоний invalid) over the cells, які будуть зайняті.
- Clearance zone — outline тонкою лінією навколо footprint (для wall — 2 cells exclusion).

Grid overlay прихований за default — з'являється тільки при drop-targeting **and** (later) Level Terrain site-prep mode. Leveling overlay uses **GREY** (already level) / **YELLOW** (needs leveling) per cell. Zone sizing UX is **DESIGN REQUIRED**.

## Architectural Note: Building-as-Pawn

Усі будівлі — це `AGP_BuildingBase`, що є child від `AGP_UnitBase`. Це означає:

- Спільні primitives: ASC, `UGP_UnitAttributeSet` (Health), gameplay tags, replication setup, death/destruction flow.
- Building-specific поведінка — через components (`UGP_StorageComponent`, `UGP_TargetingComponent`, `UGP_CombatComponent`).
- `GP.Unit.Type.Building` tag диференціює behavior там, де треба (наприклад, targeting priority).

Це **composition-driven** архітектура. Глибока inheritance заборонена — building-specific logic не йде у base class, а у компоненти. Деталі — [`../TDD/06_Building_Architecture`](../TDD/06_Building_Architecture.md). Якщо owner хоче зробити цей pattern explicit, fix-ається через ADR_0007 (Tracked у `Claude_Task_Backlog`).

## Common Capabilities

Усі MVP buildings:

- Мають `UGP_BuildingDefinition` Data Asset.
- Replicate health через `UGP_UnitAttributeSet`.
- Server-spawn only (orbital DropPod, initial deployment, or **Build Wall** inventory consume for `AGP_Wall`). Wall segments are not DropPod payloads.
- Можуть отримувати damage. При `Health <= 0` → destroyed → server тригерить destruction state → multicast VFX.
- Учасники SWARM aggro (SWARM waves цілять usable assets, building destruction скорочує оборону гравця).
- Sight source (різний `SightRadius` per type) — contribute до FoW visibility per [`11_Fog_of_War`](11_Fog_of_War.md).

## Main Base

### Definition

`DA_GP_Building_MainBase`:

- `DisplayName`: "Main Base"
- `BuildingType`: `GP.Unit.Type.Building`
- `BuildingRole`: `GP.Building.Role.Command`
- `Source`: initial deployment at match start (pre-placed per map). Не buildable / not orderable.
- `MaxHealth`: TBD balance.
- `UnitTags`: `GP.Unit.Type.Building`, `GP.Building.Type.MainBase`, `GP.Building.Role.Command`, `GP.Faction.Corporate`.
- `StorageComponent`: enabled (Container System per [`06_Resources`](06_Resources.md) — приймає ferronite drop-off, агрегує у containers, ships до орбіти при full).
- `DropOffRange`: TBD cm (worker auto-drop у цій зоні).
- `SightRadius`: wide (татичний overview around landing site).

### Behavior

- Гравець з'являється на map з 1 Main Base + N стартових Workers (per `DA_GP_Faction_Default.StartingUnits`).
- Initial MainBase begins on an authored / prepared starting site / starter foundation. Player is **not** required to level terrain first. Exact starter-foundation implementation is **deferred** ([`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md)).
- **Не виробляє Workers.** Workers arrive via orbital drop (per [`10_Orbital_Delivery`](10_Orbital_Delivery.md)).
- Приймає Ferronite drop-off від Workers (див. [`06_Resources`](06_Resources.md) §Container System). Raw drop-off піднімає `AGP_GameState.FerroniteThreatValue` (stored-at-base stock). Container fills → launch до орбіти → `GE_GP_AddOrbital` (+`OrbitalFerronite`) + `GE_GP_AddScore` (+`FerroniteScore`); launch знижує `FerroniteThreatValue`.
- Якщо Main Base знищено → **immediate annihilation loss** (за `bAnnihilationCountsAsWin`, default true). Без MainBase containers ship не може, нова Orbital Ferronite не надходить → no path до victory. Opponent wins. Деталі — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).

### Visual / Style

Landing capsule / mobile command rig. Жовто-чорні danger striping, помаранчеві signal beacons. Конструктивно — це не permanent fortress, а deployable industrial outpost.

## Logistics Hub

**Renamed from "Assembly Yard"** (2026-05-16). "Assembly Yard" implied local on-site assembly — that's no longer accurate з orbital model. "Logistics Hub" відображає actual function: **expands shipping pipeline** + grants unit cap.

### Definition

`DA_GP_Building_LogisticsHub`:

- `DisplayName`: "Logistics Hub"
- `BuildingType`: `GP.Unit.Type.Building`
- `BuildingRole`: `GP.Building.Role.Logistics`
- `Source`: **orbital purchase → READY → deploy**. Cost in Orbital Ferronite paid at **Purchase**. Player later places via ghost; DropPod delivers to confirmed location.
- `BuildTime`: 0 (no construction phase — drop pod arrives, building operational immediately).
- `MaxHealth`: TBD balance.
- `UnitTags`: `GP.Unit.Type.Building`, `GP.Building.Type.LogisticsHub`, `GP.Building.Role.Logistics`, `GP.Faction.Corporate`.
- `GameplayEffects`: `GE_GP_UnitCap_Plus5` applied immediately on landing → adds +5 to `UGP_PlayerAttributeSet.MaxUnits`.
- `MaxContainerCount` bonus: +N (TBD balance — adds shipping capacity до owning player's MainBase Storage).
- `GE_RemovalOnDestroy`: cap effect (-5) і container cap bonus removed when destroyed.
- `SightRadius`: medium.

### Behavior

- Player **Purchases** Logistics Hub (Orbital spend → READY inventory).
- Later **Deploys** READY via ghost onto **leveled + intact foundation cells** (when the Terrain stage exists) → DropPod (2–3 s; **no second spend**).
- Building operational on landing; unit-cap / container bonuses apply.
- +5 MaxUnits available instantly.
- +N MaxContainerCount applied to owning MainBase's StorageComponent.

### Trade-Off

Спускати Logistics Hub витрачає significant Orbital Ferronite (cost balanced щоб бути impactful, не trivial). Player chooses між:

- Direct production drops (Workers, Salvage Walkers) — immediate fighting / mining capacity.
- Logistics Hub — capacity expansion, but longer-term return (cap для майбутніх drops + більше shipping throughput).

### Visual / Style

Modular industrial logistics node: heavy crane gantry, exposed conveyor system, cargo lift, multi-tier loading dock. Welded modular panels, danger striping, industrial signal lights, container loading rails. Reads як прискорена field-deployed shipping facility, не military barracks.

### Naming History

- Phase 1: "Barracks" — rejected per Pillar 2 (military lexicon).
- Phase 2: "Assembly Yard" — replaced 2026-05-16 (didn't fit orbital model; nothing is "assembled" on-site).
- **Phase 3 (current): "Logistics Hub"** — accurate to function: extends shipping pipeline + cap.

## Wall

### Design Intent

Wall — це **захисна периметральна структура**. Замикає choke points, формує defensive corridor, hosts wall-mounted turrets для economy fortification. Без walls — open-field SWARM rush легко добирається до Workers. Wall — це **інженерне рішення** проти organic threat: ти не оборонюєшся армією, ти будуєш периметр.

**`Wall Foundation Rule — RESOLVED:`** Wall segments do **not** require Foundation Slabs. They are locally assembled from delivered Wall Package stock by Workers and may be constructed directly on terrain. Wall-mounted Turret follows the Wall system. Terrain suitability (max slope, visual adapt, auto-level vs player-level, voxel base interaction) is **TBD / DESIGN REQUIRED**.

### Definition

`DA_GP_Building_Wall`:

- `DisplayName`: "Wall Segment"
- `BuildingType`: `GP.Unit.Type.Building`
- `BuildingRole`: `GP.Building.Role.Defense`
- `Source`: **placed from MainBase Wall inventory** after a Wall Package delivery. Package Cost is Orbital Ferronite on `DA_GP_WallPackage` (TBD balance). Placement does **not** spend Orbital again.
- `FootprintCells`: 2×2.
- `ClearanceCells`: 0 для wall-to-wall (walls конектяться), але cell-to-other-structure clearance = 2.
- `MaxHealth`: TBD balance (нижче за Turret, але стіну легко відремонтувати / перебудувати).
- `UnitTags`: `GP.Unit.Type.Building`, `GP.Building.Type.Wall`, `GP.Building.Role.Defense`, `GP.Faction.Corporate`.
- `SightRadius`: 0 (walls не contribute vision — економно).
- `bCanHostWallTurret`: true.

### Behavior

#### Auto-Connect (8-Direction)

При placement / destroy — wall segment запитує свої 8 сусідів (N, NE, E, SE, S, SW, W, NW). Bitfield (8 bits) → визначає visual state:

- **End** — тільки 1 сусід.
- **Straight horizontal / vertical** — 2 сусіди на одній лінії.
- **Corner** (4 variants) — 2 сусіди під 90°.
- **T-junction** (4 variants) — 3 сусіди.
- **Cross** — 4 сусіди.
- **Standalone** — 0 сусідів.

Total ~16 distinct visual states. Implementation: material parameter або mesh-swap по bitfield. Деталі — [`../TDD/06_Building_Architecture`](../TDD/06_Building_Architecture.md) §Wall System.

#### Acquisition (Wall Package)

1. Order Menu **Buy Wall Package** (available when MainBase Wall stock is **0..4** and no package is in flight).
2. Spend full package `Cost` once. One rocket delivers the package to MainBase **UnitDropZone**. No placement mode.
3. Arrival: stock becomes `min(5, current + 5)`. Excess segments are wasted. Depot shows remaining blocks. Cannot buy at stock 5.
4. **Build Wall** becomes available while stock > 0.

#### Drag-Build (from inventory) — planned local engineering

Player presses **Build Wall** (not an orbital purchase):

1. LMB-press на start cell A (валідний grid cell з clearance OK; terrain suitability rules **TBD**).
2. Hold + drag — preview path:
   - A* on BuildGrid (server-side).
   - Free cells with clearance OK only.
   - Path length **cannot exceed** current Wall inventory.
3. LMB-release — confirm the **plan**:
   - Server validates cells, clearance, existing wall rules, terrain suitability (when specified).
   - A planned construction site / job exists. Construction does **not** progress automatically.
   - Exact Wall inventory consume / reserve moment is **DESIGN REQUIRED**.
4. Player assigns one or more Workers. Workers travel to valid work positions.
5. Work begins only when at least one assigned Worker is in a valid work position. Multiple Workers accelerate (formula TBD).
6. On completion, the final Wall segment(s) become operational. Auto-connect when implemented.
7. RMB / Esc on an unstarted plan — cancel; do not invent refund/reservation until consume moment is designed.

No per-segment DropPod. No `PathLength × WallSegmentCost`. No Building READY decrement. **No Foundation required.**

Single-click plan of one segment from inventory is allowed if stock ≥ 1 (same planned-job rule, not instant operational spawn).

#### Clearance Rule — "2 Cells Away"

Wall cell **не може бути placed within 2 grid cells** (Chebyshev distance) of:

- Будь-якої power building (Main Base, Logistics Hub, Defensive Turret, Wall-mounted Turret).
- Resource node (Ferronite Deposit).
- Enemy building (visible only — per FoW).

**Exception:** wall-to-wall з'єднання дозволено (walls touch each other without clearance).

Reason: prevents wallhugging-fortification (encasing MainBase у impassable wall ring). Forces player думати про perimeter design — стіни ставляться **поза** base, формуючи defensive corridor.

### Trade-Off

Walls — це **economic insurance**:
- Bought as a **5-segment package** (one Orbital spend); each placed cell spends inventory, not Orbital. Long perimeters need more packages over time (max stock 5, no stacking).
- Не contribute vision (gap у sight pyramid — wall blind spot).
- Можна обійти navigation-wise, якщо є open path (wall працює як NavMesh modifier, не magic barrier).

Player decides між:
- Тонка wall line + active defense (mobile Salvage Walker).
- Густа wall + wall-turrets + heavy fortification (passive defense, але less mobile income).

### Visual / Style

Industrial salvage walls: переобладнані cargo containers, welded plates з danger striping, exposed wiring, occasional warning lights. **Не military bunker walls.** Reads як industrial perimeter fencing, not fortress wall. Auto-connect visual must read clearly з top-down camera — corner joints у different angle helps disambiguate.

### Wall-Mounted Turret

`DA_GP_Building_WallTurret`:

- `DisplayName`: "Wall Turret"
- `BuildingType`: `GP.Unit.Type.Building`
- `BuildingRole`: `GP.Building.Role.Defense`
- `Source`: **orbital drop** (player picks specific wall cell to mount).
- `FootprintCells`: 2×2 (constrained to single wall cell).
- `ClearanceCells`: 0 (sits on wall).
- `MaxHealth`: TBD balance (нижче за free-standing Turret 4×4).
- `AttackRange / Damage / AttackSpeed`: TBD balance (typically: shorter range, lower DPS than free-standing — compensated by лower cost і wall protection).
- `UnitTags`: `GP.Unit.Type.Building`, `GP.Building.Type.WallTurret`, `GP.Capability.WallMountable`.

**Placement constraint:** can be placed only on existing wall cell. Drop target validation:
1. Cell occupied by `AGP_Wall`?
2. That wall cell не already has wall-mounted turret?

Якщо ні — reject `EReason::NotOnWall`.

**Visual:** sits on top of wall mesh. Wall mesh underneath залишається; turret mesh додає upward. If wall destroyed — wall-turret теж destroyed (it's structurally attached).

### Naming Note

"Wall" — direct functional name. Не "barricade", не "fortification", не "rampart" (всі мають military lexicon з ним). "Wall" як industrial neutral term.

## Defensive Turret

### Design Intent

Defensive Turret — primary oborona проти SWARM waves і opponent harass. Necessary tool у MVP, де гравець мусить балансувати economy (Workers + shipping) і survival (Turrets + Salvage Walker coverage). Без Turret — SWARM waves тривіально знищують undefended base.

### Definition

`DA_GP_Building_DefensiveTurret`:

- `DisplayName`: "Defensive Turret"
- `BuildingType`: `GP.Unit.Type.Building`
- `BuildingRole`: `GP.Building.Role.Defense`
- `Source`: **orbital purchase → READY → deploy**. Cost in Orbital Ferronite at **Purchase**. Deploy later via ghost / placement.
- `BuildTime`: 0 (no construction phase).
- `MaxHealth`: TBD balance (нижче за Logistics Hub, бо turret — disposable defense).
- `UnitTags`: `GP.Unit.Type.Building`, `GP.Building.Type.DefensiveTurret`, `GP.Building.Role.Defense`, `GP.Faction.Corporate`.
- `AttackRange`: TBD balance.
- `Damage`: TBD balance.
- `AttackSpeed`: TBD balance.
- `TargetingPriority`: SWARM > enemy units > enemy buildings (TBD у TDD targeting pass).
- `UPROPERTY components`: `UGP_TargetingComponent`, `UGP_CombatComponent`.
- `SightRadius`: medium.

### Behavior

- Player **Purchases** Defensive Turret (Orbital spend → READY).
- Later **Deploys** via ghost (typically choke / deposit / contested) onto prepared foundation (when the Terrain stage exists) → DropPod; **no second Orbital charge**.
- FoW/grid validation applies at **deploy** when those systems exist.
- Drop pod descent telegraph.
- Static — після приземлення не рухається.
- Auto-engages targets у `AttackRange` через `UGP_TargetingComponent`.
- Не вимагає supply / ammo у MVP.
- Не може отримати команди від player (fire-and-forget after drop).

### Trade-Off

Turret витрачає Orbital Ferronite, що не йде у наступні drops (Workers / Walkers). Гравець постійно вирішує — додаткова Turret (більше оборони, менше mining / acquisition capacity), чи додатковий Worker (більше mining, але швидше росте FerroniteThreatValue при hoarding).

### Visual / Style

Static defense node: tripod або quadruped mounting на industrial base plate, з repurposed plasma cutter або salvaged riveter-driver як projectile system. Рясно покритий welded patches і warning stripes. **Не military autocannon** — це переобладнаний industrial tool з combat conversion. Reads як salvage defensive node, не як bunker turret.

### Naming Note

"Defensive Turret" — production naming зберігається, бо "turret" як industrial term допустимий (Pillar 7 явно дозволяє "прості turret-like захисні вузли"). Visual identity, однак, **industrial defensive node**, не military bunker turret.

## Ferronite Deposit (Environment)

### Definition

`DA_GP_Building_FerroniteDeposit`:

- `DisplayName`: "Ferronite Deposit"
- `BuildingType`: `GP.Resource.Node` (під-тип Building у TDD; primary tag — resource node, не "будівля" у gameplay sense).
- `Source`: map placement only (not orderable).
- `MaxHealth`: not destructible у MVP. Альтернатива — destructible з high HP, post-MVP.
- `UnitTags`: `GP.Building.Type.FerroniteDeposit`, `GP.Resource.Node`, `GP.Resource.Type.Ferronite`.
- `MaxCapacity`: TBD balance, варіюється per placement.
- `MineRatePerWorker`: TBD balance.
- `MaxConcurrentWorkers`: 4 + queue (per [`../TDD/07_Resource_Architecture`](../TDD/07_Resource_Architecture.md)).
- `DepletedBehavior`: Destroy actor (per GP-0303 decision).
- `SightRadius`: 0 (not a vision source).

### Behavior

- Worker `GP.Command.Mine` → Worker рухається до deposit → mining tick.
- Mining tick додає ferronite до Worker's `CarriedFerronite`. Worker автоматично повертається до Main Base при capacity hit або при deposit depletion (auto-cycle per GP-0302).
- `CurrentCapacity` decrement до 0 → deposit destroyed (per DepletedBehavior).

Деталі — [`06_Resources`](06_Resources.md).

## Sell + Demolish (Lifecycle End)

Будівлі мають два різних способи зникнути за player choice:

### Sell — для будівель

- Кнопка **Sell** з'являється у `WBP_GP_HUD_SelectionPanel` building mode, коли вибрана sellable будівля.
- Hover показує preview refund (e.g., "Sell: +45 Orbital").
- Click → server validates → building destroyed → partial Orbital Ferronite refund.
- **Refund formula** (DA-driven, placeholders):
  ```
  Refund = Cost × SellRefundRate × (CurrentHealth / MaxHealth)
  ```
  Default `SellRefundRate = 0.5` (50% базової вартості). Damaged buildings refund less — anti-exploit "damage-then-sell".
- Affected on sell:
  - Cap effects (Logistics Hub +5 MaxUnits) — removed via `RemoveActiveGameplayEffectBySourceEffect`.
  - Container cap bonus — removed; existing containers stay.
  - Wall-mounted Turret sold separately — underlying wall залишається.

| Building | Sellable | Notes |
| --- | --- | --- |
| Main Base | **No** | Cannot be sold — landing pod fixed, loss = annihilation. |
| Logistics Hub | Yes | Standard sell. |
| Defensive Turret (free-standing) | Yes | Standard sell. |
| Wall-mounted Turret | Yes | Sells; underlying wall залишається. |
| Wall segment | **No** | Walls demolished, не sold (див. нижче). |
| Ferronite Deposit | **No** | Environmental, не player-owned. |

### Demolish — для стін

- Stikne кнопка **Demolish** у CommandBar коли wall selected, OR hotkey `Del` / `X`.
- Вхід у **demolish cursor mode** — cursor змінюється на demolition icon, HUD banner "DEMOLISH MODE — click walls to demolish, Esc to exit".
- Click modes:
  - Single LMB на wall cell → demolish that wall.
  - LMB-drag через wall cells → mark multiple → release → batch demolish.
- **Permanent destruction.** Жодного refund.
- Wall-mounted Turret cascades destroyed automatically (no refund per cascade).
- Adjacent walls update bitfield / mesh state (wall з'єднання визивно reshape).

### Чому Walls не sellable

- Walls arrive as a paid **package**; placed segments are already-spent material. Refund створює exploits:
  - **No-commitment defense:** place walls per wave, demolish between waves з refund → defensive flexibility without economy cost.
  - Preview/cancel does not spend; only confirmed placement consumes inventory (no pod-scout refund loop).
- Permanent demolish форсить considered placement. Per Pillar 8 — глибина від positional decision, не від undo loops.

### Engineering Note

Engineering implementation, RPC contracts, refund formula, replication rules — у [`../TDD/06_Building_Architecture`](../TDD/06_Building_Architecture.md) §Sell + Demolish System.

## Deployment Model

MVP — **orbital procurement** (no local building construction):

### Units (separate flow)

Unit Order manifest → MainBase **Unit Drop Zone**. See [`10_Orbital_Delivery`](10_Orbital_Delivery.md) / [`04_Units`](04_Units.md).

### Site preparation (Worker local engineering; not constructing the READY building)

See [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md):

1. Plan Level Terrain on a BuildGrid-aligned zone (grey / yellow per cell); assign Workers.
2. Plan Foundation installation; delivered stock available; Workers progressively install cells (consume/reserve moment **DESIGN REQUIRED**).
3. Only then deploy a normal READY building.

### Buildings

1. Open Building Order UI.
2. **Purchase** Logistics Hub / Defensive Turret / Wall Turret → `GE_GP_SpendOrbital` → READY inventory++. **Not Wall** — Wall uses Wall Package (flow C). **Not foundation** — Foundation Slab package is a separate inventory flow.
3. Click READY → **deployment mode** (semi-transparent ghost + footprint; valid/invalid tint).
4. **LMB** valid (occupancy + existing checks + leveled + intact foundation for normal buildings): consume one READY → DropPod → building operational.
5. **RMB / Esc:** cancel; READY unchanged; no refund.

Server: no double-consume READY; no duplicate spawn; **no Orbital charge on deploy**.

Foundation destruction is **per cell**. Do not destroy an entire original slab because one cell was hit. Surviving-building-after-partial-foundation-loss is **DESIGN REQUIRED**. Foundation Repair is a future Worker engineering job (tunables TBD).

Деталі — [`10_Orbital_Delivery`](10_Orbital_Delivery.md), [`../TDD/14_Orbital_Delivery`](../TDD/14_Orbital_Delivery.md), [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md).

**Eliminated** (pre-pivot): Worker channel-build of READY buildings, ConstructionComponent as a Barracks queue, construction sites that spawn Logistics Hub / Turret.  
**Not eliminated:** planned local-engineering jobs for leveling, foundation install/repair, and Wall construction (ADR-0010). These are not a factory.  
**Superseded (2026-08-08):** immediate spend-on-placement for buildings. Canonical = Purchase→READY→Deploy.  
**Ghost** returns for **building deploy only**; unit orders do not free-place.

## Out of MVP

- Research buildings.
- Standalone repair structures / repair drone bays (Worker `GP.Command.Repair` is in MVP, але standalone repair building deferred).
- Power / supply system (всі buildings free-running у MVP).
- Faction-unique buildings.
- Upgrade tiers (Logistics Hub Level 2 / Level 3).
- Resource Depot / Supply Outpost (drop-off у будь-яку будівлю окрім Main Base).
- Tech Building (передумова для advanced units).
- Air defense turret variant.
- Anti-drop intercept structures (combat against opponent's drops).
- Mobile / repositionable bases.

Усі — у `Backlog`.

## References

- Building architecture (engineering) — [`../TDD/06_Building_Architecture`](../TDD/06_Building_Architecture.md).
- Resource flow + Container System — [`06_Resources`](06_Resources.md).
- Orbital Delivery System (drop mechanics) — [`10_Orbital_Delivery`](10_Orbital_Delivery.md).
- Terrain / foundations — [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md).
- Fog of War (sight source contribution, drop zone gating) — [`11_Fog_of_War`](11_Fog_of_War.md).
- SWARM waves як defense trigger — [`03_Factions`](03_Factions.md), [`07_Match_Flow`](07_Match_Flow.md).
- Pillars — [`01_Game_Pillars`](01_Game_Pillars.md).
- Unit cap mechanic — [`04_Units`](04_Units.md).
- Visual style — [`Lore_Setting`](Lore_Setting.md).
- Architectural decisions — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md).
