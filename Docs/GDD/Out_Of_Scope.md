# Out Of Scope

## Scope

Цей файл фіксує механіки і технічні напрями, які свідомо не входять у MVP GrimProtocol і **не плануються** у найближчий post-MVP горизонт. Це відрізняє Out_Of_Scope від `Backlog`, де лежать validated ідеї з reconsider triggers.

## MVP Exclusions

- Campaign save/load (між матчами немає persistent state у MVP).
- Replay system (запис і відтворення матчу).
- Procedural maps (всі maps створюються hand-authored).
- Fog of War як повна vision/memory система (basic visibility tag може бути для MVP).
- Multi-resource economy (тільки Ferronite).
- Large faction roster (тільки одна симетрична фракція + SWARM environmental threat).
- Modding або generic RTS framework (custom games, scenario editor).
- Dedicated server production target (MVP — listen server only; архітектура не блокує future dedicated migration).
- Lyra architecture, GameFeatures-first architecture, MassEntity-based gameplay (per [ADR_0005](../Architecture_Decisions/ADR_0005_No_Lyra.md), [ADR_0006](../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md)).
- Off-world meta progression як **persistent** system (intra-match "send to orbit = score" — у MVP; persistent upgrades між матчами — у `Backlog`).

## Hard Bans (Permanent — Not Just MVP)

Наступне заборонено **назавжди**, не тільки на MVP. Будь-який attempt повернути ці пункти — review-blocking за порушення canonical pillars з [`01_Game_Pillars`](01_Game_Pillars.md).

### Narrative / Lore Bans (Pillar 5)

- **Time travel** як plot, faction backstory, gameplay tool, або mechanic.
- **Multiverse** як narrative framing, faction identity, або mechanical differentiator.
- **Alternative realities** як пояснення фракцій, ворогів, технологій.
- **Magic / supernatural** abilities (psychic, mystical, divine).
- **Hero protagonists** і character-centric narrative arcs.
- **Asymmetry через біологічні раси.**
- **Asymmetry через магію / supernatural traits.**

### Visual / Animation Bans (Pillar 2, Pillar 7)

- **Military vehicle aesthetic** — танки, APCs, gunships, fighter jets, bombers.
- **Bipedal humanoid soldiers** — infantry, troopers, riflemen, snipers.
- **Combat mech aesthetic** — Battletech / Gundam / MechWarrior-style mechs з military framing.
- **Hero unit visual centerpieces** — outsize, cinematic, hand-animated juggernauts.
- **Creature-like player units** — organic motion, biological silhouettes.
- **Humanoid combat animations** — punches, kicks, dramatic poses, melee combos.
- **Cinematic skeletal animation sets** для player-controlled units (idle variants, victory poses, cinematic transitions).
- **Animation-heavy юніти** що потребують >2 hours specialized animator time на one-off behavior.

### Gameplay Identity Bans (Pillar 1, Pillar 3, Pillar 4, Pillar 6)

- **Multi-resource economy** з non-interchangeable currencies.
- **Capacity decoupled від economy** (auto-scaling cap, time-based cap).
- **Player control over SWARM** — direct commands, summoning, buffs, indirect manipulation як primary mechanic.
- **SWARM as playable faction.**
- **SWARM that doesn't react to economy** (constant predetermined waves).
- **Hero units** як combat centerpiece з unique anim set.
- **Military RTS metagame** — focus на army composition / push-the-base як primary loop.

## Note on AI Opponent

AI opponent **IS у MVP** (reaffirmed 2026-05-16). Required для singleplayer playable path.

- Scope: primitive state machine з 4-5 станами (`Explore → Mine → Ship → Order → Defend`), реагує на core economy / orbital cycles.
- Implementation: `AGP_AIController` (server-only), low-frequency decision tick (2-5 s), реагує на map state + own resource pool.
- AI **підпадає** під ту ж saint orbital model — orders drops, ships containers, attacked by SWARM.
- Не AAA — не utility AI, не goal-oriented planning, не learning. Звичайний state machine достатньо для playable v1.

Advanced AI (goal-oriented, utility, learning) — Out_Of_Scope для MVP. Reconsider — окремий task post-MVP.

## Review Rule

Будь-який attempt повернути out-of-scope пункт у MVP має пройти feature validation з [Backlog](Backlog.md) і отримати окреме architecture/design рішення (ADR draft, якщо це архітектурне).

Hard Bans з секції вище — **не підлягають reconsider** без явного pillar amendment у [`01_Game_Pillars`](01_Game_Pillars.md). Pillar amendment = окремий ADR з owner sign-off, не routine feature decision.

## References

- Backlog (validated post-MVP ideas) — [`Backlog`](Backlog.md).
- Architecture decisions — [`../Architecture_Decisions/`](../Architecture_Decisions/README.md).
- AI opponent (now in MVP, primitive only) — [`03_Factions`](03_Factions.md).
- Game pillars (canonical identity rules) — [`01_Game_Pillars`](01_Game_Pillars.md).
- Lore hard bans — [`Lore_Setting`](Lore_Setting.md).
- Mechanics validator framework — [`../../SKILLS/gp-mechanics-validator/SKILL.md`](../../SKILLS/gp-mechanics-validator/SKILL.md).
