# Lore and Setting

## Scope

Світ GrimProtocol: де відбуваються матчі, чому гравці воюють один з одним, чому планета вороже налаштована, який візуальний стиль фракції і техніки. Цей документ — gameplay rationale layer для механік з [`03_Factions`](03_Factions.md), [`04_Units`](04_Units.md), [`05_Buildings`](05_Buildings.md), [`06_Resources`](06_Resources.md). Не TDD, не implementation.

## Hard Lore Bans

GrimProtocol — **industrial sci-fi extraction RTS**, не sci-fi fantasy. Категорично заборонено:

- **Time travel** як plot mechanic, faction backstory, або gameplay tool.
- **Multiverse** як narrative framing або faction differentiator.
- **Alternative realities** як пояснення опонентів, ворогів, технологій або механік.
- **Magic / supernatural** у будь-якій формі (psychic, mystical, divine, paranormal).
- **Cosmic horror** як lore expansion (рій — біологічна / екологічна загроза, не Lovecraftian entity).
- **Hero protagonists** і character-driven narrative arcs.

Усе пояснюється industrial / corporate / ecological логікою. Якщо механіка або фракція потребує "час перевертається" або "ми з іншої реальності" — це **review-blocking** порушення pillar 5 (Corporate Rivalry, Not Hero War).

## Planet EREBUS-9

- Класифікація: F-3 (виcoка корисність ресурсів, висока небезпека).
- Атмосфера: непридатна для дихання людиною. **Тільки машини** оперують на поверхні.
- Geological feature: у надрах планети — родовища **ферроніту**, металу з аномальною енергетичною щільністю.
- Discovery context: планета не була науковою експедицією. Це була гонка корпорацій за першу хвилю видобутку.

## Ferronite

Ферроніт — це не звичайний метал. Властивості:

- Джерело енергії з аномально високою щільністю.
- Впливає на ДНК біологічних форм життя.
- Невід'ємна частина місцевої екосистеми — істоти EREBUS-9 еволюціонували навколо ферроніту.

Для корпорації, що першою наладить промисловий видобуток, ферроніт = монополія на енергетичний ринок Сонячної системи. Тому видобуток на EREBUS-9 не є економічним вибором — це **гонка на виживання корпорації**.

Детальніше про ресурс як механіку — [`06_Resources`](06_Resources.md).

## Why Colonists Fight Each Other

На EREBUS-9 діє правило колоніального права:

> "Те, що ти утримуєш — твоє."

Зв'язок із Землею слабкий, центральне втручання запізнюється на тижні. Закон — це твоя база і твої вежі. Корпорації знають це, тому відправили на планету:

- автономні шахтарські загони,
- модульну військову техніку для самозахисту,
- мінімальний support — після висадки кожен сам за себе.

Гравці воюють не з ідеологічних причин. Вони воюють тому, що:

- ферроніту не вистачає на всіх;
- видобуток — єдиний шлях повернутися на Землю з прибутком;
- кожна база — інвестиція на мільярди, втрата якої = крах корпорації-власника.

У контексті MVP — це симетрична PvP-механіка з [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md): хто більше видобув і відправив на орбіту до кінцевого таймера — переміг.

## Why Locals Attack

EREBUS-9 не безлюдна. На планеті існує екосистема істот (далі — **SWARM**, AI-only environmental faction), які:

- відчувають вібрацію видобутку ферроніту,
- харчуються ферронітом,
- використовують його у власних панцирах, рогах, нейроструктурі.

Видобуток для них — акт агресії проти екосистеми. Чим більше гравці видобувають, тим:

- частіше відбуваються хвилі атак,
- сильніші стають окремі особини рою,
- ширше зона активності рою.

Це створює self-balancing economic pressure: видобуток дає score, але стимулює загрозу. MVP реалізація — Variant 2 ("SWARM as background") з [`03_Factions`](03_Factions.md): SWARM слабкий проти Salvage Walker і Defensive Turret, але загрозливий для workers і будівель без захисту.

## Visual Style

### Overall Direction

Industrial Siege Sci-Fi. "Втомлений" сай-фай, не вилизаний. Референси:

- Deep Rock Galactic, Moon Miners — primary reference. Шахтарська експедиція на ворожу планету, industrial / engineering identity.
- Aliens 2 — для **atmosphere** колоніальної expedition (atmosphere of overstretched corporate operation), **не** для military combat aesthetic.
- Starship Troopers — **only** для general "hostile planet ecology / waves of biological threat" pacing. **Не** для military soldier visual identity — це порушує Pillar 2 (Engineer, Not Soldier).

Pillar 2 переважає над усіма visual references. Якщо reference тягне у military aesthetic — він відкидається на користь industrial / engineering read.

