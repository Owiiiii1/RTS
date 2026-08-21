# Game Design Document

Gameplay-canonical документація GrimProtocol. Без implementation деталей — для них див. [TDD/](../TDD/README.md).

## Pages

- [00_Project_Overview](00_Project_Overview.md) — pitch, target audience, MVP.
- [01_Game_Pillars](01_Game_Pillars.md) — core pillars, що тримають gameplay identity.
- [02_Core_Gameplay_Loop](02_Core_Gameplay_Loop.md) — second-to-second, minute-to-minute, match-to-match loops.
- [03_Factions](03_Factions.md) — фракції MVP, SWARM third-party і AI scope.
- [04_Units](04_Units.md) — список юнітів MVP, ролі, capability tags.
- [05_Buildings](05_Buildings.md) — список будівель MVP, ролі, dependencies.
- [06_Resources](06_Resources.md) — Ferronite economy, mining, drop-off.
- [07_Match_Flow](07_Match_Flow.md) — phases матчу, timing, SWARM escalation.
- [08_Win_Lose_Conditions](08_Win_Lose_Conditions.md) — score-based primary, time-out resolve.
- [09_UI_UX](09_UI_UX.md) — screens, canonical two-bar production HUD IA, and MainBase PURCHASE (UNITS / BUILDINGS / DEFENSE) inside the bottom-right panel.
- [10_Orbital_Delivery](10_Orbital_Delivery.md) — orbital drop pod mechanic, two-state resource, order flow, drop zone rules.
- [11_Fog_of_War](11_Fog_of_War.md) — 3-level FoW (Unexplored / Explored / Visible), sight sources, selection / combat / drop interactions.
- [12_Session_Tuning_And_Calibration](12_Session_Tuning_And_Calibration.md) — **production operational doc.** Session params, debug cheats, balance calibration workflow, canonical post-pivot core loop, reward loop, player flow states, parameters table, playtest metrics, hot/warm/cold change rules, ownership matrix.
- [13_Terrain_Engineering_And_Foundations](13_Terrain_Engineering_And_Foundations.md) — destructible voxel terrain, Worker leveling / site prep, per-cell foundation slabs, building placement dependency.
- [Lore_Setting](Lore_Setting.md) — EREBUS-9, Ferronite, SWARM ecology, visual style, hard lore bans.
- [First_Playable_Match](First_Playable_Match.md) — end-to-end player story, singleplayer + PvP, без gaps.
- [Backlog](Backlog.md) — validated ideas після MVP.
- [Out_Of_Scope](Out_Of_Scope.md) — свідомо відкладені або rejected напрями.

## Backlog / Out of Scope

Усе, що не входить у MVP, але має track-record:

- [Backlog](Backlog.md) — feature ideas, що пройшли validation checklist але не у MVP.
- [Out_Of_Scope](Out_Of_Scope.md) — explicitly rejected механіки і чому.

## Feature Validation

Перед новою механікою відповісти: чи потрібна для MVP, яку gameplay проблему вирішує, чи підсилює core loop, чи можна простіше, multiplayer complexity, GAS complexity, UI complexity, production cost, data-driven compatibility, required tags, required attributes, required Data Assets, scope creep risk, чи можна відкласти після MVP.

## Cross-Reference з TDD

| GDD сторінка | TDD сторінка |
| --- | --- |
| `02_Core_Gameplay_Loop` | `00_Technical_Overview`, `04_RTS_Selection_And_Commands` |
| `04_Units` | `05_Unit_Architecture`, `02_GAS_Architecture` |
| `05_Buildings` | `06_Building_Architecture` |
| `06_Resources` | `07_Resource_Architecture` |
| `07_Match_Flow` | `03_Multiplayer_Architecture` |
| `09_UI_UX` | (UI TDD — TBD) |
| `10_Orbital_Delivery` | `14_Orbital_Delivery` |
| `11_Fog_of_War` | `15_Fog_of_War` |
| `13_Terrain_Engineering_And_Foundations` | `16_Voxel_Terrain_And_Foundations` |
| `Lore_Setting` | (no direct TDD; informs visual / asset pipeline) |
