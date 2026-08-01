# Project Overview

## Pitch

GrimProtocol — інді RTS у real-time tactical стилі. Гравець керує **видобувною експедицією** корпорації, що добуває рідкісний ферроніт на ворожій планеті EREBUS-9 під тиском конкурентів і місцевого рою. Multiplayer-first, server-authoritative, delivery-quota-based, 10-хвилинні матчі.

**Core fantasy:** видобути ресурс → накопичити у containers на базі → відправити на орбіту → витратити Orbital Ferronite на orbital-delivered units / structures → доставити quota швидше та ефективніше за конкурентів. Не класична військова RTS про тотальне знищення — гра про логістику, контроль території, ризик-vs-greed.

**Усе на планеті прибуває з орбіти.** Player не будує локально — order-ить drop pods за Orbital Ferronite. Helldivers-style reinforcement fantasy.

Worldbuilding — [`Lore_Setting`](Lore_Setting.md). Orbital delivery деталі — [`10_Orbital_Delivery`](10_Orbital_Delivery.md). Fog of War — [`11_Fog_of_War`](11_Fog_of_War.md).

## Genre

Real-Time Strategy (top-down RTS з RTS camera).

## Engine

Unreal Engine 5.8.1. C++ first, Blueprint — для UI і presentation.

## Target Audience

PC players, що цінують:

- Швидкі match-based RTS (10 хв per game).
- Економну механіку (single resource, single combat archetype, без overwhelming production trees у MVP).
- Symmetric PvP з environmental pressure (SWARM waves) як engagement multiplier.
- Industrial sci-fi setting (Deep Rock Galactic — primary reference; Aliens 2 atmosphere; Starship Troopers pacing of hostile ecology — **не** military aesthetic).

## Team Profile

Інді-команда. MVP scope і production cost — обмежені. Кожна додаткова механіка проходить feature validation checklist у [`/CONTRIBUTING.md`](../../CONTRIBUTING.md) і [`gp-mechanics-validator`](../../SKILLS/gp-mechanics-validator/SKILL.md).

## MVP Scope

### Core Multiplayer Target (MVP)

- Steam matchmaking.
- 2 players, host/client (listen server), symmetric corporate colonial faction.
- 1 RTS map.
- PvP score race over 10-minute timer.
- Replicated gameplay loop.
- SWARM faction (Variant 2 "background") тисне на обох гравців.

### Singleplayer Target (MVP)

- Локальний запуск тієї ж мапи.
- 1 player vs. primitive AI opponent (state machine: explore → mine → ship → order → defend). Деталі — [`03_Factions`](03_Factions.md).
- RTS camera, zoom, rotation.
- Selection, move command, attack command, mine command, order-drop command.

### Minimal Gameplay Set (MVP)

- **Main Base** (initial expedition pod — already deployed at match start; drop-off + container storage + ship-to-orbit launch site).
- **Worker unit** (mining + repair; arrives via orbital drop after order).
- **Salvage Walker** (combat-capable industrial defender; orbital drop).
- **Defensive Turret** (oborona проти SWARM waves; orbital drop placement).
- **Logistics Hub** (was "Assembly Yard"; orbital drop placement; +5 MaxUnits cap; expanded storage cap).
- **Ferronite** як MVP resource у **two states**: Planetary (containers at base, not spendable) → Orbital (currency, spendable on orbital drops). Containers ship to orbit when full.
- **3-Level Fog of War**: Unexplored / Explored / Actively Visible.
- **SWARM waves**: intensity масштабується з total Ferronite **shipped to orbit** (per Pillar 6 — greed signals planet to attack harder; **не** time-based escalation).
- **Orbital Delivery System**: spend Orbital Ferronite → select drop target → pod arrives → asset deployed.
- 10-min match timer з **delivery-quota** win condition (highest Orbital Ferronite shipped wins if neither hits quota first).

### Out of MVP

- Advanced AI (goal-oriented, utility, learning).
- Procedural / generated maps.
- Replay system.
- Save/load campaign.
- Multiple resource types.
- Repair/upgrade modules.
- Black market / smuggler alternate buyers.
- Corporate economy layer (external contracts).
- Stealth / scanner reveal mechanics.
- Generic RTS framework (modding, custom games).
- Large faction roster (2+ asymmetric corporate rivals).
- Advanced economy (multi-resource, market, trade).
- Off-world orbit meta upgrades між матчами (Helldivers-like persistent).
- Massive UI (research trees, advanced unit cards).
- SWARM design Variants 1, 3, 4.

## Pillars (One-Line)

Gameplay pillars (identity):

1. **Industrial Extraction First** — гра про видобуток, не про війну.
2. **Engineer, Not Soldier** — гравець інженер з переобладнаними інструментами, не воєначальник.
3. **One Resource, Many Tradeoffs** — Ferronite як universal exchange currency, multiple decision axes.
4. **Capacity Is Strategy** — unit cap як свідомий strategic resource, не пасивний ліміт.
5. **Corporate Rivalry, Not Hero War** — конфлікт корпорацій, без героїв, без військового епосу.
6. **SWARM as Environmental Pressure** — рій як ecological force-of-nature, AI-only, non-playable.
7. **Simple Machines, Strong Readability** — прості mechanical animations, production-friendly visual constraint.

Technical pillar (foundation):

8. **Server-Authoritative, Data-Driven, GAS-First** — engineering fundament, що обслуговує gameplay pillars.

Meta-rule (scope discipline):

- **Indie-Honest Scope** — те, що в MVP, реально playable. Tie-break overrider для всіх pillars.

Розширено — [`01_Game_Pillars`](01_Game_Pillars.md).

## Success Criteria (MVP Done)

- 2 players joining via Steam, partying у lobby, starting match.
- Кожен гравець керує Main Base + Workers, видобуває ферроніт з deposits, повертає workers у Main Base для drop-off, бачить score інкремент.
- SWARM waves запускаються після ~60s, escalating у часі, цілять незахищені workers і buildings.
- Гравець може замовляти з орбіти Logistics Hub (для +5 cap + storage) і Defensive Turret (для оборони бази). Per [10_Orbital_Delivery](10_Orbital_Delivery.md).
- Salvage Walker виконує move/attack команди, atackує SWARM і enemy units, отримує damage, помирає.
- Building отримує damage, руйнується.
- Match завершується на 10:00 timer expiry; server compute score, declare winner, replicate result.
- Singleplayer: AI opponent виконує build → mine → produce → attack loop, генерує власний score.
- No desync, no client-authoritative gameplay drift.
- Match completes у 10 хвилин (hard cap, без extension).

## References

- Technical mapping — [`../TDD/00_Technical_Overview.md`](../TDD/00_Technical_Overview.md).
- Architecture rules — [`/CONTRIBUTING.md`](../../CONTRIBUTING.md).
- Worldbuilding — [`Lore_Setting`](Lore_Setting.md).
- Game pillars — [`01_Game_Pillars`](01_Game_Pillars.md).
- Match flow і timing — [`07_Match_Flow`](07_Match_Flow.md).
- Win conditions і score — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
