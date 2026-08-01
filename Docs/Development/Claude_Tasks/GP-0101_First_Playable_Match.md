# GP-0101 Define First Playable Match

## Goal

Описати first playable match як один end-to-end player story без прогалин у правилах.

## Inputs

- [`../../GDD/00_Project_Overview.md`](../../GDD/00_Project_Overview.md)
- [`../../GDD/07_Match_Flow.md`](../../GDD/07_Match_Flow.md)
- [`../../GDD/08_Win_Lose_Conditions.md`](../../GDD/08_Win_Lose_Conditions.md)
- [`../../TDD/03_Multiplayer_Architecture.md`](../../TDD/03_Multiplayer_Architecture.md)

## Code Allowed

No.

## Scope

Документація only. Не пропонувати класи, окрім якщо потрібно як open question для пізнішої architecture task.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Гравець запускає матч, бачить main base і worker, керує ними, будує escalation path і доводить матч до завершення.

## System Rules

- Singleplayer: одна локальна сесія на ПК.
- PvP: Steam matchmaking, 2 players, host/client.
- Одна карта.
- Server-authoritative gameplay.
- Match ends when win/lose condition is satisfied.

## Deliverables

- Уточнити first playable story у GDD.
- Уточнити match phases.
- Уточнити win/lose trigger.
- Додати multiplayer authority notes у TDD, якщо бракує.

## Validation

- New player can understand the match objective.
- Host/client result is one shared truth.
- Match cannot get stuck without end condition.

## Stop Condition

Зупинитися після documentation update і Mechanics Review summary. Не створювати code.
