# Game Pillars

Пілари — короткі неподільні правила, що тримають gameplay identity GrimProtocol. Кожне дизайнерське рішення перевіряється проти них. Pillars розділені на **Gameplay Pillars** (1–8, ідентичність гри і design philosophy) і **Technical Pillar** (9, інженерний фундамент, що обслуговує gameplay).

Конфлікт між pillars — tie-break per нижче.

## Framework Note

Кожен pillar описаний через структуру з [`game-design-framework`](../../SKILLS/game-design-framework/SKILL.md):

- **Player Goal** — що гравець хоче і чому це важливо у грі.
- **What It Allows** — простір дозволених рішень.
- **What It Forbids** — категорично заблоковано.
- **5-Component Check** — Clarity / Motivation / Response / Satisfaction / Fit implication.

---

## 1. Industrial Extraction First

**Identity statement.** GrimProtocol — гра про видобуток, а не про війну. Кожна основна decision гравця прив'язана до економіки видобутку ферроніту. Combat існує як defensive necessity, не як primary verb.

### Player Goal

Зайняти ділянку планети, налагодити видобуток ферроніту і вивезти максимум на орбіту до закриття контрактного вікна.

### What It Allows

- Economy-driven RTS mechanics (mining nodes, drop-off chains, capacity expansion).
- Map-control механіки через extraction sites (захопити deposit = підняти cap economy).
- Defensive mechanics як інвестиція у захист видобутку.
- Sabotage / interference mechanics проти opponent economy (post-MVP).

### What It Forbids

- Frontline military mechanics (army composition focus, push-the-base loop як основа).
- Hero combat focus.
- Mechanics, де combat — це primary score generator.
- Score / progression, відірваний від extraction throughput.

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | Кожен score increment ясно прив'язаний до drop-off event. Score readout = mining performance proxy. |
| **Motivation** | Score — primary win condition; mining → score → перемога. Direct loop. |
| **Response** | Команди `Mine`, `Build`, `Move` дають immediate local feedback; server confirms у read-only вікно. |
| **Satisfaction** | Drop-off generates +Score flash + audio cue + animated shipment lift (post-MVP). |
| **Fit** | Industrial extraction fantasy узгоджена з усіма visuals і audio. |

---

## 2. Engineer, Not Soldier

**Identity statement.** Гравець — менеджер інженерної expedition, не воєначальник. Юніти — переобладнане шахтарське / інженерне обладнання, не армія. Натхнення: Dead Space (інженер з інструментом, не солдат з автоматом).

### Player Goal

Розгорнути добувну операцію, де навіть combat capability читається як industrial improvisation (drill arm з режимом self-defense, перенацілений riveter як turret, перероблений conveyor як attack-conveyor).

### What It Allows

- Combat юніти як industrial-derived (drilling rig з combat mode, salvage walker, repurposed lifter).
- Defensive structures як industrial nodes (perimeter charge launchers, salvaged plasma cutter turrets).
- Visual flavor: heavy industrial silhouettes, exposed mechanisms, welded armor, danger striping.

### What It Forbids

- Humanoid soldier units (infantry, troopers з gun-and-helmet read).
- Hero units (single named character з cinematic role).
- Cinematic creature-like player units.
- Military vehicle aesthetic (tanks, APCs, gunships, fighter jets, mechs з військовою функцією як primary read).
- Animation-heavy hand-to-hand combat.

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | Юніт із read "mining rig" одразу читається як economy unit. Combat-mode silhouette change — telegraphed transition. |
| **Motivation** | Trade-off "every combat unit could be mining" створює tension. |
| **Response** | Engineer units виконують прості machinery actions; response feels mechanical, не cinematic. |
| **Satisfaction** | Drilling, mechanical pistons, heavy industrial impact effects. |
| **Fit** | Visual identity консистентна; тільки industrial visual language. |

---

## 3. One Resource, Many Tradeoffs

**Identity statement.** У грі один основний ресурс — ферроніт, що є universal exchange currency. Економіка тримається на постійному виборі, куди витратити одну валюту: score / capacity / defense / production / development.

