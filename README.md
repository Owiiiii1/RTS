# GrimProtocol

**Industrial extraction RTS.** Дві корпорації висадили видобувні експедиції на ворожу планету EREBUS-9. У них 10 хвилин, щоб викопати рідкісний метал і відправити його на орбіту швидше за конкурента, поки місцевий біологічний рій тисне на бази тим сильніше, чим більше вони викопали.

Helldivers-style orbital delivery + risk-vs-greed economy + 3-level fog of war + indie scope (UE 5.8.1, 2-player Steam matchmaking + singleplayer AI).

## What This Is, In Plain Words

Якщо ви новий учасник команди і не знаєте, що це за гра — **почніть тут:**

➡️ **[`Docs/Game_Pitch.md`](Docs/Game_Pitch.md)** — гейм-спіч українською. Без технічного жаргону. Що це за гра, як виглядає матч, як перемогти, чим відрізняється від інших RTS.

## Project Facts

- **Engine:** Unreal Engine 5.8.1.
- **Genre:** RTS (real-time strategy, top-down).
- **Match length:** 10 minutes (hard cap).
- **Multiplayer:** Steam matchmaking, 2 players, host/client (listen-server).
- **Singleplayer:** vs. primitive AI opponent (state machine), same map, same rules.
- **Project prefix:** `GP`.
- **Content path:** `/Game/GrimProtocol`.
- **Architecture direction:** GAS-first, multiplayer-first, data-driven-first, **orbital-delivery-first** (see ADR-0009).
- **UI framework:** Common UI + MVVM (server updates ViewModels; widgets bind to VMs).
- **Documentation language:** Ukrainian body text with English headings.

## Foundation Rules

- Gameplay implementation is C++ first; Blueprint — для UI / editor workflow / tuning.
- Gameplay state — server-authoritative і replicated intentionally.
- Runtime balance and tuning — у Data Assets (no hardcoded balance у C++).
- Content references — **soft only** (`TSoftObjectPtr` / `TSoftClassPtr`). Loaded via Asset Manager async.
- Gameplay Tags — primary state taxonomy. No magic-string tags.
- **No Lyra architecture / Experience System.**
- New gameplay code — only after documentation and review.
- **Pillar 8 (Simple Core, Combinatorial Depth):** any new mechanic passes 5-question MVP gate.

## Documentation Structure

```
Docs/
├── Game_Pitch.md              ← Start here for non-technical onboarding
├── README.md                  ← Docs landing page (technical + design)
├── GDD/                       ← Game Design Document (what the game is)
│   ├── 00_Project_Overview
│   ├── 01_Game_Pillars        ← Canonical identity rules
│   ├── 02_Core_Gameplay_Loop
│   ├── 03_Factions / 04_Units / 05_Buildings / 06_Resources / 07_Match_Flow
│   ├── 08_Win_Lose_Conditions
│   ├── 09_UI_UX
│   ├── 10_Orbital_Delivery    ← Helldivers-style drop pod model
│   ├── 11_Fog_of_War          ← 3-level visibility system
│   ├── First_Playable_Match
│   ├── Lore_Setting / Backlog / Out_Of_Scope
├── TDD/                       ← Technical Design Document (how it's built)
│   ├── 00_Technical_Overview
│   ├── 01_Module_Architecture
│   ├── 02_GAS_Architecture
│   ├── 03_Multiplayer_Architecture
│   ├── 04_RTS_Selection_And_Commands (sections per task GP-0202..0204)
│   ├── 05_Unit_Architecture (section per task GP-0302)
│   ├── 06_Building_Architecture (sections per GP-0301, GP-0304, Post-Pivot Override)
│   ├── 07_Resource_Architecture (Container System Update)
│   ├── 08_Steam_Matchmaking
│   ├── 09_Gameplay_Tags
│   ├── 10_Data_Assets (+ Asset Manager Loading Flow)
│   ├── 11_RTS_Camera
│   ├── 12_UI_Architecture (Common UI + MVVM, MVP HUD, Feedback Matrix)
│   ├── 13_Architecture_Proposal (consolidated, stop point before C++ code)
│   ├── 14_Orbital_Delivery
│   ├── 15_Fog_of_War
├── Architecture_Decisions/    ← ADRs (immutable decisions)
│   ├── ADR_0001..0007 (foundational)
│   ├── ADR_0008_AI_Opponent_AAIController
│   ├── ADR_0009_Orbital_Delivery_Pillar
├── Development/               ← Work plan, backlog, slice template
│   ├── Claude_Work_Plan
│   ├── Claude_Task_Backlog
│   ├── Slice_Template
│   ├── Claude_Tasks/          ← Per-task specs (GP-0001..GP-0806, GP-S01..GP-S58)
│   └── Coding_Rules, Git_Workflow, Naming_Conventions, External_Skills
└── Archive/                   ← Legacy / superseded notes
```

## Key Root Documents

- [Docs/Game_Pitch.md](Docs/Game_Pitch.md) — **non-technical onboarding** (Ukrainian).
- [Docs/Development/DOCUMENTATION_INDEX.md](Docs/Development/DOCUMENTATION_INDEX.md) — documentation SoT index + current NEXT task.
- [Docs/README.md](Docs/README.md) — documentation landing / reading order.
- [CONTRIBUTING.md](CONTRIBUTING.md) — engineering rules and review boundaries.
- [STYLE.md](STYLE.md) — naming, code style, content layout, GP conventions.
- [Docs/Architecture_Decisions/](Docs/Architecture_Decisions/README.md) — immutable architectural decisions.
- [Docs/GDD/01_Game_Pillars.md](Docs/GDD/01_Game_Pillars.md) — canonical gameplay identity rules.
- [Docs/TDD/13_Architecture_Proposal.md](Docs/TDD/13_Architecture_Proposal.md) — consolidated implementation plan.

## For New Team Members

| Якщо ти… | Читай у такому порядку |
| --- | --- |
| Не знаєш, що це за гра, не технічний | `Docs/Game_Pitch.md` → `Docs/GDD/00_Project_Overview.md` → `Docs/GDD/First_Playable_Match.md` |
| Геймдизайнер | Game_Pitch → `01_Game_Pillars` → решта `Docs/GDD/` |
| Художник / звуковик | Game_Pitch → `Lore_Setting` → `09_UI_UX` → `12_UI_Architecture` (Feedback Matrix) |
| Програміст | Game_Pitch → `CONTRIBUTING.md` → `STYLE.md` → `Docs/TDD/00_Technical_Overview.md` → `13_Architecture_Proposal` |
| Продюсер / PM | Game_Pitch → `Docs/Development/Claude_Task_Backlog.md` |
