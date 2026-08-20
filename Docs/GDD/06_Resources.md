# Resources

## MVP Resource Model

**Один resource type у MVP — Ferronite. Universal exchange currency.**

- Native gameplay tag: `GP.Resource.Type.Ferronite`.
- Worldbuilding rationale — [`Lore_Setting`](Lore_Setting.md).
- Player meaning — і economy currency, і одиниця score. Видобутий ферроніт відправляється на орбіту контейнерами і конвертується у бали (FerroniteScore) + spendable currency (OrbitalFerronite).

Per Pillar 3 (One Resource, Many Tradeoffs) і Pillar 4 (Capacity Is Strategy) з [`01_Game_Pillars`](01_Game_Pillars.md). Усі orbital drops оплачуються одним currency — **OrbitalFerronite** (orbital state Ferronite). Через нього гравець order-ить **все**, що прибуває з орбіти:

- Orbital drops: Worker, Salvage Walker, Defensive Turret, Wall Package, Logistics Hub (per [`10_Orbital_Delivery`](10_Orbital_Delivery.md)).
- Capacity expansion (через Logistics Hub drop — +MaxUnits + container cap).
- Score (через container ship-to-orbit conversion → FerroniteScore).
- Post-MVP — corporate doctrine purchases.

Немає on-planet production/construction — усі assets прибувають orbital drop pod-ами через Logistics Hub / Order Menu (per Orbital Delivery model). Не існує окремої "military credits" або "research points" — все через Ferronite. Це **hard constraint** Pillar 3.

### Why Single Resource

- Multi-resource economy multiplyує UI complexity, building complexity, balance complexity.
- MVP мета — validate flow (mine → drop-off → spend / score), не depth of economy.
- Лор підтримує single-resource: один цінний метал, заради якого корпорації прибули на EREBUS-9.
- Universal currency створює clean decision pressure: кожен spend = opportunity cost проти score.

### Second "Resource" — Unit Capacity

Per Pillar 4 — unit capacity (`UGP_PlayerAttributeSet.MaxUnits`) функціонує як **other strategic resource**. Гравець постійно вирішує:

- Розширити capacity (OrbitalFerronite → Logistics Hub drop → +MaxUnits + container cap) — інвестиція у roster size.
- Тримати tight roster — більше OrbitalFerronite йде у score / defense / combat drops.

Capacity не є passive cap. Це conscious strategic axis з visible OrbitalFerronite cost. Деталі — [`04_Units`](04_Units.md).

## Two-State Storage Model (Container System)

**Pivot 2026-05-16.** Ferronite живе у двох станах:

### State 1: Planetary Ferronite

Сирий ферроніт, що знаходиться на планеті.

- Видобувається Workers з deposits.
- Накопичується у **containers** на Main Base.
- **Не spendable** — це сировина, що чекає відправки.
- Vulnerable: якщо Main Base / containers знищено — planetary ferronite **втрачено повністю**.

### State 2: Orbital Ferronite

Той ферроніт, що **успішно відправлений на орбіту**.

- Stored на `UGP_PlayerAttributeSet.OrbitalFerronite`.
- **Spendable currency.**
- Дозволяє order-ити одиниці, техніку, оборонні модулі, Logistics Hub з орбіти (per [`10_Orbital_Delivery`](10_Orbital_Delivery.md)).
- Persistent для match duration (off-planet, безпечно).

### Container System

Контейнери — fixed-volume буфери на Main Base.

| Property | Behavior |
| --- | --- |
| Capacity per container | TBD balance (placeholder: 100 Ferronite per container). |
| Number of containers | TBD; baseline на Main Base (e.g., 5 slots). Logistics Hub додає +N (TBD). |
| Auto-fill | Worker drop-off → resource enters first available non-full container. |
| Auto-ship | Container full → enters Launch state (2-3 s telegraph) → ships to orbit. |
| Storage overflow | Якщо всі containers full / launching → Worker drop-off **lost** (HUD warns "Storage full"). Player мусить ship або extend cap. |

### Container Lifecycle

```
[Idle, empty]
    Worker drop-off
        ↓
[Filling, partial]
    More drop-offs
        ↓
[Filling, full → 100% → triggers Ready]
        ↓
[Ready, awaiting launch]
    Server auto-launch (head Ready container, 1 active launch at a time)
        ↓
[Launching, 2-3 s telegraph — vulnerable window]
        ↓
[Departed]
    OrbitalFerronite += Container.Volume × OrbitalConversionRate
    FerroniteScore += Container.Volume × ScoreConversionRate
    FerroniteThreatValue -= Container.Volume   // raw stock leaves base → swarm pressure DROPS
        ↓
[Idle, empty — slot reusable]
```

