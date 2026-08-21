# Units

## MVP Unit Roster

| Unit | Type Tag | Role | Data Asset |
| --- | --- | --- | --- |
| Worker | `GP.Unit.Type.Worker` | Ferronite mining + transport до MainBase containers + repair + **local engineering** (level / foundation install / foundation repair / Wall construction) | `DA_GP_Unit_Worker` |
| Salvage Walker | `GP.Unit.Type.SalvageWalker` | Industrial defender — protects workers, repels SWARM waves, engages opposing player units | `DA_GP_Unit_SalvageWalker` |

Дві unit-категорії у MVP. Жодних support / siege / hero / specialist юнітів. SWARM units описані у [`03_Factions`](03_Factions.md) (не player-controllable).

**Усі units (Worker, Salvage Walker) ACQUIRED via orbital unit delivery, не produced** (per [ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md), [`10_Orbital_Delivery`](10_Orbital_Delivery.md)). Local production усунено. Старт: MainBase + 2 Workers pre-deployed; додаткові units — Unit Order **manifest** → DropPod → MainBase **Unit Drop Zone** (не free world placement). Packing uses **Transport Slots** (pod capacity ≠ MaxUnits).

Per Pillar 2 (Engineer, Not Soldier) і Pillar 7 (Simple Machines, Strong Readability) з [`01_Game_Pillars`](01_Game_Pillars.md). Жодних military silhouettes, hero units або humanoid combat anims.

## Common Capabilities

Усі MVP units:

- Replicate health через `UGP_UnitAttributeSet`.
- Виконують команди через `UGP_CommandComponent` (отримують server commands; не слухають input напряму).
- Помирають (Health ≤ 0) → server тригерить death state → multicast cosmetic.
- Атакують deterministic HP-based (без RNG, без damage type vs armor type у MVP).
- Visual identity — industrial machinery, не military equipment.

## Worker

### Definition (Data Asset)

`DA_GP_Unit_Worker`:

- `DisplayName`: "Worker"
- `UnitType`: `GP.Unit.Type.Worker`
- `DropCost`: TBD Orbital Ferronite (per-unit cost in unit manifest; balance pass)
- `TransportSlotCost`: **1** (MVP tuning example; DataAsset-driven, not immutable)
- `MaxHealth`: 50
- `MoveSpeed`: 350 cm/s
- `CarryCapacity`: 50 Ferronite (визначає, скільки raw Ferronite worker несе за один trip у MainBase containers; TBD)
- `MineRatePerSecond`: 10 (TBD; також live у `DA_GP_Resource_Ferronite.MineRatePerWorker` — final tuning у одній з пар, не дублювати).
- `RepairCost`: TBD (per repair tick — застосовується через `GE_GP_Cost_RepairTick`)
- `AllowedCommands`: `GP.Command.{Move, Stop, Mine, Repair}` today. **Future:** Level Terrain / foundation install / foundation repair / Wall construction assignment (exact tags TBD; see [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md)).
- `GrantedAbilities`: Repair. Worker does **not** build READY orbital buildings. Worker **does** perform field engineering / local Wall construction (level terrain, install/repair foundation, construct Walls). Exact future command tags TBD.
- `UnitTags`: `GP.Unit.Type.Worker`, `GP.Faction.Corporate`

### Identity

Industrial mining drone на гусеничному шасі. Drilling arm для видобутку, cargo hopper на задній частині, repair / utility arm для ремонту. Жодного humanoid силуету. Animation budget — обертання бура, hydraulics руху, conveyor зачерпування cargo.

Reads з top-down camera як excavator-class mining rig. Не tank, не infantry, не mech.

### Behavior

Role — **mining + transport + repair + site engineering**:

- Видобуває з `Ferronite Deposit` (`UGP_MiningComponent`). Деталі — [`06_Resources`](06_Resources.md).
- Несе raw (Planetary) Ferronite до MainBase containers; drop-off наповнює container і піднімає `FerroniteThreatValue` (per [`06_Resources`](06_Resources.md) §Container System).
- Ремонтує own-team damaged buildings / units (`GP.Command.Repair`, GAS-driven). Деталі — нижче §Repair.
- **Does not construct/build the actual READY building.** READY buildings still arrive complete from orbit. There is no Barracks / factory / local building-production queue.
- **Does** later perform **local engineering** on planned jobs: **level terrain**, **install already-delivered Foundation Slab material** (progressive per-cell labor), **repair damaged foundation**, and **construct Walls** from delivered Wall Package stock. Plan first; work starts only after assigned Workers reach valid positions; multiple Workers accelerate (formula TBD). Generic work-presentation pulses follow the mining pattern (gameplay events, Blueprint Niagara). See [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md), [ADR-0010](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md).
- **Не атакує** у MVP. Це навмисне обмеження — workers є м'якою ціллю для SWARM waves, що стимулює оборону.
- Уразливий до SWARM атак — низький HP, без attack response.

