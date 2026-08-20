# Claude Task Backlog

> **CURRENT ROADMAP AUTHORITY (2026-08-20):**
> [`MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`](MVP_Roadmap_Reconciliation_Post_Building_Vitals.md).
> The phase/S-number queue below is retained as task inventory and history; it is not the current
> execution order. Classify the capability against current code before opening any historical task.
> Immediate NEXT is the three-state per-team Fog of War runtime foundation. SWARM is a separate MVP
> system from the RTS AI Opponent and is the final gameplay implementation stage, after a mandatory
> design/reconciliation review.

## Scope

Це індекс задач для Claude Code. Задачі виконуються **строго по одній**: Claude відкриває один task-файл, виконує тільки його scope, фіксує результат, зупиняється або переходить до наступної задачі тільки після явного дозволу.

Кожна gameplay-facing задача використовує:

- `SKILLS/game-design-framework`
- `SKILLS/gp-mechanics-validator`

## Execution Rule

1. Не брати дві задачі одночасно.
2. Не змішувати documentation, architecture і code implementation в одному кроці.
3. Не створювати C++ gameplay code, якщо task не має явного `Code Allowed: Yes`.
4. Кожна задача завершується `Stop Condition`.
5. Якщо task відкриває нові питання, Claude пише їх у `Open Questions` і зупиняється.

## Task Queue

### Phase 0: Orientation

1. [GP-0001 Read Canonical Rules](Claude_Tasks/GP-0001_Read_Canonical_Rules.md)
2. [GP-0002 Validate Documentation Map](Claude_Tasks/GP-0002_Validate_Documentation_Map.md)

### Phase 1: First Playable Design

3. [GP-0101 Define First Playable Match](Claude_Tasks/GP-0101_First_Playable_Match.md)
4. [GP-0102 Define Core Gameplay Loop](Claude_Tasks/GP-0102_Core_Gameplay_Loop.md)

### Phase 2: Player Control Systems

5. [GP-0201 RTS Camera](Claude_Tasks/GP-0201_RTS_Camera.md)
6. [GP-0202 Selection](Claude_Tasks/GP-0202_Selection.md)
7. [GP-0203 Move Command](Claude_Tasks/GP-0203_Move_Command.md)
8. [GP-0204 Attack Command](Claude_Tasks/GP-0204_Attack_Command.md)

### Phase 3: MVP Economy and Entities

9. [GP-0301 Main Base](Claude_Tasks/GP-0301_Main_Base.md)
10. [GP-0302 Worker Unit](Claude_Tasks/GP-0302_Worker_Unit.md)
11. [GP-0303 Resource Primary](Claude_Tasks/GP-0303_Resource_Primary.md)
12. [GP-0304 Logistics Hub (renamed from Assembly Yard / Barracks)](Claude_Tasks/GP-0304_Barracks.md) — task file ще під старим іменем; rename pending GP-0802.
13. [GP-0305 Wall + Wall-mounted Turret + Build Grid System](Claude_Tasks/GP-0305_Wall.md) — acquisition superseded by [GP-0305R](Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md)
14. [GP-0306 AI Opponent — design (state machine, AAIController per ADR-0008)](Claude_Tasks/GP-0306_AI_Opponent.md)
15. [GP-0307 Sell + Demolish (buildings sold for partial refund; walls demolished permanently)](Claude_Tasks/GP-0307_Sell_Demolish.md)

### Phase 4: UI and Feedback

13. [GP-0401 MVP HUD](Claude_Tasks/GP-0401_MVP_HUD.md)
14. [GP-0402 Feedback Pass](Claude_Tasks/GP-0402_Feedback_Pass.md)

### Phase 5: Multiplayer MVP

15. [GP-0501 Steam Matchmaking MVP](Claude_Tasks/GP-0501_Steam_Matchmaking_MVP.md)

### Phase 6: Approval and Implementation Planning

16. [GP-0601 Architecture Proposal](Claude_Tasks/GP-0601_Architecture_Proposal.md) — output: [`../TDD/13_Architecture_Proposal.md`](../TDD/13_Architecture_Proposal.md).
17. [GP-0602 Implementation Slices](Claude_Tasks/GP-0602_Implementation_Slices.md) — output: [`Slice_Template.md`](Slice_Template.md) + per-slice task files `GP-S##` enumerated below.

### Phase 6A: Implementation Slices (Foundation)

> All `GP-S##` slices require explicit approval of previous slice. Per-slice deliverables та CI gates документовані у [`Slice_Template.md`](Slice_Template.md).

**Slice 1 — Foundation** (materialized)

- [GP-S01 Module Scaffolds](Claude_Tasks/GP-S01_Module_Scaffolds.md)
- [GP-S02 Native Gameplay Tags](Claude_Tasks/GP-S02_Native_Gameplay_Tags.md)
- [GP-S03 Attribute Sets](Claude_Tasks/GP-S03_Attribute_Sets.md)
- [GP-S04 AbilitySystemComponent Subclass](Claude_Tasks/GP-S04_AbilitySystemComponent_Subclass.md)
- [GP-S05 Damage Calculation MMC](Claude_Tasks/GP-S05_Damage_Calculation_MMC.md)

