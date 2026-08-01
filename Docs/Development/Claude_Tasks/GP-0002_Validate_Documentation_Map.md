# GP-0002 Validate Documentation Map

## Goal

Перевірити, що документація має зрозумілу навігацію між GDD, TDD, Development і ADR.

## Inputs

- [`../../README.md`](../../README.md)
- [`../../GDD/README.md`](../../GDD/README.md)
- [`../../TDD/README.md`](../../TDD/README.md)
- [`../../Development/README.md`](../../Development/README.md)
- [`../../Architecture_Decisions/README.md`](../../Architecture_Decisions/README.md)

## Code Allowed

No.

## Scope

Дозволено редагувати тільки markdown navigation links і короткі missing-page notes.

## Required Skill Pass

`documentation-knowledge-manager`.

## Deliverables

- Оновлені links, якщо вони биті або неповні.
- Список gaps, які не треба виправляти в цій задачі.

## Validation

- `Docs/README.md` веде до всіх основних секцій.
- GDD і TDD index pages мають cross-reference.
- Development page веде до Claude task queue.

## Stop Condition

Зупинитися після documentation map check. Не деталізувати gameplay.
