# GP-0302 Worker Unit

## Goal

Описати worker як перший controllable economy unit.

## Inputs

- [`../../GDD/04_Units.md`](../../GDD/04_Units.md)
- [`../../GDD/06_Resources.md`](../../GDD/06_Resources.md)
- [`../../TDD/05_Unit_Architecture.md`](../../TDD/05_Unit_Architecture.md)
- [`../../TDD/10_Data_Assets.md`](../../TDD/10_Data_Assets.md)

## Code Allowed

No.

## Scope

Worker role, commands, resource interaction, Data Asset fields, tags.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Deliverables

- Worker player role.
- Allowed MVP commands.
- `DA_GP_Unit_Worker` fields.
- Tags: worker identity/capabilities.
- Playtest scenarios.

## Validation

- Worker supports move and gather.
- Worker role is readable in UI.
- Worker does not expand MVP into full economy tree.

## Stop Condition

Зупинитися після worker spec.

## Output

- Design spec: section **"Detailed Worker Unit Rules (GP-0302)"** у [`../../TDD/05_Unit_Architecture.md`](../../TDD/05_Unit_Architecture.md).
- Decisions (defaults — no AskUser overrides):
  - Cargo full → auto-return + resume cycle (`bAutoCycle=true`). Worker delivers raw (Planetary) Ferronite до MainBase containers.
  - Deposit depleted → auto-find nearest у `AutoRetargetSearchRadius=3000 cm`.
  - **No build / no produce** (orbital delivery model): Worker не будує і не виробляє. Усі buildings / walls / units прибувають з орбіти через drop pods (Order Menu / `UGP_OrbitalDeliverySubsystem`). No build mode, no ghost preview, no `Server_BuildAt`, no `AGP_GhostBuilding`.
  - Repair STAYS in MVP: hookup confirmed (GP-0301 ability), `GP.Command.Repair` added to AllowedCommands (cost TBD).
  - No-attack rule enforced via `bAutoAttacks=false` і відсутність `Command_Attack/AttackMove` у AllowedCommands.
- DataAsset fields finalized; new `UGP_CargoComponent` design contract.
- Tags added: `GP.Unit.State.Mining`, `GP.Unit.State.Repairing` (no `GP.Unit.State.Building` — Worker не будує).
- Code implementation deferred to follow-up task **GP-0302A Worker Unit Implementation** (Code Allowed: Yes).