**Slice 10 — AI Opponent** (materialized — fills critical MVP gap)

- [GP-S54 AI PlayerController](Claude_Tasks/GP-S54_AI_PlayerController.md)
- [GP-S55 AI Behavior DataAsset](Claude_Tasks/GP-S55_AI_Behavior_Definition.md)
- [GP-S56 AI State Implementations](Claude_Tasks/GP-S56_AI_State_Implementations.md)

**Slices 2-9 і 11-13** — to be materialized incrementally як slices approve sequentially. Enumeration of all sub-tasks (S06..S67) lives у [`../TDD/13_Architecture_Proposal.md`](../TDD/13_Architecture_Proposal.md) §Implementation Order:

- Slice 2 — Match Flow + Asset Loader (S06-S11)
- Slice 3 — Camera (S12-S15)
- Slice 4 — Selection + Smart Commands (S16-S19)
- Slice 5 — Movement (S20-S22)
- Slice 6 — Worker + Resources (S23-S28)
- Slice 7 — Combat (S29-S33)
- Slice 8 — Buildings + Orbital Drops + Wall + Grid (S34-S46)
- Slice 9 — UI Foundation + FoW (S47-S53)
- Slice 11 — Feedback Pass (S57-S60)
- Slice 12 — Steam MVP (S61-S64)
- Slice 13 — Match End + Polish (S65-S67)

### Phase 7: Post-MVP Backlog (Pending Owner Sign-Off)

18. GP-0701 Expand TDD/12 UI Architecture — file created by GP-0401 з MVP HUD section; GP-0701 додає theme system, localization, settings panel, accessibility, scaling.
19. GP-0702 Expand AI Architecture — `AGP_AIController` (per [ADR-0008](../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md)) state machine deep dive: state transitions, decision-tick policies, `UGP_AIBehaviorDefinition` schema, drop-targeting heuristics, defense reaction thresholds. AI is **MVP** (per `project_ai_opponent_in_mvp`); це expansion task для AI nuance after Slice 11A implementation. Live або у TDD/16_AI_Architecture.md OR section у TDD/13.
20. GP-0703 Historical docs task — delivery-quota/FerroniteThreatValue reconciliation is already reflected in current canon. **SWARM itself is MVP, not post-MVP:** it requires a dedicated design gate and is the final gameplay implementation stage per the current roadmap. Post-MVP applies only to explicitly deferred SWARM variants/depth.
21. GP-0801 Corporate Doctrine Tree Design (post-MVP asymmetry) — pending file creation. Scope: design 5 doctrine axes (Mining / Logistics / Defensive / Extraction Efficiency / Automated Machinery), per Pillar 5. **Updated:** doctrines apply additive modifiers до orbital drop catalog / container throughput / SWARM resistance / mining rates. Use `gp-mechanics-validator` mandatory.
22. GP-0802 Logistics Hub Content Rewrite — `GP-0304_Barracks.md` отримав SUPERSEDED header (filename retained для cursor stability — **не** renaming). Залишилось: написати повноцінний Logistics Hub task spec per orbital model (no local build, no production component, no construction speedup; passive +MaxUnits + container-cap contribution, arrives via orbital drop). References [`05_Buildings`](../GDD/05_Buildings.md), [`06_Building_Architecture`](../TDD/06_Building_Architecture.md).
23. GP-0803 Game Pillars Adoption Pass — verify, що всі existing tasks references Pillar-driven validation per нової `01_Game_Pillars.md` версії. **Updated scope:** verify Pillar 8 (Simple Core, Combinatorial Depth) 5-question MVP gate applied у task acceptance criteria. Verify Technical Pillar correctly renumbered 8 → 9 across all docs. Verify orbital pivot consistency.
24. GP-0804 Win Condition Rewrite — **DONE** у цьому docs pass: `GDD/08_Win_Lose_Conditions` переписано з "score race" до delivery-quota (`DeliveryQuotaFerroniteScore`) per [ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md). Canonical tie-break ladder: `FerroniteScore` → `OrbitalFerronite` → `CurrentUnits` → deterministic seed (score metric = FerroniteScore, **не** OrbitalFerronite). Annihilation path: MainBase destroyed = auto-loss if `bAnnihilationCountsAsWin`.
25. GP-0805 (new) FoW Adoption Pass — verify selection / inspect / combat / drop targeting docs всі reference `15_Fog_of_War`. Update minimap rendering section у `TDD/12` із 3-layer FoW colors. Update CONTRIBUTING.md hard bans з FoW-related anti-patterns (no client visibility decisions, no all-actors-replicated-everywhere).
26. GP-0806 (new) Game Pitch Onboarding Doc — `Docs/Game_Pitch.md`, non-technical Ukrainian onboarding для new team members.

## Global Stop Rule

Після `GP-0601` Claude має зупинитися і чекати підтвердження перед C++ gameplay code.

## Pillar Compliance Rule

Будь-яка gameplay task у будь-якій фазі **повинна** пройти validation через [`gp-mechanics-validator`](../../SKILLS/gp-mechanics-validator/SKILL.md) проти canonical pillars з [`../GDD/01_Game_Pillars.md`](../GDD/01_Game_Pillars.md). Pillar Violation verdict — review-blocking.
