# Claude Work Plan

> **Current execution authority (2026-08-20):**
> [`MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`](MVP_Roadmap_Reconciliation_Post_Building_Vitals.md).
> This file's original stage sequence is process history, not proof that a capability remains missing.
> Immediate NEXT is the three-state per-team Fog of War runtime foundation. Architecture/config cleanup
> ended with Building Vitals / Definition Ownership; footprint cleanup is deferred pending a possible
> building construction redesign.

## Scope

Це покрокове завдання для Claude Code. Claude має рухатися поетапно, не генерувати gameplay C++ code до погодження і тримати зміни маленькими.

Детальний task backlog для Claude лежить у [`Claude_Task_Backlog.md`](Claude_Task_Backlog.md). Він веде до окремих task-файлів у [`Claude_Tasks/`](Claude_Tasks/README.md). Claude має виконувати одну задачу за раз.

## Stage 1: Analysis

- Прочитати [`../../README.md`](../../README.md).
- Прочитати [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md).
- Прочитати [`../../STYLE.md`](../../STYLE.md).
- Прочитати [`../README.md`](../README.md).
- Прочитати [`../TDD/00_Technical_Overview.md`](../TDD/00_Technical_Overview.md).
- Прочитати [`../GDD/00_Project_Overview.md`](../GDD/00_Project_Overview.md).
- Не генерувати код.

## Stage 2: Documentation

- Деталізувати GDD.
- Деталізувати TDD.
- Додати markdown links між GDD і TDD.
- Синхронізувати gameplay design і technical design.
- Описати first playable target у GDD/TDD без розширення scope.

## Stage 3: Architecture

- Запропонувати мінімальний список стартових C++ класів.
- Описати responsibility кожного класу.
- Описати module ownership.
- Описати Data Asset ownership.
- Описати Gameplay Tags structure.

## Stage 4: Approval Stop

- Зупинитися після документації.
- Надати список запропонованих C++ класів.
- Не створювати C++ gameplay code без підтвердження.

## Stage 5: After Approval

- Створювати код маленькими кроками.
- Один system slice за раз.
- Перевіряти capability status у current roadmap, а не продовжувати historical S-number механічно.
- Поточний dependency path: FoW runtime → production UI/HUD + minimap → RTS AI Opponent → remaining
  bounded gameplay/building work after its design gate → Steam MVP → match completion flow.
- SWARM і RTS AI Opponent — різні системи.
- SWARM: **MVP — FINAL IMPLEMENTATION STAGE; DESIGN REVIEW REQUIRED BEFORE IMPLEMENTATION**.
- Після SWARM — full MVP end-to-end validation/stabilization.

## First Playable Target

Singleplayer:

- Режим з ПК.
- Одна мапа.
- RTS camera.
- Selection.
- Move command.
- Attack command.

PvP:

- Steam matchmaking.
- 2 players.
- Host/client.
- Та сама мапа.
- Server-authoritative gameplay.

Gameplay (orbital delivery MVP loop):

- Main Base + 2 Workers на старті; OrbitalFerronite = 0.
- Worker mineить raw (Planetary) Ferronite і носить його у MainBase containers.
- Container ships to orbit → Planetary Ferronite конвертується у OrbitalFerronite (spendable) + FerroniteScore (cumulative shipped score).
- FerroniteThreatValue = raw stock at base (up on drop-off, down on launch) → drives continuous SWARM pressure (numbered waves **superseded**; see [`../GDD/14_SWARM.md`](../GDD/14_SWARM.md)).
- Logistics Hub + Order Menu (`UGP_OrbitalDeliverySubsystem`) — spend OrbitalFerronite на orbital drops (Workers / combat units / buildings / walls) via `AGP_DropPod`. No local production / construction.
- Worker repairs (`GP.Command.Repair`, cost TBD). No build, no produce.
- Base receives damage.
- Match ends: перший до `DeliveryQuotaFerroniteScore` (placeholder 5000) wins; інакше highest `FerroniteScore` at timer; MainBase destroyed = loss (if `bAnnihilationCountsAsWin`).

## Guardrails

- Не додавати Lyra architecture.
- Не створювати зайві runtime modules.
- Не створювати C++ gameplay code без погодження.
- Не масово генерувати класи, ассети або Blueprint logic.
- Будь-яка нова механіка проходить feature validation з [`Backlog`](../GDD/Backlog.md) і [`External_Skills`](External_Skills.md).
- Детальні MVP tasks ведуться у [`Claude_Task_Backlog.md`](Claude_Task_Backlog.md) і [`Claude_Tasks/`](Claude_Tasks/README.md).
