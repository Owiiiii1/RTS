# GrimProtocol Documentation

Локальна wiki проєкту. Markdown-only, плоска навігація, source of truth для гейм-дизайну, технічних рішень і архітектури.

## Перший крок — для всіх

➡️ **[Game_Pitch.md](Game_Pitch.md)** — гейм-спіч українською. Без технічного жаргону. Що це за гра, передісторія, як виглядає матч, як перемогти, чим відрізняється від інших RTS. **Читати всім** — і дизайнерам, і художникам, і програмістам, і продюсерам — як перший крок у проєкт.

## Структура

- [Development/DOCUMENTATION_INDEX.md](Development/DOCUMENTATION_INDEX.md) — **единый индекс**: sources of truth, deprecated docs, conflicts, current NEXT task.
- [Game_Pitch.md](Game_Pitch.md) — **non-technical onboarding** (Ukrainian, no tech-speak). What the game is, why it's interesting, how a match plays.
- [GDD/](GDD/README.md) — Game Design Document. Gameplay-canonical: pillars, gameplay loop, factions, units, buildings, resources, match flow, win/lose conditions, UI/UX, orbital delivery, fog of war.
- [TDD/](TDD/README.md) — Technical Design Document. Engineering-canonical: module architecture, GAS, multiplayer, RTS pipeline, unit/building/resource architecture, Steam matchmaking, camera, UI/MVVM, orbital delivery system, fog of war, consolidated architecture proposal.
- [Architecture_Decisions/](Architecture_Decisions/README.md) — ADRs. Прийняті архітектурні рішення з context, decision, consequences.
- [Development/](Development/README.md) — operational rules. Coding rules, naming, git workflow, slice template, per-task specs.
- [Archive/](Archive/README.md) — legacy документи попереднього проєкту, reference-only.

## Canonical Files (root)

- [/CONTRIBUTING.md](../CONTRIBUTING.md) — engineering rules, multiplayer/GAS/data-driven discipline, hard bans, DoD.
- [/STYLE.md](../STYLE.md) — naming, code style, asset placement, content layout.
- [/README.md](../README.md) — project landing page, plain-words pitch, document map.

## Conventions

- Body — українською. Headings — англійською.
- Кожна папка має `README.md` з coverage та linked sub-pages.
- GDD і TDD синхронізовані: gameplay decisions з GDD мають technical mapping у TDD; technical constraints з TDD обмежують GDD ambitions.
- Кожне архітектурне рішення фіксується у `Architecture_Decisions/` перед імплементацією.
- Game_Pitch — first-touch document для не-технічних людей; завжди up-to-date з GDD pivots.

## Reading Order

### Для будь-кого, хто новий у проєкті

1. [Game_Pitch.md](Game_Pitch.md) — зрозуміти, що це за гра і чому це цікаво.
2. [Development/DOCUMENTATION_INDEX.md](Development/DOCUMENTATION_INDEX.md) — sources of truth, NEXT task, known conflicts.
3. [GDD/00_Project_Overview](GDD/00_Project_Overview.md) — формальний overview.
4. [GDD/01_Game_Pillars](GDD/01_Game_Pillars.md) — canonical identity rules.

### Для контриб'юторів (after onboarding steps)

5. [TDD/00_Technical_Overview](TDD/00_Technical_Overview.md).
6. `/CONTRIBUTING.md` і `/STYLE.md`.
7. [Architecture_Decisions/](Architecture_Decisions/README.md) — read all (їх ~9, всі короткі).
8. [TDD/13_Architecture_Proposal](TDD/13_Architecture_Proposal.md) — consolidated implementation map.
9. Targeted GDD/TDD page по area-of-work.

### За роллю

| Якщо ти… | Читай у такому порядку |
| --- | --- |
| **Геймдизайнер** | Game_Pitch → `GDD/01_Game_Pillars` → решта GDD |
| **Художник / звуковик** | Game_Pitch → `GDD/Lore_Setting` → `GDD/09_UI_UX` → `TDD/12_UI_Architecture` (Feedback Matrix секція) |
| **Програміст gameplay** | Game_Pitch → `/CONTRIBUTING.md` → `/STYLE.md` → `TDD/13_Architecture_Proposal` → `TDD/02_GAS_Architecture` → relevant slice |
| **Програміст UI** | Game_Pitch → `TDD/12_UI_Architecture` → `Development/Slice_Template` |
| **Продюсер / PM** | Game_Pitch → `Development/Claude_Task_Backlog` → `GDD/Backlog` + `GDD/Out_Of_Scope` → `GDD/12_Session_Tuning_And_Calibration` (ownership matrix) |
| **QA / playtester** | Game_Pitch → `GDD/First_Playable_Match` → `GDD/12_Session_Tuning_And_Calibration` §Playtest Metrics → playtest scenario tables у кожному TDD section |
| **Балансер / Designer (tuning)** | Game_Pitch → `GDD/12_Session_Tuning_And_Calibration` → DataAsset references + Cheats list |

## What's Recent (Pivots)

- **2026-05-16** — Orbital Delivery Model pivot. Усе крім initial MainBase прибуває з орбіти. Container System (two-state Ferronite). Helldivers fantasy. Win condition → delivery quota. AssemblyYard → Logistics Hub. See [ADR-0009](Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md).
- **2026-05-16** — 3-Level Fog of War у MVP (was deferred). See [GDD/11_Fog_of_War](GDD/11_Fog_of_War.md) і [TDD/15_Fog_of_War](TDD/15_Fog_of_War.md).
- **2026-05-16** — AI opponent reaffirmed у MVP (primitive state machine, `AAIController` per [ADR-0008](Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md)).
- **2026-05-16** — New Pillar 8 (Simple Core, Combinatorial Depth) inserted. Technical Pillar renumbered 8 → 9.
- **2026-05-16** — Soft references mandatory for all content (ADR-0002 reaffirmed з explicit table).
- **2026-05-16** — Common UI + MVVM mandatory for all HUD/widget work.
