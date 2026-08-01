# Backlog

## Scope

Цей файл тримає gameplay-ідеї, які пройшли коротку feature validation, але свідомо не входять у first playable MVP target. Кожна ідея — окремий запис з context і reasoning.

## Feature Validation Questions

Перед додаванням нової механіки відповісти:

1. Чи потрібна для MVP?
2. Яку gameplay проблему вирішує?
3. Чи підсилює core loop?
4. Чи можна простіше?
5. Яка multiplayer complexity?
6. Яка GAS complexity?
7. Яка UI complexity?
8. Який production cost?
9. Чи сумісна з data-driven підходом?
10. Які required tags?
11. Які required attributes?
12. Які required Data Assets?
13. Який scope creep risk?
14. Чи можна відкласти після MVP?

Skill для повного analysis — [`gp-mechanics-validator`](../../SKILLS/gp-mechanics-validator/SKILL.md).

## Candidates

### Off-World Orbit Meta Upgrades

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** Це meta-progression layer (Helldivers-like) — persistent state між матчами, upgrade UI, balance шар, server backend або local profile. Конфліктує з пілар "Match-Based RTS" (один матч — одна gameplay arc, без persistence). Production cost — disproportionate для MVP, де ще не валідовано in-match loop.

**Notes:**

- Концепт: за кожен матч відправлений на орбіту ферроніт конвертується у meta currency.
- Meta currency витрачається на passive upgrades (faster mining rate, cheaper orbital drops, +1 starting Worker, etc.), що активуються на старті наступного матчу.
- У MVP залишається тільки **intra-match** "send to orbit = score", без persistence між матчами.

**Reconsider trigger:** після того як 2-player MVP loop стабільний у Steam playtests і базова balance pass завершена.

### SWARM Design Variant 1 — Distributed Corridors

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** Map-design heavy. Потребує специфічну map layout (один "SWARM corridor" як constant pressure, інші — для PvP). Це окрема map design pass, окрема spawn pipeline configuration, окремий wave routing.

**Notes:**

- Concept: один map entry постійно атакується SWARM units (як constant threat), інші входи — для player-vs-player conflict.
- Створює просторову асиметрію вибору агресії.

**Reconsider trigger:** після того як Variant 2 (background SWARM units) playtested і map design pipeline стабільний.

### SWARM Design Variant 3 — Influence Structures

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** Нова механіка з кількома sub-features: (a) inhibitor structure that suppresses SWARM spawn у радіусі на cooldown; (b) energy corridor that protects units in transit; (c) decoy structure that draws SWARM aggro. Кожне — окремий Building Data Asset, окрема Ability, окремий UI flow.

**Notes:**

- Створює tactical layer: гравець активно маніпулює SWARM threat, а не просто tank-ить.
- Defensive Turret з MVP — preview цієї філософії, але без active manipulation.

**Reconsider trigger:** після того як Variant 2 baseline visible у playtests і defender feel calibrated.

### SWARM Design Variant 4 — SWARM Manipulation (Aggressive)

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** Найбільший scope з 4 варіантів. Гравець опосередковано спрямовує SWARM waves проти опонента — або через SWARM attractor structures, або через ability що "evolves" SWARM у певній зоні, або через partial command над SWARM aggro vectors. Це повноцінна asymmetric warfare layer, що потребує:

- SWARM faction перебудована з passive-pressure у dynamic actor.
- Нова UI для SWARM manipulation tools.
- Нова balance система (як обмежити, щоб SWARM-як-weapon не зламав PvP).

**Notes:**

- Високопотенційний для emergent gameplay, але високий risk балансу.
- Тільки після того як вся MVP-baseline стабільна.

**Reconsider trigger:** post-MVP eval, після того як Variants 1 і 3 розглянуті і базова SWARM fiction стабілізована.

### Annihilation as Alternative Win Condition

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP win condition — score-based з 10-min cap ([`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md)). Annihilation був попереднім draft. У 10-min форматі annihilation створює anti-fun (early Main Base rush, no recovery). У довшому форматі (post-MVP, 20-30 хв match) annihilation як **secondary** win condition може повернути strategic depth.

**Notes:**

- Hybrid: основна перемога — score, але якщо опонент втрачає Main Base — instant win (early termination).
- Потребує playtest для перевірки, чи це не ламає score-pacing.

**Reconsider trigger:** після того як score-based MVP loop validated.

### Multi-Tier Ferronite Deposits

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP — один тип deposit з варіативною capacity. Multi-tier (Rich / Standard / Poor варіанти з різним yield per worker, capacity, depletion rate) додає balance complexity і нову UI permutation. Цінно для map design, але не критично для playable.

### Resource Depot / Supply Outpost

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP — drop-off тільки у Main Base. Додатковий Supply Outpost (Worker може drop-off у будь-який, скорочує trip distance) додає expansion mechanic і захист outposts як new objective. Цінно, але не у MVP — спрощує balance і map design.

### Tech Building

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP не має tech tree. Tech Building як передумова для advanced units / upgrades — це окремий gating layer, новий Building Data Asset, нова UI для tech state, balance шар "коли saw-it ahead unlock pump". Цінно для depth, не критично для playable MVP.

### Worker з Attack Capability

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP Worker — pure utility (mine + build), не атакує. Це навмисне rules: workers — м'яка ціль, що стимулює defensive turrets + Salvage Walker coverage. Worker з self-defense уберігає baseline tension. Post-MVP — variant Worker з weak self-defense tool (для AoE genres familiarity), зберігаючи industrial framing.

### Specialist Combat Units (Sniper / Siege / Anti-Armor / Anti-Air)

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP — один combat-capable unit (Salvage Walker). Specialist units додають composition decision, armor/damage type matchup, новий balance axis. Дуже цінно для tactical depth, але не критично для baseline playable loop. Per Pillar 2 (Engineer, Not Soldier) усі specialists мають читатися як industrial / engineering equipment.

### SWARM Multi-Tier (Elite / Brood / Siege SWARM)

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP — один SWARM тип (Grunt). Multi-tier SWARM units (e.g., Brood Mother що спавнить додаткових grunts на map, Siege SWARM що завдає damage будівлям із range, Elite SWARM як mid-match boss) додає SWARM depth, але потребує окремі Data Assets, AI behaviors, animation sets.

### Armor Type vs Damage Type Matchup

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP — deterministic HP-based damage без armor type system. Armor vs damage matchup (Light / Medium / Heavy armor; Piercing / Energy / Explosive damage) додає composition depth, але потребує нову UI для damage info, нову balance таблицю, нові tags.

### Surrender / Forfeit UI

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** MVP — без surrender (per [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md)). Гравець, що хоче вийти, disconnects. Це сирий UX. Surrender button з confirmation або vote-to-end (multiplayer) — UI polish, не критично для playable.

### Pings / Contextual Map Markers

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** Multiplayer communication tool — corner-case у 2-player MVP, де гравці часто на voice chat. Cool-to-have, але не критично.

### Score Timeline / Replay UI

**Status:** Backlog (deferred post-MVP).

**Validated:** 2026-05-16.

**Why deferred:** End-of-match screen у MVP — простий winner + final scores. Score timeline graph (line chart of score over time) — polish, що добавить readability post-match. Не critical для playable.

## References

- Feature validation framework — [`/CONTRIBUTING.md`](../../CONTRIBUTING.md), [`gp-mechanics-validator`](../../SKILLS/gp-mechanics-validator/SKILL.md).
- Out of scope (rejected outright) — [`Out_Of_Scope`](Out_Of_Scope.md).
