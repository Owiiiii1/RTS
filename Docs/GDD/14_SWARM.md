# SWARM

> **Status (2026-09-02):** approved concept / documentation-only. Runtime implementation **not started**.
> Gameplay WHAT for environmental pressure. Technical HOW: [`../TDD/17_SWARM_Architecture.md`](../TDD/17_SWARM_Architecture.md).
>
> This page **supersedes** discrete-wave placeholders (`WaveInterval`, `WaveSize`, `WaveStartDelay` as a
> required wave model, `WaveSpawnPoints`, `ThreatToWaveSize`, `ThreatToWaveFrequency`, predetermined
> wave schedule). Do not invent balance numbers here.

## Role

SWARM — це **environmental pressure**, не playable third faction.

- Викликається **поточним raw Planetary Ferronite** у контейнерах MainBase відповідної команди.
- Pressure **окремий для кожної команди**. `FerroniteScore` і `OrbitalFerronite` **не** керують SWARM.
- Гравець **не може** контролювати SWARM і **не може** спрямовувати його на противника.
- Основна стратегічна ціль потоку — **MainBase цієї команди**.
- MainBase є основним сховищем. Знищення MainBase = **immediate annihilation loss**.
- Окремого похищення, поїдання чи знищення Ferronite істотами **немає**: ресурс зникає разом із
  знищеною MainBase, і матч закінчується.

Current runtime вже тримає per-team `FerroniteThreatValue` на `AGP_GameState`. Per-team **director /
spawn stream** ще не існує — це required future implementation, не готову систему.

## Continuous Pressure

SWARM іде **безперервним потоком**. Класичних numbered waves немає.

Допустимі короткі природні імпульси щільності всередині потоку. Вони **не** є окремими waves і не
мають власного schedule.

**Threat bands** визначають:

- цільовий active swarm budget;
- кількість одночасно активних напрямків спавну;
- частоту поповнення;
- допустимий roster;
- ймовірність і ліміт Large істот.

При підвищенні або зниженні threat інтенсивність змінюється **поступово**. Вже створені істоти **не
зникають** при зниженні threat.

Конкретні band thresholds, budget і spawn rates — **TBD у balance / prototype pass**. Не вигадувати
числа в цій концепції.

### Superseded wave model

Не використовувати як затверджену модель:

- `WaveInterval`, `WaveSize`, `WaveStartDelay` як обов'язкова wave-модель;
- `ThreatToWaveSize`, `ThreatToWaveFrequency`;
- predetermined wave schedule / `WaveSpawnPoints` як фіксовані відомі входи.

Якщо існуючі runtime / Data Asset поля ще носять ці імена — це **legacy / current-runtime
placeholders**, які потребують later implementation reconciliation. Код у цьому checkpoint не
перейменовується.

## Spawn

Навколо playable level розташовується **замкнений зовнішній spawn spline**. Director випадково
обирає допустимі точки / ділянки цього сплайна. Спавн відбувається **за межами баз і нормального
ігрового простору**.

Кількість одночасно активних ділянок залежить від threat band.

Обрана точка перевіряється на:

- досяжність MainBase цільової команди;
- відсутність недоступної геометрії;
- знаходження поза базою / start zone;
- прийнятну видимість появи;
- достатнє рознесення активних напрямків, якщо не обрано концентрований pressure pulse.

Кожен створений unit / group отримує **MainBase своєї команди** як кінцеву стратегічну ціль. SWARM
рухається до MainBase і атакує все, що заважає руху.

Стіни затримують потік і створюють kill zones, але SWARM може зруйнувати перешкоду і продовжити
шлях. Випадковий зовнішній підхід змушує будувати **кругову оборону**, а не укріплювати кілька
відомих spawn points.

## Classes

### Large

Повноцінний самостійний gameplay unit: власні HP, рух, collision, attack і targeting. Може бути
реальною загрозою армії. Одночасно присутній у **невеликій** кількості.

### Medium group

Один gameplay unit. Усередині кілька візуальних мешів. Спільні HP, path, target, collision, attack і
replication. Кількість живих візуальних мешів зменшується за health thresholds.

### Small group

Один gameplay unit. Усередині приблизно десять або більше візуальних істот. Спільні HP, path,
target, aggregate damage, radius і replication. Окремі візуальні істоти **не** мають власного
pathfinding, targeting, gameplay collision або мережевої сутності.

Конкретні class names / Data Assets — future implementation. Не затверджувати ще не створені player
unit types як відповідь на SWARM.

## Group Damage And Attack

- Вибух або інший AoE завдає кожній зачепленій **gameplay-групі** шкоду **один раз**.
- Шкода **не** множиться на кількість візуальних учасників усередині групи.
- Дотик лише візуального краю натовпу **не** є влучанням.
- Використовується **gameplay footprint** групи, менший за візуальну область.
- Допустимий стандартний radial falloff.
- При проходженні health threshold гине візуальний учасник, **найближчий до точки впливу**.
- Attack damage групи пропорційний кількості живих візуальних мешів (наприклад ~70% живих → ~70%
  вихідного aggregate damage).
- Атака залишається **однією gameplay-операцією**, хоча всі живі меші грають attack animation з
  різними фазами.

## Interaction With Player Armies

Small / Medium SWARM залишається значно небезпечнішим для **будівель і Workers**, ніж для бойових
машин. Водночас живі Medium і Large **фізично блокують** рух player units.