### Player Goal

Максимізувати throughput ферроніту, водночас вирішуючи між інвестицією у score (ship to orbit) і інвестицією у viability (capacity, defense, production capability).

### What It Allows

- Single-currency economy (Ferronite spent на builds, units, capacity expansion, defensive structures, doctrine purchases post-MVP).
- Score conversion як end-state of currency.
- Multi-axis tradeoff design (capacity vs offense vs defense vs immediate score).

### What It Forbids

- Multi-resource economy (gold + wood + oil; food + mana + stamina).
- "Soft" сурогати ресурсу (XP, fame, reputation), що дублюють currency роль.
- Mechanics, де currency не interchangeable (e.g., "military credits" окремо від "economy credits").

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | Один pool readable у HUD. Cost / available — single number comparison. |
| **Motivation** | Кожен spend = opportunity cost проти score. Direct decision pressure. |
| **Response** | Build cost feedback instant local, server confirms. |
| **Satisfaction** | Score flash при drop-off ясний; spend "drains" readable у HUD. |
| **Fit** | Industrial mono-currency узгоджена з narrative (corporate contract paid у ferronite tonnage). |

---

## 4. Capacity Is Strategy

**Identity statement.** Unit capacity — це не пасивний ліміт, а **other "resource"**. Гравець постійно вирішує, наскільки розширювати capacity (spend Ferronite на capacity-extending structures) чи тримати tight roster і інвестувати в інше.

### Player Goal

Спланувати unit roster розмір як strategic decision: tight з high-utility units проти broad з expendable workforce. Кожне rозширення capacity — це significant Ferronite spend, що не йде у score.

### What It Allows

- Capacity-expanding structures (Logistics Hub додає +5 MaxUnits per orbital drop, per [`10_Orbital_Delivery`](10_Orbital_Delivery.md)).
- Unit cost diversity (cheap workers vs expensive specialists), де capacity gating робить вибір значущим.
- Future doctrines (post-MVP), що по-різному впливають на capacity efficiency.

### What It Forbids

- Implicit "infinite cap" або soft caps без gameplay cost.
- Capacity, що зростає автоматично з часом без player decision.
- Capacity decoupled від economy (e.g., "capacity = час від старту").

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | HUD показує `CurrentUnits / MaxUnits`. Cap blocked → failure feedback. |
| **Motivation** | Cap forces strategic roster shaping. Не just "build more". |
| **Response** | Production blocked при cap — immediate UI signal. |
| **Satisfaction** | Cap expansion = visible "expansion of options" moment. |
| **Fit** | Industrial logistics fantasy — corporation обмежує число розгорнутих машин одночасно. |

---

## 5. Corporate Rivalry, Not Hero War

**Identity statement.** Конфлікт між гравцями — корпоративна гонка за extraction quota, не епічна war story. Без heroes, без personalities, без cinematic protagonists. Гравець — це менеджер remote operation, виконавчий вектор корпоративного контракту.

### Player Goal

Випередити opponent corporation у extraction throughput. Сценарій конфлікту — економічний, не моральний. Перемога — better extraction logistics, не military triumph.

### What It Allows

- Asymmetric **corporate doctrines** як post-MVP differentiator (mining doctrine, logistics doctrine, defensive doctrine, extraction efficiency, automated machinery style).
- Multiple corporate identities (color, logo, voiceover tone) як narrative layer без mechanical asymmetry у MVP.
- PvP як economic race з sabotage / interference possibilities.

### What It Forbids

- "Biological races" з різною біологією або магією.
- "Multiverse factions" або "alternative reality factions".
- Hero units, named protagonists, cinematic centerpiece characters.
- Narrative arcs focused on individual heroism or moral struggle.
- Asymmetric magic / supernatural abilities.

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | Opponent — це інша corporation з тими ж rules, просто player-controlled (або AI-controlled). Чіткий "як я, але по інший бік". |
| **Motivation** | Race fantasy — visible opponent score створює постійний motivation pressure. |
| **Response** | Opponent actions readable through map state, не cinematic events. |
| **Satisfaction** | Score lead = readable bragging right; final-second drop-off може перевернути матч. |
| **Fit** | Industrial corporate fantasy узгоджена через всі layers (visual, audio, narrative copy). |