### Repair

- Issued як `GP.Command.Repair` на own-team damaged target (building або unit, `Health < MaxHealth`).
- Server-validated: target own-team, target damaged, worker у repair range.
- Worker рухається до target → repair tick застосовує heal + cost через `GE_GP_Cost_RepairTick` (repair cost — TBD у balance pass).
- Завершується на full heal, target loss, або player cancel.
- Repair НЕ є build / construction READY будівлі — це відновлення вже-deployed asset. Terrain leveling / foundation install / Wall construction are separate **local engineering** jobs and also do **not** spawn Logistics Hub / Turret.

### Visual / Animation Budget

- Tracked chassis (гусениці) — looped animation.
- Drill arm spin (mining state) — single rotating mesh.
- Hydraulic arm cycle (repair state) — 2-3 keyframe animation (welding / patching pass).
- Idle state — minimal (light flicker, ambient hum, hydraulic settling).

Per Pillar 7 — animation budget низький, всі motion = mechanical primitives.

## Salvage Walker

### Design Intent

Salvage Walker — primary defender MVP. **Не military mech.** Це переобладнана шахтарська платформа з retrofitted defensive arm — застаріла excavator / heavy salvage rig, оснащена industrial-derived weapons (repurposed plasma cutter, salvaged riveter як rapid-fire driver). Industrial silhouette, exposed mechanisms, welded armor patches.

Архетип gameplay — **heavy durable defender** (попередньо описаний як "tank archetype"; identity змінена per Pillar 2, gameplay role зберігся). Високий HP, помірний damage, ближній / середній діапазон, повільніший рух.

Призначення:

- Захист бази від SWARM waves.
- Escort workers під час mid-match expansion до додаткових Ferronite Deposits.
- Engagement з ворожою армією у direct combat (industrial brawl, не military skirmish).

Чому одна defender unit у MVP: одна combat unit з clear industrial identity (durable salvage rig) краща, ніж два mid-tier units з overlapping ролями. Trade-off — гравець не має варіантів combat composition, але це MVP-ціль.

### Naming Note

"Salvage Walker" — production naming. Lore framing — це **переобладнаний heavy mining walker з combat retrofit**, який корпорація розгорнула, бо не повезла достатньо armed escort. Кожен Salvage Walker — це industrial rig з зварними патчами і repurposed зброєю, не frontline military vehicle.

Альтернативні робочі назви, відхилені:

- "Trooper" — military-coded, порушує Pillar 2.
- "Tank" — direct military read.
- "Mech" / "Battlewalker" — Battletech / Gundam-style military silhouette association.
- "Sentinel" — занадто generic / military.

### Definition (Data Asset)

`DA_GP_Unit_SalvageWalker`:

- `DisplayName`: "Salvage Walker"
- `UnitType`: `GP.Unit.Type.SalvageWalker`
- `DropCost`: TBD Orbital Ferronite (per-unit cost in unit manifest; balance pass)
- `TransportSlotCost`: **2** (MVP tuning example; DataAsset-driven, not immutable)
- `MaxHealth`: 200 (TBD; durable tier — приблизно 4× Worker HP)
- `Armor`: 0 (MVP — без armor type system; raw HP)
- `MoveSpeed`: 250 cm/s (TBD; повільніше за Worker для heavy industrial feel)
- `Damage`: 20 per hit (TBD)
- `AttackRange`: 600 cm (TBD; short-medium baseline — exact value у Data Asset)
- `AttackSpeed`: 1 attack/s (TBD; 1 hit per second)
- `AllowedCommands`: `GP.Command.{Move, Stop, Attack}`
- `GrantedAbilities`: (none у MVP — attack — це behavior через `UGP_CombatComponent`, не explicit ability)
- `UnitTags`: `GP.Unit.Type.SalvageWalker`, `GP.Faction.Corporate`

### Behavior

- Move + Attack (basic).
- Auto-target у attack range через `UGP_TargetingComponent`. Pri target — SWARM > enemy player unit > enemy building (TBD у targeting priority pass).
- Explicit Attack command тригерить engage конкретного target.

### Visual / Animation Budget

- Treaded chassis OR quadruped industrial walker (final form factor TBD у concept art pass — обидва industrial-fit).
- Retrofitted defensive arm: repurposed plasma cutter, salvaged riveter, або mining laser з combat mode. Visible "this is repurposed equipment" detail (welded mounts, exposed cabling, makeshift coolant lines).
- Welded patch armor — segmented, irregular, з danger striping.
- Animation: idle hydraulic settling, attack — kickback з recoil, movement — tread / leg cycle.
- Кожна моторика — mechanical primitive (rotation, piston cycle), не cinematic body motion.

