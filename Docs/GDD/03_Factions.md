# Factions

## MVP Faction Scope

GrimProtocol MVP має **дві сторони, що задіяні у матчі**:

1. **Player Faction (Corporate Colonial)** — одна симетрична фракція для обох гравців.
2. **SWARM (Environmental Threat)** — neutral hostile third party, що загрожує обом гравцям.

Обидва гравці використовують ту саму corporate colonial фракцію; розрізнення — тільки team color і team ID. Це найшвидший шлях до playable build і максимально відсікає cost asymmetric balance у MVP.

Per Pillar 5 (Corporate Rivalry, Not Hero War) і Pillar 6 (SWARM as Environmental Pressure) з [`01_Game_Pillars`](01_Game_Pillars.md).

## Player Faction (Corporate Colonial)

### Design Intent

Гравець керує підрозділом корпорації, відправленим на видобуток ферроніту. Worldbuilding — [`Lore_Setting`](Lore_Setting.md). Mechanically — це класична RTS фракція без unique-tech дерева. Усі MVP-юніти і будівлі належать цій фракції, обидва гравці мають симетричний доступ.

### Faction Data Model

`DA_GP_Faction_Default`:

- `DisplayName` (`FText`) — placeholder "Corporate Colonial".
- `Color` (`FLinearColor`) — team color для materials, UI (різні значення per team у `AGP_PlayerState`).
- `StartingUnits` — `TArray<TSoftObjectPtr<UGP_UnitDefinition>>` (e.g., 2 × Worker).
- `StartingBuildings` — `TArray<TSoftObjectPtr<UGP_BuildingDefinition>>` (1 × Main Base).
- `StartingResources` — `TMap<FGameplayTag, float>` (`GP.Resource.Type.Ferronite -> 0`; empty pool per [`06_Resources`](06_Resources.md)).
- `AllowedUnits` — список UnitDefinitions, що ця faction може produce.
- `AllowedBuildings` — список BuildingDefinitions.
- `FactionTags` — `FGameplayTagContainer`.

### Faction Assignment

- `AGP_PlayerState.FactionAsset : TSoftObjectPtr<UGP_FactionDefinition>` — replicated.
- `AGP_GameMode` assigns faction при join (поки обидва гравці отримують `DA_GP_Faction_Default`).
- Team ID (1 або 2 для PvP, або 0 для AI opponent у singleplayer) — окремий primitive (`AGP_PlayerState.TeamId : int32`, replicated).

### Why Tag-Driven

Замість enum'у `EFaction` — `FGameplayTag` контейнер. Це дозволяє:

- Не міняти enum при додаванні нової фракції у post-MVP.
- Кваліфікувати unit capability через tags (`Unit.Faction.Corporate`, `Unit.Faction.Swarm`), без enum-switch блоків.
- Designer-friendly: нова faction = новий tag + новий Data Asset.

### Asymmetry Direction (Post-MVP)

Per Pillar 5 (Corporate Rivalry, Not Hero War), будь-яка asymmetry між гравцями у post-MVP проходить **виключно через corporate doctrine trees**, не через biological races, magic, alternative realities, multiverse або time travel.

Asymmetry axes, що дозволені для майбутнього доступу:

- **Mining Doctrine** — efficiency, deposit reach, worker count, drop-off speed.
- **Logistics Doctrine** — transport throughput, drop-off bonus, capacity-per-Assembly-Yard.
- **Defensive Doctrine** — defense node cost reduction, HP bonus, repair efficiency.
- **Extraction Efficiency** — Ferronite score conversion rate, bonus per drop-off.
- **Automated Machinery Style** — passive automation tier (autonomous workers, repair drones).

Гравець обирає / купує doctrine path на старті матчу або через in-match progression (mechanic TBD). Це **не у MVP** — у `Backlog` як окремий validated entry.

Asymmetry hard bans (review-blocking, per Pillar 5):

- Біологічні раси з різною біологією.
- Магічні / supernatural abilities.
- Multiverse / alternative-reality factions.
- Time-travel based asymmetry.
- Hero-unit-centric faction identity.

## SWARM (Environmental Threat)

### Design Intent

SWARM — це **environmental / ecological pressure**, не повноцінна third RTS faction. Worldbuilding — [`Lore_Setting`](Lore_Setting.md). SWARM:

- **AI-only.** Не може бути обраний як playable faction.
- **Не контрольований гравцем** ні прямо (commands), ні опосередковано (buffs, summoning, controlling structures).
- Не має economy, base, production.
- Спавниться server-side з **замкненого зовнішнього spawn spline** (випадкові допустимі ділянки), не з фіксованого набору відомих входів.
- Стратегічна ціль потоку — **MainBase цієї команди**. Усе на шляху атакується як перешкода.
- Інтенсивність — безперервний per-team pressure від `FerroniteThreatValue` (raw Ferronite у контейнерах MainBase; ↓ при launch). Canonical: [`14_SWARM`](14_SWARM.md).

Це **hard constraint** з Pillar 6. Будь-яка mechanic, що дозволяє гравцю керувати SWARM або суттєво впливати на його напрям — review-blocking.

### MVP Variant — "SWARM as Background" (Variant 2)

SWARM у MVP — **background pressure**, не main combat focus. Це означає:

- Small / Medium значно небезпечніші для **будівель і Workers**, ніж для бойових машин; Medium / Large все одно **блокують** рух армії.
- На високому pressure потік стає живим бар'єром навколо бази; потрібна кругова оборона і важка промислова техніка, щоб пробити периметр.
- Гравець мусить інвестувати у Defensive Turrets / Walls ([`05_Buildings`](05_Buildings.md)) і тримати бойове покриття біля бази.

Це створює стратегічний trade-off: more workers = more score, але більше SWARM aggro = більше потреби в обороні. Гравець постійно балансує економіку проти захисту.

### Continuous Pressure (supersedes Wave Schedule)

Дискретна wave-модель (`WaveInterval`, `WaveSize`, `WaveStartDelay`, `WaveSpawnPoints`,
`ThreatToWaveSize` / `ThreatToWaveFrequency`) **superseded**. Канон — безперервний per-team потік:
[`14_SWARM`](14_SWARM.md), технічно [`../TDD/17_SWARM_Architecture`](../TDD/17_SWARM_Architecture.md).

- Threat bands задають budget, кількість активних spline-напрямків, replenishment, roster, Large cap.
- `FerroniteThreatValue` — per-team raw Ferronite у MainBase containers (↑ drop-off, ↓ launch).
  Замінює `SwarmAggressionLevel` / `AggressionPerUnit*`. `FerroniteScore` / `OrbitalFerronite` не
  керують SWARM.
- Якщо код/DA ще містить старі wave-імена — legacy placeholders, не approved production schema.

Класи: Large (окремий gameplay unit), Medium group, Small group. Конкретні DA / roster — future
implementation, не вигадувати числа тут.

### SWARM Targeting

1. Спавн на допустимій ділянці зовнішнього spline.
2. Кінцева стратегічна ціль — MainBase **цієї** команди.
3. Рух до MainBase; атака всього, що блокує шлях (стіни, юніти, інші будівлі).
4. SWARM не керує економікою і не займає ferronite deposit.

Рух груп — lightweight group simulation (TDD/17), **не** обов'язково поточний `UGP_MovementComponent`.

### SWARM Tags

- `GP.Faction.Swarm` — root SWARM tag.
- `GP.Unit.Type.Swarm` — SWARM unit type.
- `GP.SWARM.Tier.Grunt` — tier у MVP (post-MVP — Elite, Brood, etc.).

### Open Design — Variants 1, 3, 4 (Tracked Gaps)

Owner надав 4 варіанти SWARM design. У MVP — Variant 2 (background). Інші:

- **Variant 1 (Distributed Corridors):** один вхід постійно атакується SWARM, інші — для player conflict. Map-design heavy. → `Backlog`.
- **Variant 3 (Influence Structures):** гравець будує inhibitor / energy corridor / decoy, що впливає на SWARM spawn або поведінку. Нова механіка, нові building Data Assets. → `Backlog`.
- **Variant 4 (SWARM Manipulation):** гравець опосередковано спрямовує SWARM проти опонента. Asymmetric warfare layer. → `Backlog`. **Note:** approved concept **forbids** directing SWARM onto the opponent (Pillar 6 / GDD/14). This variant still needs a pillar amendment before any reconsideration.

Усі три — `Strong Mechanic, Production Trap` за [`gp-mechanics-validator`](../../SKILLS/gp-mechanics-validator/SKILL.md): кожен додає мінімум одну нову систему, не критичну для MVP playable.