На високому SWARM pressure потік стає живим бар'єром навколо бази. Для наступу через такий периметр
потрібна важка промислова бойова техніка. Для доставки десанту через щільний SWARM потрібні важкі
захищені транспорти.

SWARM може **опосередковано** захищати базу, але цей захист викликаний небезпечним накопиченням
Ferronite і вимагає від власника власної постійної оборони.

Не використовувати візуальну термінологію звичайних сучасних tanks / APC як затверджений art
direction. Зберігати Pillar 2: industrial vehicles, converted machinery, heavy industrial
transports.

Формулювання «дві майже неперетинні війни» **superseded**.

## Crush Rules

Розділяти **живих** істот і **трупи**.

### Living

- Light vehicles **не** розчавлюють живий SWARM.
- Medium і Heavy vehicles розчавлюють **тільки Small** SWARM.
- Medium і Heavy **сповільнюються**, рухаючись крізь Small SWARM. Заповільнення може залежати від
  кількості перетинаних Small groups, але має **cap**.
- Crush damage діє лише при русі **вище мінімальної швидкості**.
- Живих Medium і Large **не можна** просто переїхати: їх треба знищити зброєю.

### Small-group vehicle avoidance

- Small groups м'яко змінюють **локальну** траєкторію, щоб не потрапити під техніку.
- Вони **не** тікають, **не** змінюють MainBase target і **не** будують новий глобальний маршрут.
- Використовується predicted vehicle corridor за позицією, напрямком і швидкістю.
- Обрана сторона обходу тимчасово фіксується (без oscillation).
- Реакція на **стоячу** техніку мінімальна.
- Це має запобігати обороні бази простим постійним катанням по Small SWARM.
- Small groups **не** блокують Light vehicles і не отримують від них crush damage.

### Corpses

- Light vehicle очищає тільки Small corpses.
- Medium vehicle очищає Small і Medium corpses.
- Heavy vehicle очищає Small, Medium і Large corpses.
- Живих Medium / Large спочатку треба вбити.

Класи Light / Medium / Heavy для player vehicles — **conceptual crush tiers**, не затверджений
конкретний roster нових юнітів.

## Corpses As Temporary Obstacles

- Small death може створювати короткостроковий corpse mesh.
- Medium і Large corpses є **тимчасовими перешкодами**.
- Труп спочатку лишається на поверхні, потім опускається нижче рівня землі і видаляється. Це **не**
  має виглядати як миттєвий despawn.
- Collision вимикається, коли тіло візуально перестає перекривати шлях.
- Large corpse може тимчасово перекрити choke point.
- SWARM Medium / Large проходять вузькі ділянки **по черзі**.
- Гравець може тактично вбити велику істоту в проході і тимчасово затримати потік.
- Підходяща техніка може продавити труп і розчистити прохід.

Обов'язковий runtime NavMesh rebuild **не** затверджується. Переважний напрям: transient obstacle
data / traversability layer або локальна перевірка і repath. Узгоджувати з
[`../TDD/16_Voxel_Terrain_And_Foundations.md`](../TDD/16_Voxel_Terrain_And_Foundations.md). Конкретний
backend — prototype / profile pass.

## Blood

- Одна істота загинула — з'являється **одне** кроваве пятно, одночасно з corpse mesh.
- Пятно **не** розтікається і **не** збільшується з часом.
- Persistent blood splashes не створюються; бризки допустимі лише як короткочасний impact VFX.
- Розмір пятна залежить від класу Small / Medium / Large. У кожному розмірі має бути багато
  візуальних варіантів (masks, rotation, scale variation, mirror, tint / intensity).
- Пятна лишаються **до кінця матчу**, накладаються і збільшують насиченість поверхні.
- Якщо вся передбачувана площа нового пятна вже має максимальну інтенсивність — новий stamp не
  малюється. Якщо насичена лише частина — stamp має розширити / посилити решту області.
- Тисячі постійних Decal Actors **не** є цільовою архітектурою. Переважний напрям: accumulated
  world-space blood mask, tiled render targets / splat map, або RVT-compatible approach.
- Blood — **cosmetic-only**: не впливає на рух, damage або navigation.
- Враховувати вільне обертання камери і збільшення приблизно вдвічі відносно звичайного виду.

## What This Page Does Not Decide

- Конкретні threat-band числа, spawn rates, HP, damage.
- Імена C++ класів director / group actor.
- Остаточний renderer (Niagara / VAT / sprites) і Mass vs lightweight groups — див. TDD/17.
- Нові player unit types як відповідь на crush / transport fantasy.

## References

- Pillar 6 — [`01_Game_Pillars.md`](01_Game_Pillars.md).
- Factions framing — [`03_Factions.md`](03_Factions.md).
- MainBase / walls — [`05_Buildings.md`](05_Buildings.md).
- Threat SoT — [`06_Resources.md`](06_Resources.md).
- Match flow — [`07_Match_Flow.md`](07_Match_Flow.md).
- Annihilation — [`08_Win_Lose_Conditions.md`](08_Win_Lose_Conditions.md).
- Technical architecture — [`../TDD/17_SWARM_Architecture.md`](../TDD/17_SWARM_Architecture.md).
- Terrain / traversability — [`../TDD/16_Voxel_Terrain_And_Foundations.md`](../TDD/16_Voxel_Terrain_And_Foundations.md).