---

## 6. SWARM as Environmental Pressure

**Identity statement.** SWARM — це **environmental / ecological threat**, не playable third faction. Це force-of-nature, що тисне на обох гравців пропорційно до їхнього видобутку. SWARM створює natural escalation curve без manual designer pacing.

### Player Goal

Захистити operation від ескалюючої SWARM aggression, балансуючи investment у defense проти score push. SWARM не може бути "переможений" — тільки витриманий до timer expiry.

### What It Allows

- AI-only SWARM faction.
- SWARM, що реагує на player economy через `FerroniteThreatValue` (raw Ferronite, складений у контейнерах на MainBase; зростає при drop-off, спадає при launch на орбіту).
- SWARM tiered units (Grunt MVP; Elite / Brood / Siege post-MVP).
- SWARM як design tool для economy self-balancing (більше нескинутого Ferronite на базі = більший threat; відправка на орбіту знижує тиск).

### What It Forbids

- Player control над SWARM (commanding, summoning, directly buffing).
- SWARM як selectable playable faction.
- SWARM, що не реагує на economy (constant predetermined waves).
- SWARM, що завершує матч (e.g., "wipe out → instant lose" — match завершується по timer + score).

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | SWARM aggression HUD indicator readable; wave spawn telegraph; minimap markers. |
| **Motivation** | Threat = constant tension; кожен mining decision adds threat. |
| **Response** | Defensive structures, Salvage Walker-class defenders, positioning — все player response tools. |
| **Satisfaction** | Repelling wave with defense investment — readable success. |
| **Fit** | EREBUS-9 ecosystem react до extraction — узгоджено з worldbuilding. |

---

## 7. Simple Machines, Strong Readability

**Identity statement.** Production budget і visual clarity вимагають, щоб усі gameplay-active обʼєкти були simple machines: бури, поршні, гусениці, крани, конвеєри, прості turret-like nodes, світлові індикатори. Жодних cinematic skeletal anims, animation-heavy juggernauts, hero rigs.

### Player Goal

Прочитати function обʼєкта за ≤ 1 second з map distance. Workflow, де гравець за оглядом upgrade-я / battlefield розуміє, що мається на увазі.

### What It Allows

- Mechanical animations (rotation, pistons, conveyor loops, drill spins, crane swings).
- Simple turret rotation animations.
- Skeletal animation тільки для walkers, що clearly industrial (treaded, tripod, quadruped excavator).
- Modular visual kits (interchangeable parts, swappable arm attachments) для variety без anim cost.

### What It Forbids

- Humanoid combat animations (punches, kicks, dramatic poses).
- Hero unit animation sets (idle variants, victory poses, cinematic transitions).
- Creature-like skeletal animation з organic motion curves.
- Animation-heavy юніти, які потребують >2 hours specialized animator time на one-off behavior.

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | Силует читається з top-down camera. Function = visible mechanical action. |
| **Motivation** | Гравець довіряє, що обʼєкт робить те, що виглядає. |
| **Response** | Animation = function feedback. Drill spinning = mining. Turret rotating = engaging. |
| **Satisfaction** | Mechanical impact + sparks + industrial audio. |
| **Fit** | Узгоджено з Engineer-Not-Soldier і Industrial Extraction First. Production cost — низький. |

---

## 8. Simple Core, Combinatorial Depth

**Identity statement.** Перший playable будується навколо простих, зрозумілих і швидко-перевіряємих рішень. Складність народжується з **поєднання** простих систем, не з complexity самих систем. Гра — не каталог фіч, а grid простих елементів, що множаться один на одного.

### Player Goal

Зрозуміти гру за хвилини, не за години. Робити стратегічні рішення, що випливають з зрозумілих базових механік, а не з вивчення rule-book. Бачити, як прості дії складаються у складні наслідки.