## AI Opponent (Singleplayer MVP)

### Scope

Singleplayer MVP має **primitive AI opponent**, що використовує ту саму corporate colonial фракцію і **ту саму orbital-delivery модель**, що й гравці (нічого не будується локально — все прибуває з орбіти через drop pods). Дизайн-рішення (raised from out-of-scope):

1. AI замовляє orbital drops для розширення бази (Logistics Hub + кілька Defensive Turrets) через Order Menu / `UGP_OrbitalDeliverySubsystem`.
2. AI mineить ferronite (1-2 worker-и mineять, інші — резерв) і відправляє контейнери на орбіту (Planetary → Orbital Ferronite).
3. AI замовляє defenders до early-mid roster size (5-8 Salvage Walkers) через orbital drops.
4. AI атакує player base, коли roster threshold досягнутий.
5. AI passively реагує на SWARM pressure (defenders + turrets handle), не намагається micro.

Це не goal-oriented і не utility AI. Це state machine з 4-5 станами, що оцінює прості умови (orbital Ferronite pool, unit count, time elapsed).

### AI States

`EGP_AIState` (per [`../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController`](../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md)): `Explore`, `Mine`, `Ship`, `Order`, `Defend`. Mapped to MVP behaviour:

- `Explore` / `Mine` — спавн / розподіл workers, mining ferronite.
- `Ship` — відправка повних контейнерів на орбіту (конвертація у Orbital Ferronite).
- `Order` — замовлення orbital drops (Logistics Hub, Defensive Turret, Salvage Walkers) коли вистачає Orbital Ferronite.
- `Defend` — переходить у Defend якщо власна база critically damaged або при високому swarm-тиску; стягує roster до бази.

### AI Implementation Hint

- `AGP_AIController : AAIController` — server-side AI controller. Вибір `AAIController` (а **не** `AGP_PlayerController`) зафіксовано у [`../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController`](../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md) — більше не TBD.
- AI має звичайний `AGP_PlayerState` з ASC (симетрично з гравцем) і використовує **ту саму orbital-delivery модель**, що й гравці — окремого AI-шляху немає.
- AI **не** використовує client→server `Server_RequestCommand` RPC; натомість викликає **ті самі server-side command-execution helpers**, що й human PC після валідації (спільний server-authoritative command layer). Деталі — [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md) §AI Opponent Authority Notes.
- Decision tick — низькочастотний (раз в 2-5 секунд), не frame-tick.
- Цільові правила — у Data Asset (`DA_GP_AIBehavior_Default`), щоб designer міг tuneити без C++ зміни.

Не повноцінний AI. Не learning. Не planning. **Призначення — забезпечити opponent для playable singleplayer match-у**, не challenging опонент.

Деталі рішення — [`../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController`](../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md); authority / command-path — [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md) §AI Opponent Authority Notes.

## Cross-Reference with Other GDD Pages

- Player units — [`04_Units`](04_Units.md).
- Player buildings (Main Base, Logistics Hub, Defensive Turret) — [`05_Buildings`](05_Buildings.md).
- SWARM concept — [`14_SWARM`](14_SWARM.md).
- SWARM у timeline — [`07_Match_Flow`](07_Match_Flow.md).
- Score-based win — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- Resource as aggro trigger — [`06_Resources`](06_Resources.md).
- Worldbuilding — [`Lore_Setting`](Lore_Setting.md).

## Out of MVP

- Друга player faction (asymmetric corporate rival).
- Faction-specific unique units / abilities / tech tree.
- Faction lore divergence (одна фракція = одна корпорація з власним візуалом).
- SWARM design Variants 1, 3, 4.
- Multiple SWARM tiers / roles (siege SWARM, swarmer, heavy SWARM unit).
- Advanced AI (goal-oriented, utility, learning).

Усі — у `Backlog/`.

## References

- Unit definitions — [`04_Units`](04_Units.md).
- Building definitions — [`05_Buildings`](05_Buildings.md).
- Resource flow — [`06_Resources`](06_Resources.md).
- Tag namespace — `/CONTRIBUTING.md` → Gameplay Tag Philosophy.
- Worldbuilding — [`Lore_Setting`](Lore_Setting.md).