### Asset Aesthetics

- Грубий метал, болти, зварні шви, труби, контейнери.
- Жовто-чорні попереджувальні смуги, іржа, потерті поверхні.
- Світлові акценти — світло-синій або помаранчевий, через щілини або індикатори.

### No-Go

- Плавні, "інопланетні" органічні форми.
- Лощений, чистий "Apple"-style сай-фай.
- Магія, фентезі-елементи.

### Color Scheme (Faction)

- Основний: темно-сірий / сталевий.
- Вторинний: жовтий + чорний (попереджувальні смуги, маркування модулів).
- Акценти: світло-синій / помаранчевий для світлових елементів.

## Tech Style

GrimProtocol — **engineer expedition aesthetic**, не military aesthetic. Принцип Pillar 2 (Engineer, Not Soldier) застосовується до всієї техніки фракції:

- Mining / engineering / industrial equipment — primary visual read.
- Combat capability виглядає як **переобладнані інструменти** або defensive systems (drill з combat mode, salvaged plasma cutter як turret, repurposed conveyor як attack vector).
- Контейнери, модульні блоки, бурові установки, крани, конвеєри, generator hubs.
- Тимчасові укріплення, modular industrial outposts.
- Обладнання подвійного призначення (mining tool that can also defend).
- Heavy industrial silhouettes з exposed mechanisms, welded armor, danger striping.

### Visual Vocabulary (Allowed)

- Drilling rigs з rotating bits.
- Tracked mining vehicles (гусениці, бури, scoops).
- Pistons, hydraulics, гідравлічні підйомники.
- Cranes, conveyor belts, modular cargo lifters.
- Turret-like defensive nodes (не military turrets — repurposed industrial cutters).
- Lights / indicator panels / warning beacons.
- Welded steel plating, riveted armor, yellow-black hazard striping.

### Visual Vocabulary (Forbidden)

- **Military vehicle aesthetic.** Танки, APCs, gunships, fighter jets, bombers. Жодних military silhouettes.
- **Bipedal humanoid soldiers.** Infantry, troopers, riflemen, snipers — будь-яка людиноподібна combat фігура.
- **Combat mech aesthetic.** Battletech / Gundam / MechWarrior-style mechs. Якщо є walker — це industrial excavator з combat retrofit, не bipedal warmachine.
- **Hero unit visual centerpieces.** Outsize, cinematic, hand-animated juggernauts.
- **Creature-like player units.** Organic motion, biological silhouettes для player faction.
- **Heavy skeletal animation** як основа visual identity (per Pillar 7).

Концептуально — кожна combat-capable машина має lore "це переобладнаний шахтарський інструмент, бо корпорація не повезла достатньо armed escort". Це **зафіксована візуальна канва**, не optional flavor.

## SWARM Faction Visual

SWARM (uppercase, lore-canonical name) — environmental ecological threat, AI-only, non-playable. Деталі gameplay — [`03_Factions`](03_Factions.md).

Visual identity:

- Хітинові панцирі з вкрапленнями ферроніту (світяться приглушеним помаранчевим або синім).
- Силуети — гострі, кутасті, без округлих форм.
- Розмір варіюється від worker-sized (рядові) до building-sized (elite tiers, post-MVP).
- Animation budget — мінімальний; SWARM units мають basic locomotion і single-pose attack. Жодних cinematic creature anims.

## Player Fantasy

Гравець — представник корпорації, що отримав контракт на видобуток. Він **не герой**. Він — менеджер автоматизованої колоніальної операції на ворожій планеті, який:

- розгортає базу,
- запускає видобуток,
- захищає інфраструктуру від рою,
- конкурує з іншими корпораціями за обʼєм видобутку,
- відправляє ферроніт на орбіту, доки таймер не закінчиться.

Це не епічна війна за виживання людства. Це холодний, утилітарний індустріальний конфлікт.

## Open Questions

- Чи показуємо на map physical "shuttle launch" для відправки ферроніту на орбіту як cosmetic event? `TBD`.
- Чи має кожна корпорація unique faction identity (logo, кольори), при тому що mechanically обидва гравці симетричні? `TBD` — наразі placeholder `DA_GP_Faction_Default` з [`03_Factions`](03_Factions.md).
- Чи допускається cinematic intro при старті match-у з показом landing capsule? `TBD`.

## References

- Resource mechanic — [`06_Resources`](06_Resources.md).
- Factions and SWARM design — [`03_Factions`](03_Factions.md).
- Win condition tied to score — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- Match flow з SWARM wave escalation — [`07_Match_Flow`](07_Match_Flow.md).
- Game pillars — [`01_Game_Pillars`](01_Game_Pillars.md).