`FerroniteThreatValue` — це сумарний обсяг **raw Planetary Ferronite, що фізично лежить у containers на Main Base ПРЯМО ЗАРАЗ** (fluctuating stock, живе на `AGP_GameState` per player). Worker drop-off піднімає його; container launch знижує. Він — driver SWARM pressure (див. нижче). Не плутати з FerroniteScore (cumulative, monotonic) чи OrbitalFerronite (spendable).

### Greed-vs-Safety Loop

Цей buffer створює central tension. Ключовий механізм — **накопичений на базі raw Ferronite сам по собі притягує рій** (`FerroniteThreatValue`):

- **Накопичувати (hoard)** багато raw Ferronite у containers = high score potential при successful launch, АЛЕ `FerroniteThreatValue` росте → **сильніший і частіший SWARM** + vulnerable до raid (raw ferronite втрачається разом з базою).
- **Регулярно ship-ити** = launch скидає `FerroniteThreatValue` → **менше SWARM pressure** (relief/safety), resource у безпеці на орбіті, але slower bank, бо ship delay є.
- **Late-game decision:** hold container щоб накопичити більший залп балів (ризикуючи піком рою), чи ship зараз щоб зняти threat magnet?

Greed-vs-Safety: **hoard = high score potential + high swarm danger; ship = safe + scores але обнуляє threat magnet.** Гравець постійно балансує жадібність проти безпеки. Це **core risk loop** MVP per Pillar 6 (SWARM as Environmental Pressure) і Pillar 8 (Simple Core).

## SWARM Pressure Tie-In (FerroniteThreatValue)

Per [`01_Game_Pillars`](01_Game_Pillars.md) Pillar 6 — SWARM wave intensity / frequency / targeting масштабується від **`FerroniteThreatValue`** = обсягу raw Planetary Ferronite, що фізично зберігається у Main Base containers ПРЯМО ЗАРАЗ. Це fluctuating stock, не accumulator. Рій реагує на сирий метал, що лежить незахищеним на поверхні.

> **Roadmap gate (2026-08-20):** SWARM is MVP's final gameplay implementation stage, separate from the
> RTS AI Opponent. A dedicated design/reconciliation review is mandatory first. Curve names and wave
> examples below express the established threat relationship, not approved final roster, spawning,
> director, targeting, authority, replication, or balance answers.

Implementation:

- `FerroniteThreatValue` живе на `AGP_GameState` per player.
- Worker drop-off → `FerroniteThreatValue += AcceptedAmount` (× `ThreatPerStoredUnit`, default 1.0 — threat = stored volume) → **БІЛЬШЕ swarm pressure**.
- Container launch (state → Departed) → `FerroniteThreatValue -= Container.Volume` → **МЕНШЕ swarm pressure**.
- Wave size / frequency = `DA_GP_Resource_Ferronite.ThreatToWaveSize` / `ThreatToWaveFrequency` curves keyed on `FerroniteThreatValue`.
- **Deprecated (superseded):** `SwarmAggressionLevel` як monotonic shipped/mined accumulator, `AggressionPerUnitShipped`, `AggressionPerUnitMined`. Driver рою — це поточний stored stock, не сумарний обсяг видобутого чи відправленого. FerroniteScore і OrbitalFerronite **не** впливають на swarm pressure.

> **Open Question (design TBD):** оскільки shipping ЗНИЖУЄ pressure, fast-shipper match може ніколи не ескалювати до climax. Можливо потрібен повільний secondary **global escalation floor** (наприклад mild time- або score-driven baseline під threat curve), щоб гарантувати, що матч завжди ескалює і завершується. Це **design question, не firm decision** — стартове значення + test plan визначаються у balance pass; чисел не вигадуємо.

## GAS Transactions (Updated)

| Effect | Trigger | Modifier |
| --- | --- | --- |
| `GE_GP_AddPlanetaryFerronite` (internal helper, no player-attribute write) | Worker drop-off → StorageComponent.AddToContainer | Container state mutation (not GAS-attribute) |
| `GE_GP_AddOrbital` | Container Launching → Departed transition | `OrbitalFerronite += Volume × OrbitalConversionRate` |
| `GE_GP_AddScore` | Same transition | `FerroniteScore += Volume × ScoreConversionRate` |
| `GE_GP_SpendOrbital` | Orbital drop order accepted | `OrbitalFerronite -= Cost` |
| `GE_GP_RefundOrbital` | Order canceled / drop failed (post-MVP) | `OrbitalFerronite += refund` |
| `GE_GP_Cost_RepairTick` | Worker repair tick | `OrbitalFerronite -= TickCost` |

**Removed** (pre-pivot effects superseded): `GE_GP_AddFerronite`, `GE_GP_SendToOrbit` (split into AddOrbital + AddScore), `GE_GP_SpendFerronite` (renamed → SpendOrbital).

## Starting Pool

