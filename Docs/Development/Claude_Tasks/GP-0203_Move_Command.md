# GP-0203 Move Command

## Goal

Описати move command від local input до server-authoritative resolution.

## Inputs

- [`../../GDD/04_Units.md`](../../GDD/04_Units.md)
- [`../../TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md)
- [`../../TDD/05_Unit_Architecture.md`](../../TDD/05_Unit_Architecture.md)

## Code Allowed

No.

## Scope

Command rules, authority model, validation, feedback, edge cases.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Гравець наказує owned mobile unit рухатися у точку на карті.

## Deliverables

- Command intent flow.
- Server validation rules.
- Invalid terrain/unreachable target behavior.
- Feedback requirements.
- Required tag: `GP.Command.Move`.

## Validation

- Client cannot move enemy units.
- Spam commands have defined behavior.
- Unit death during command is handled.

## Stop Condition

Зупинитися після move command spec.

## Output

- Design spec: section **"Detailed Move Command Rules (GP-0203)"** у [`../../TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md).
- Decisions: formation deferred (stack-on-point), `ProjectPointToNavigation` snap fallback з `MaxNavSnapExtent=1500cm`, last-wins + server-side 50 ms debounce per PC, `EGP_CommandRejectReason` enum для rejection feedback.
- Required tag confirmed: `GP.Command.Move` (native, GPGASRuntime).
- Reserved blockers: `GP.Unit.State.Dead`, `GP.Unit.State.Stunned`, `GP.Ability.State.Channeling`.
- Code implementation deferred to follow-up task **GP-0203A Move Command Implementation** (Code Allowed: Yes).