### What It Allows

- Mechanics, що пояснюються максимум у 1-2 реченнях.
- Mechanics, що зрозумілі без великого туторіалу.
- Mechanics, що дають веселий результат уже у першій ітерації.
- Mechanics, що масштабуються через комбінації, а не через додавання складності.
- Combinatorial depth: емерджентна складність із поєднань простих елементів.
- Content scaling (нові юніти / будівлі / ресурсні режими) як спосіб розширення, не нові правила.

### What It Forbids

- Mechanics, що потребують > 1-2 речень для пояснення.
- Mechanics, які вимагають великого туторіалу або UI-tutorial overlay.
- Mechanics, що не дають веселий результат у v1 ("fun колись пізніше").
- Mechanics, які розширюються лише додаванням нових правил, а не нового контенту.
- Single-purpose complex mechanics на користь multiple simple composable mechanics.
- Перевантаження гравця правилами замість поступового розкриття емерджентної глибини.

### Combinatorial Depth Scaling

Орієнтир для content density:

- **1 юніт** уже створює гру.
- **3 юніти** створюють взаємодію.
- **2 будівлі** створюють вибір.
- **5 будівель** створюють стратегію.

Глибина гри **народжується**, не **проектується**, з:

- Поєднання простих систем.
- Рішень гравця.
- Темпу матчу.
- Взаємодії механік між собою.

### MVP Production Rule

Кожна нова механіка проходить 5-питальний gate:

1. **Чи весело це вже зараз?**
2. **Чи зрозуміло це новому гравцю?**
3. **Чи додає це новий тип рішення?**
4. **Чи можна це реалізувати дешево і швидко?**
5. **Чи масштабується це далі контентом?**

Якщо відповідь "ні" хоча б на декілька пунктів:

- Mechanic не входить у MVP.
- Йде у backlog, спрощується, або повністю відкидається.

### Design Philosophy Stack

Гра не будується через **кількість систем**. Гра будується через (in priority order):

1. **Читабельність** — гравець бачить, що відбувається.
2. **Швидкість розуміння** — minutes-to-grasp, not hours.
3. **Приємний game feel** — feedback layer (per Pillar 7 і feedback matrix у TDD/12).
4. **Варіативність простих речей** — combinatorial design.
5. **Хороша взаємодія між базовими механіками** — composition over feature stacking.

Складність повинна з'являтися **природно під час гри**, а не через перевантаження гравця правилами.

### 5-Component Check

| Component | Implication |
| --- | --- |
| **Clarity** | Mechanic readable у 1-2 sentence summary. Player знає що robить before clicking. |
| **Motivation** | Each mechanic adds new decision type — not redundant variant of existing. |
| **Response** | Mechanic playable у v1 — не "буде весело після 3-х ітерацій". |
| **Satisfaction** | Combinatorial multiplication: 3 units = interaction, 5 buildings = strategy. Гравець відкриває нові шари без нових правил. |
| **Fit** | Узгоджено з Indie-Honest Scope (meta-rule) і Pillar 7 (visual readability). |

---

## 9. Technical Pillar: Server-Authoritative, Data-Driven, GAS-First

**Identity statement.** Engineering fundament, що обслуговує gameplay pillars. Кожна gameplay decision реалізується **через** server authority, Data Assets і GAS — не **замість** них.

Це consolidation з попередніх pillars (Server-Authoritative Clarity, Data-Driven Content, Component-Driven Behavior) у один technical pillar, щоб не змішувати engineering fundament з gameplay identity.

### Technical Goal

Забезпечити, щоб усі gameplay rules з pillars 1–7 reliably реалізувалися у multiplayer-correct, designer-tunable, GAS-native код.

### What It Allows

- Будь-яка mechanic, що відповідає gameplay pillars і реалізується через documented stack.
- Composition over inheritance (component-first per ADR_0007 building-as-pawn).
- Data Asset як primary tuning surface.
- GAS Attributes / Effects / Tags як runtime state owners.

### What It Forbids

