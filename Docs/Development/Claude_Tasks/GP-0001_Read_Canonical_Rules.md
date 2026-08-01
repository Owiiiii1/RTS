# GP-0001 Read Canonical Rules

## Goal

Claude має зрозуміти канонічні правила GrimProtocol перед будь-якою зміною.

## Inputs

- [`../../../README.md`](../../../README.md)
- [`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md)
- [`../../../STYLE.md`](../../../STYLE.md)
- [`../../README.md`](../../README.md)
- [`../../GDD/00_Project_Overview.md`](../../GDD/00_Project_Overview.md)
- [`../../TDD/00_Technical_Overview.md`](../../TDD/00_Technical_Overview.md)

## Code Allowed

No.

## Scope

Тільки читання і коротке резюме. Не редагувати файли.

## Required Skill Pass

Не потрібен.

## Deliverables

- Summary: project shape, hard bans, first playable target.
- Branch/status note.
- List of documents read.

## Validation

Claude явно підтверджує:

- no Lyra;
- no extra runtime modules;
- no C++ gameplay code without approval;
- GAS-first, multiplayer-first, data-driven-first.

## Stop Condition

Зупинитися після summary.
