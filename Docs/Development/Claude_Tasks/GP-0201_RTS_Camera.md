# GP-0201 RTS Camera

## Goal

Описати RTS camera behavior для PC first playable.

## Inputs

- [`../../GDD/09_UI_UX.md`](../../GDD/09_UI_UX.md)
- [`../../TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md)

## Code Allowed

No.

## Scope

Camera design only: pan, zoom, bounds, local ownership, UI focus interaction.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Гравець швидко оглядає карту, не втрачає selected units і може комфортно видавати команди.

## Deliverables

- Camera controls.
- Local-only authority rule.
- Edge cases: map bounds, UI focus, alt-tab, match end.
- Playtest scenarios.

## Validation

- Camera does not mutate gameplay state.
- Camera supports selection/commands without blocking them.
- No multiplayer authority concerns beyond local input.

## Stop Condition

Зупинитися після camera spec.

## Output

- Design spec: [`../../TDD/11_RTS_Camera.md`](../../TDD/11_RTS_Camera.md).
- Decisions: Pawn-owned camera (Simple First), `UGP_CameraConfigDataAsset` tuning (Data First), Enhanced Input set `IMC_GP_Camera` with disjoint bindings from selection/commands.
- Code implementation deferred to follow-up task **GP-0201A RTS Camera Implementation** (Code Allowed: Yes) — to be added to backlog after spec approval.