Per Pillar 7 — animation budget помірний; жодних унікальних juggernaut anim sets, всі motion modular.

## Combat Resolution Model

MVP — **deterministic HP-based damage**:

```
DamageDealt = Attacker.Damage   (constant, без RNG)
TargetHealth -= DamageDealt
```

Без armor type matchup, без random rolls, без crit chance. Це найпростіше для:

- Server validation (deterministic, no desync).
- UI feedback (predictable HP bars).
- Balance tuning (один axis — number).

Post-MVP — armor type vs damage type table, possible crit chance, optional RNG. У `Backlog`.

## Attack Range Baseline

Range live у Data Assets. Baseline values для проектування:

| Tier | Approximate Range (cm) | Use Case |
| --- | --- | --- |
| Melee | 150 | Не у MVP. |
| Short-medium | 600 | Salvage Walker (MVP default). |
| Long | 1200 | Не у MVP (sniper / siege архетипи — Backlog). |

Salvage Walker стартує з `AttackRange: 600`. Уточнюється у playtest, value live у `DA_GP_Unit_SalvageWalker`.

## Acquisition (Unit Delivery)

Units **не виробляються локально.** Additional units arrive via **Unit Delivery**:

- Старт: MainBase + 2 Workers pre-deployed (per `DA_GP_Faction_Default.StartingUnits`).
- Додаткові Workers / Salvage Walkers: Unit Order **manifest** → spend `OrbitalFerronite` once → DropPod → **MainBase Unit Drop Zone** (no free world placement).
- Transport packing: `sum(count × TransportSlotCost) <= PodTransportSlotCapacity` (MVP examples: Worker **1**, Salvage Walker **2**, pod **4**).
- `MaxUnits` / `CurrentUnits` is a **separate** army-cap gate; reject the **full** manifest if over cap (no silent partial fill).

Деталі — [`10_Orbital_Delivery`](10_Orbital_Delivery.md), [`05_Buildings`](05_Buildings.md).

## Unit Cap (Capacity)

Per Pillar 4 (Capacity Is Strategy) — unit capacity це **strategic resource**, не пасивний ліміт. **Do not conflate** with pod **Transport Slots**.

`AGP_PlayerState.ASC` має `UGP_PlayerAttributeSet.MaxUnits` (default 5 на старті).

Кожна Logistics Hub додає +5 до `MaxUnits` через `UGameplayEffect` (`GE_GP_UnitCap_Plus5`) on **deploy landing**. Кожне розширення capacity — Ferronite spend at **purchase** that does not add FerroniteScore. Tradeoff явний.

`UGP_PlayerAttributeSet.CurrentUnits` — replicated, інкрементується при spawn, декрементується при death.

Якщо complete unit manifest would exceed free MaxUnits room — order **rejected** with UI feedback (MVP: no partial fill).

## Movement Constraint

- Movement — server-authoritative (server position + client interpolation). У MVP — без client-side prediction (per `TDD/00_Technical_Overview` authority map).
- Mining flow (worker → deposit → main base → deposit) — chain command, що auto-issued від `UGP_MiningComponent`. Гравець не мусить кожен trip розписувати.

## Out of MVP

- Specialist combat units (sniper, siege, anti-armor, anti-air).
- Healer / support unit.
- Hero unit.
- Veterancy / experience system.
- Unit upgrades (tier 2 Salvage Walker, тощо).
- Armor type / damage type matchup table.
- Worker з attack capability.
- Air units.

Усі — у `Backlog`.

## Hard Constraints (Pillar-Enforced)

Будь-яке додавання unit type **повинно проходити перевірку** per [`gp-mechanics-validator`](../../SKILLS/gp-mechanics-validator/SKILL.md):

- Visual identity — industrial / engineering / mining-derived. Не military, не creature-like, не humanoid soldier.
- Animation budget — mechanical primitives. Не cinematic skeletal sets.
- Combat role — defensive / industrial defense, не frontline assault як primary fantasy.
- Gameplay role — узгоджена з extraction-first loop, не military RTS metagame.

## References

- Unit architecture — [`../TDD/05_Unit_Architecture`](../TDD/05_Unit_Architecture.md).
- GAS — [`../TDD/02_GAS_Architecture`](../TDD/02_GAS_Architecture.md).
- Commands — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md), [`../TDD/04_RTS_Selection_And_Commands`](../TDD/04_RTS_Selection_And_Commands.md).
- Mining mechanic — [`06_Resources`](06_Resources.md).
- Terrain leveling / foundations — [`13_Terrain_Engineering_And_Foundations`](13_Terrain_Engineering_And_Foundations.md).
- SWARM units (non-player) — [`03_Factions`](03_Factions.md).
- Visual style — [`Lore_Setting`](Lore_Setting.md).
- Pillars — [`01_Game_Pillars`](01_Game_Pillars.md).