Гравець стартує з **порожнім pool у обох станах:**

- `OrbitalFerronite = 0`.
- Containers empty.
- 2 starting Workers (pre-deployed, не arrive з drop).
- Player мусить mine → fill container → ship → отримати перший Orbital Ferronite → order перший drop. Loop opens.

Worldbuilding rationale: експедиція щойно висадилася, нічого ще не оброблено. Перший supply-drop приходить через ~30-60 s mining + ship cycle.

## Ferronite Deposits (Mining Source)

## Ferronite Deposits (Mining Source)

Ферроніт видобувається з родовищ — `Ferronite Deposit`. У TDD це `AGP_BuildingBase` з building type tag `GP.Resource.Node` (per ADR_0007 building-as-pawn; деталі deposit actor — у [`05_Buildings`](05_Buildings.md)).

### Data Asset Split

| Data Asset | Owner | Responsibility |
| --- | --- | --- |
| `DA_GP_Resource_Ferronite` | This page (resource type metadata) | DisplayName, gameplay tag, score conversion rate per unit, orbital conversion rate per unit, `ThreatPerStoredUnit` + `ThreatToWaveSize` / `ThreatToWaveFrequency` curves, visual tint / icon hint. |
| `DA_GP_Building_FerroniteDeposit` | [`05_Buildings`](05_Buildings.md) (actor properties) | Capacity, MineRatePerWorker, MaxConcurrentWorkers, DepletedBehavior, visual mesh, collision. References `DA_GP_Resource_Ferronite` для type identity. |

Це anti-duplication routing per [`documentation-knowledge-manager`](../../SKILLS/documentation-knowledge-manager/SKILL.md). Resource type — generic метадані. Deposit — instance actor properties.

### Resource Type Properties

`DA_GP_Resource_Ferronite`:

- `DisplayName` — "Ferronite".
- `ResourceTag` — `GP.Resource.Type.Ferronite`.
- `ScoreConversionRate` — units of FerroniteScore per unit Ferronite shipped to orbit (MVP — 1:1; TBD у balance pass).
- `OrbitalConversionRate` — units of OrbitalFerronite per unit shipped (MVP — 1:1; TBD у balance pass).
- `ThreatPerStoredUnit` — float, scalar для `FerroniteThreatValue` per unit raw Ferronite stored at base (default 1.0 — threat = stored volume; TBD у balance pass).
- `ThreatToWaveSize` / `ThreatToWaveFrequency` — `UCurveFloat` curves keyed on `FerroniteThreatValue` → SWARM wave size / frequency.
- `Tint` — `FLinearColor` для UI / VFX tinting (наприклад teal-blue glow).
- `Icon` — `TSoftObjectPtr<UTexture2D>` для UI readouts.

### Deposit Actor Properties

Capacity, mine rate, concurrent worker cap, depleted behavior — у `DA_GP_Building_FerroniteDeposit` (per [`05_Buildings`](05_Buildings.md)). Не дублюються тут.

### Mining Flow

```
Worker selected
   |
   v
Player issues GP.Command.Mine on Ferronite Deposit
   |
   v
Server validates: Worker type, Deposit alive (Capacity > 0), ownership
   |
   v
Worker moves to Deposit (UGP_MovementComponent)
   |
   v
Worker enters mining range
   |
   v
UGP_MiningComponent (server) starts tick:
   every 1s -> +MineRatePerWorker into Worker's CarriedFerronite (capped at WorkerCarryCapacity, наприклад 50)
   |
   v
When CarriedFerronite >= WorkerCarryCapacity OR Deposit depleted:
   Worker auto-returns to Main Base
   |
   v
At Main Base drop-off range:
   - Drop-off fills a MainBase container = UGP_StorageComponent state mutation (NOT a player GE).
     StorageComponent.AddPlanetaryFerronite(CarriedFerronite) -> returns accepted amount.
   - AGP_GameState.FerroniteThreatValue += AcceptedAmount × ThreatPerStoredUnit (swarm pressure rises).
   - NO income GE on drop-off. OrbitalFerronite / FerroniteScore GEs fire ONLY on container LAUNCH.
   - Worker auto-returns to Deposit (chain command), якщо Deposit alive і container slot free
```

### Drop-Off Rules

- Worker завжди повертається до **Main Base** (а не до будь-якої будівлі). Спрощує UX у MVP, узгоджується з лорою (Main Base = landing capsule + dispatcher).
- Якщо Main Base знищено → Worker stops carrying (loses CarriedFerronite або dumps на ground — TBD).

## Deposit Distribution (Map Design Constraint)

Це впливає на map design, але документується тут як gameplay constraint:

- **1 rich deposit** біля стартової зони кожного гравця (наприклад 2000 capacity) — забезпечує early-game без map exploration.
- **2-3 додаткові deposits** поза стартовою зоною (наприклад по 1500 capacity) — стимулюють mid-match expansion і конфлікт.
- Загальна capacity на мапі має забезпечити 10-хвилинний матч повного видобутку обома гравцями + reserves.

Це `Tracked` gap — точні числа фіналізуються через balance pass після першого playtest. Map placement — у [`07_Match_Flow`](07_Match_Flow.md) і future map design doc.

## OrbitalFerronite Costs (MVP)

Усі assets order-яться як orbital drops; cost списується з `OrbitalFerronite` (spendable state). Жодного on-planet build/produce.

| Orbital Drop | Cost (OrbitalFerronite) | Source |
| --- | --- | --- |
| Worker | TBD | `UGP_OrbitalDropDefinition.Cost` (DA_GP_OrbitalDrop_Worker) |
| Salvage Walker | TBD | `UGP_OrbitalDropDefinition.Cost` (DA_GP_OrbitalDrop_SalvageWalker) |
| Defensive Turret | TBD | `UGP_OrbitalDropDefinition.Cost` (DA_GP_OrbitalDrop_DefensiveTurret) |
| Wall Package (5 segments) | TBD | `UGP_WallPackageDefinition.Cost` (`DA_GP_WallPackage`) |
| Logistics Hub | TBD | `UGP_OrbitalDropDefinition.Cost` (DA_GP_OrbitalDrop_LogisticsHub) |

Усі numbers — TBD у balance pass. Зберігаються у Data Assets, не в C++. Spend виконується через `GE_GP_SpendOrbital` на accepted drop order (per [`10_Orbital_Delivery`](10_Orbital_Delivery.md)).

**Trade-off:** кожна одиниця OrbitalFerronite, витрачена на drop, не лишається у банку, АЛЕ score (FerroniteScore) уже зарахований при launch і **не** зменшується від spending. Гравець постійно балансує "інвестую в roster / захист — менше spendable currency зараз" vs "тримаю currency у резерві". Окремий, глибший trade-off — hoard-vs-ship (greed-vs-safety) на рівні raw Ferronite, що керує SWARM (див. SWARM Pressure Tie-In).

## Score Mechanic Tie-In

`FerroniteScore` — primary metric перемоги. Деталі — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md):

- Сumulative протягом матчу.
- Не зменшується (не "втрачається" при руйнуванні бази).
- Реплікується через PlayerState ASC, видно опонента.
- Гравець бачить власний і опонента score у HUD у реальному часі (див. [`09_UI_UX`](09_UI_UX.md)).

## SWARM Escalation Tie-In

Незахищений сирий ферроніт на базі збуджує рій. Що **більше raw Ferronite фізично лежить у containers зараз** (`FerroniteThreatValue`) → сильніші і частіші waves. Hoarding небезпечний; shipping знижує тиск. Це зворотний баланс, що запобігає необмеженому snowball-у накопичення. Конкретна шкала — `DA_GP_Resource_Ferronite.ThreatToWaveSize` / `ThreatToWaveFrequency` curves keyed on `FerroniteThreatValue`; per-unit scalar — `ThreatPerStoredUnit`. Деталі match-flow — у [`07_Match_Flow`](07_Match_Flow.md).

## UI Surface

HUD показує (per [`09_UI_UX`](09_UI_UX.md)):

- OrbitalFerronite (spendable currency, numeric).
- Cumulative FerroniteScore (numeric, prominent).
- Opponent FerroniteScore (numeric, secondary).
- Container row (raw Planetary Ferronite stored at base — visualizes FerroniteThreatValue).
- Match Timer (countdown from 10:00).
- SWARM threat indicator driven by FerroniteThreatValue (TBD form — bar, icon, color shift; rises on hoard, drops on launch).

## Out of MVP

- Multi-resource economy (additional metals, gas, energy cells).
- Resource market / trade.
- Resource decay або spoilage.
- Player-built resource storage / silo.
- Explicit "Launch Shipment" action з захистом конвою.
- Off-world meta upgrades між матчами (Helldivers-like persistent). Це окрема feature у `Backlog`.
- Multi-tier deposits (rich / standard / poor variants).
- Mineable from buildings (не тільки workers).

## References

- Resource technical model — [`../TDD/07_Resource_Architecture`](../TDD/07_Resource_Architecture.md).
- Deposit як `AGP_BuildingBase` child — [`05_Buildings`](05_Buildings.md).
- GAS attribute / effect setup — [`../TDD/02_GAS_Architecture`](../TDD/02_GAS_Architecture.md).
- Score mechanic — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- SWARM waves escalation — [`07_Match_Flow`](07_Match_Flow.md), [`03_Factions`](03_Factions.md).
- Worldbuilding — [`Lore_Setting`](Lore_Setting.md).
