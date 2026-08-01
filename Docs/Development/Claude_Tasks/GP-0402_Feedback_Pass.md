# GP-0402 Feedback Pass

## Goal

Описати feedback для MVP actions before code.

## Inputs

- [`../../GDD/09_UI_UX.md`](../../GDD/09_UI_UX.md)
- Task specs GP-0202 to GP-0304.

## Code Allowed

No.

## Scope

Feedback requirements for selection, move, attack, damage, resource gain/spend, build complete, match end.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Deliverables

- Feedback matrix.
- Minimum two channels where practical.
- Cosmetic vs gameplay truth separation.
- Multiplayer note for cosmetic events.

## Validation

- Significant action has readable feedback.
- Feedback does not become gameplay authority.
- UI/VFX/audio requirements are scoped for MVP.

## Stop Condition

Зупинитися після feedback matrix.

## Output

- Design spec: section **"Detailed Feedback Matrix (GP-0402)"** у [`../../TDD/12_UI_Architecture.md`](../../TDD/12_UI_Architecture.md).
- 30+ action events mapped через 10 channels (V-Decal, V-VFX, V-Mat, V-Mesh, V-UI, A-3D, A-2D, HUD-Toast, HUD-Flash, HUD-Indicator).
- Cosmetic vs Truth separation audit з anti-pattern checklist.
- Multiplayer rules: listen-server parity, multicast unreliable cap ≤ 30/s, sound priority hierarchy (UI > Player-action > World > Opponent).
- DataAsset surface: `DA_GP_FeedbackBundle_*` (Worker, Building per type, Resource per type, Command, Notification) — asset refs у data per Data-Driven First.
- 16 playtest scenarios + 6 open questions + Out of MVP list.
- Code implementation tied to GP-0401A (HUD) + per-system implementation tasks (GP-0203A/0204A/0301A/0302A/0303A/0304A).
