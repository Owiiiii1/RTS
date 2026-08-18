# Development

Operational rules для щоденної роботи у репозиторії.

## Pages

- [DOCUMENTATION_INDEX](DOCUMENTATION_INDEX.md) — **единый индекс документации**: SoT, deprecated, conflicts, NEXT task.
- [AI_Project_Log](AI_Project_Log.md) — stage reports / current status trail.
- [Coding_Rules](Coding_Rules.md) — C++ rules, що не покриваються STYLE.md (review checklist).
- [Naming_Conventions](Naming_Conventions.md) — повна таблиця префіксів і конвенцій.
- [External_Skills](External_Skills.md) — підключення `SKILLS/UnrealEngine5-Skills` і `game-design-framework`.
- [Git_Workflow](Git_Workflow.md) — branching model, commit conventions, PR flow.
- [Slice_Template](Slice_Template.md) — canonical structure для code-implementation `GP-S##` slice tasks (header, branch / commit conventions, CI gates, granularity guardrails, anti-patterns, Pillar 8 5-question gate).
- [Claude_Work_Plan](Claude_Work_Plan.md) — поетапне завдання для Claude Code.
- [Claude_Task_Backlog](Claude_Task_Backlog.md) — індекс модульних Claude задач (Phase 0 → Phase 7 + Phase 6A code slices).
- [Claude_Tasks/](Claude_Tasks/README.md) — окремі task-файли, один файл на одну задачу.
- [GRIM_PROTOCOL_START_RULES](GRIM_PROTOCOL_START_RULES.md) — AI/operator process protocol (keep in sync with root copy).
- [Risk_Based_Development_Workflow](Risk_Based_Development_Workflow.md) — canonical test/build selection for slices (adaptive validation, not weaker validation).

## Task Categories

### Design tasks (`GP-NNNN_*.md`)

Pre-code spec work. Each task — single area of design (camera, selection, resource, etc.). Output = section у TDD/GDD або new doc file.

| Phase | IDs | Status |
| --- | --- | --- |
| 0 — Orientation | GP-0001, GP-0002 | done |
| 1 — First Playable Design | GP-0101, GP-0102 | done |
| 2 — Player Control Systems | GP-0201..0204 | done |
| 3 — MVP Economy & Entities | GP-0301..0305 | done (GP-0305 added for Wall) |
| 4 — UI & Feedback | GP-0401, GP-0402 | done |
| 5 — Multiplayer MVP | GP-0501 | done |
| 6 — Approval & Implementation Planning | GP-0601, GP-0602 | done |
| 7 — Post-MVP / Pivot Cascade | GP-0701..0806 | scope defined, file creation pending |

### Code-implementation slices (`GP-S##_*.md`)

Per-slice spec для actual C++ writing. Follows [`Slice_Template.md`](Slice_Template.md). Materialized incrementally (foundation S01-S05 ready, rest list-only у backlog).

## Pointers

- Engineering rules — [`/CONTRIBUTING.md`](../../CONTRIBUTING.md).
- Style guide — [`/STYLE.md`](../../STYLE.md).
- Module split — [`../TDD/01_Module_Architecture.md`](../TDD/01_Module_Architecture.md).
- Consolidated architecture — [`../TDD/13_Architecture_Proposal.md`](../TDD/13_Architecture_Proposal.md).
- Feature validation — [`../GDD/Backlog.md`](../GDD/Backlog.md).
- Game Pitch (non-tech onboarding) — [`../Game_Pitch.md`](../Game_Pitch.md).
