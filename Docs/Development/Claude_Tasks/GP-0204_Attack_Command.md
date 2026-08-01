# GP-0204 Attack Command

## Goal

Описати attack command, damage authority і feedback.

## Inputs

- [`../../GDD/04_Units.md`](../../GDD/04_Units.md)
- [`../../GDD/08_Win_Lose_Conditions.md`](../../GDD/08_Win_Lose_Conditions.md)
- [`../../TDD/02_GAS_Architecture.md`](../../TDD/02_GAS_Architecture.md)
- [`../../TDD/05_Unit_Architecture.md`](../../TDD/05_Unit_Architecture.md)

## Code Allowed

No.

## Scope

Attack intent, target validity, damage through GAS, death/destroyed target handling.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Гравець наказує unit атакувати enemy unit/building/base, щоб наблизити win condition.

## Deliverables

- Attack command rules.
- Target validation.
- Damage/health ownership.
- Feedback requirements.
- Required tag: `GP.Command.Attack`.

## Validation

- Client cannot fake damage.
- Target destroyed mid-command has defined outcome.
- Base damage can connect to match end.

## Stop Condition

Зупинитися після attack command spec.

## Output

- Design spec: section **"Detailed Attack Command Rules (GP-0204)"** у [`../../TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md).
- Decisions:
  - Attack-move included у MVP як `GP.Command.AttackMove` (новий native tag).
  - Friendly fire: allowed only через explicit `Attack` command; auto-acquire hard-blocks same team.
  - LOS: 3-point line traces (Eye→Head, Chest→Chest, Feet→Feet) на `ECC_Visibility`; ANY clear trace = LOS valid (forgiving cover).
  - Auto-acquire: basic closest enemy у `AcquireRange = AttackRange × 1.25`; tick 0.5 s; workers `bAutoAttacks=false`.
  - Damage authority server-only через `ASC->ApplyGameplayEffectSpecToTarget` + `GE_GP_Cooldown_Attack`. VFX через `Multicast Unreliable`.
- Tags added/required: `GP.Command.Attack`, `GP.Command.AttackMove`, `GP.Unit.State.Attacking`, `GP.Unit.State.AttackCooldown` (existing).
- UnitDefinition new fields: `bAutoAttacks : bool`, `AutoAcquireRangeMultiplier : float`.
- MainBase destruction → match-end hook через `AGP_GameMode::OnUnitDied` + `GP.Building.Type.MainBase` tag (deep rule documented у GP-0301).
- Code implementation deferred to follow-up task **GP-0204A Attack Command Implementation** (Code Allowed: Yes).

## Pivot Note (2026-05-16 — Fog of War у MVP)

**Combat scope обмежено FoW** per [`../../TDD/15_Fog_of_War.md`](../../TDD/15_Fog_of_War.md):

- Auto-acquire filter: `UGP_TargetingComponent::FindClosestEnemy` додає `IsVisibleToTeam(AttackerTeamId, candidate.Location)` check. Hidden enemies не attacked automatically.
- LOS multi-trace (existing 3-trace) — physical line check, ortogonal до FoW. Both required (FoW visible AND LOS clear).
- Explicit Attack on target that becomes hidden: command persists; attacker chases last-known location; re-engage on re-sight.
- Server `IsNetRelevantFor` override: hidden enemy actors not replicated to opponent client → cheat-resistant.