- Client-authoritative gameplay logic.
- Hardcoded balance у C++.
- Magic-string gameplay tags.
- Deep inheritance замість composition.
- Blueprint gameplay authority (per `/CONTRIBUTING.md`).
- Lyra patterns, GameFeatures-first, MassEntity-based gameplay (per ADR_0005, ADR_0006).

### 5-Component Check

Technical pillar не оцінюється через gameplay 5-component — він **enables** правильну реалізацію інших pillars без додавання identity. Замість 5-component:

| Quality | Implication |
| --- | --- |
| **Determinism** | Server-authoritative result, same input → same observable state. |
| **Iteration speed** | Data-driven balance tuneable без recompile. |
| **Replication correctness** | GAS handles attribute, ability, tag replication out of box. |
| **Modular architecture** | Composition allows independent system slices. |

---

## Pillar Conflict Tie-Break

При конфлікті pillars порядок resolution (від найвищого пріоритету):

1. **Simple Core, Combinatorial Depth (Pillar 8)** — design philosophy gate. Якщо mechanic порушує 5-question MVP rule, fails незалежно від інших fits.
2. **Engineer, Not Soldier (Pillar 2)** — visual / fantasy identity, без якої проект втрачає differentiation.
3. **Industrial Extraction First (Pillar 1)** — gameplay loop ідентичність.
4. **One Resource, Many Tradeoffs (Pillar 3)** — economy simplicity.
5. **Capacity Is Strategy (Pillar 4)** — strategic depth axis.
6. **SWARM as Environmental Pressure (Pillar 6)** — threat layer.
7. **Corporate Rivalry, Not Hero War (Pillar 5)** — narrative framing.
8. **Simple Machines, Strong Readability (Pillar 7)** — production-friendly constraint.
9. **Server-Authoritative, Data-Driven, GAS-First (Pillar 9)** — technical fundament (не gameplay identity, тому останній).

Окремо: **Indie-Honest Scope** (meta-rule, scope overrider) — якщо нова mechanic порушує scope, її переносять у Backlog незалежно від pillar fit. Pillar 8 і Indie-Honest працюють разом: Pillar 8 формулює **як** проектувати mechanic; Indie-Honest вирішує **чи** взагалі додавати.

## What Changed From Previous Pillars

Попередня версія мала 5 pillars: Server-Authoritative Clarity / Match-Based RTS / Data-Driven Content / Indie-Honest Scope / Component-Driven Behavior. З них:

- **Server-Authoritative Clarity + Data-Driven Content + Component-Driven Behavior** → consolidated у Pillar 9 (Technical, was Pillar 8 у попередній ревізії).
- **Match-Based RTS** → перенесено у `07_Match_Flow` як design constraint, а не pillar identity.
- **Indie-Honest Scope** → перенесено у meta-rule (tie-break overrider), не identity pillar.

Identity тепер несе 8 gameplay pillars (1–8), що відповідають owner directive 2026-05-16:

- Pillars 1–7 — game identity (industrial extraction, engineer-not-soldier, single resource, capacity strategy, corporate rivalry, SWARM pressure, visual readability).
- **Pillar 8 (новий, додано 2026-05-16)** — Simple Core, Combinatorial Depth: design philosophy gate, MVP Production Rule (5 questions), combinatorial depth scaling, design philosophy stack.
- Pillar 9 — technical fundament (renumbered з 8 → 9).

## References

- Gameplay loop, побудований на pillars — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md).
- Visual / lore implications — [`Lore_Setting`](Lore_Setting.md).
- Engineering implications — [`../TDD/00_Technical_Overview`](../TDD/00_Technical_Overview.md).
- Scope discipline — [ADR_0006_Indie_Scope_No_Overengineering](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md).
- Pillar-driven validation framework — [`../../SKILLS/gp-mechanics-validator/SKILL.md`](../../SKILLS/gp-mechanics-validator/SKILL.md).
- Design framework (5-component rubric) — [`../../SKILLS/game-design-framework/SKILL.md`](../../SKILLS/game-design-framework/SKILL.md).
