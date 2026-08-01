# GP-S05 Damage Calculation MMC

## Slice Group
Slice 1 — Foundation

## Code Allowed
Yes — after GP-S04 approval.

## Depends On
- GP-S04.

## Goal
Implement `UGP_DamageCalculation : UGameplayModMagnitudeCalculation` stub — reads source `Damage`, target `Armor` and `DamageResistance`; outputs negative Health delta. Used by `GE_GP_Damage_Basic` у later combat slice.

## Scope
- Create `UGP_DamageCalculation : UGameplayModMagnitudeCalculation`.
- `DECLARE_ATTRIBUTE_CAPTUREDEF` для Source `Damage`, Target `Armor`, Target `DamageResistance`.
- Override `CalculateBaseMagnitude(const FGameplayEffectSpec& Spec) const`:
  - `RawDamage = Source.Damage`.
  - `EffectiveDamage = max(0, RawDamage - Target.Armor) * (1.0 - clamp(Target.DamageResistance, 0, 1))`.
  - Return `-EffectiveDamage` (Health modifier is negative).
- Place у `GPGASRuntime/Public/Calculations/`.

## Out of Scope
- `GE_GP_Damage_Basic` `.uasset` (created у Combat slice).
- Critical hit / armor penetration logic.
- Per-damage-type variations.

## Required Skill Pass
- `ue5-gas`

## Files Touched
- `GP/Source/GPGASRuntime/Public/Calculations/GPDamageCalculation.h` — new
- `GP/Source/GPGASRuntime/Private/Calculations/GPDamageCalculation.cpp` — new

## Acceptance Criteria
- [ ] Compiles clean.
- [ ] `UGP_DamageCalculation` selectable у GE `Magnitude` modifier picker (after Combat slice creates the GE).
- [ ] Unit test (or debug PIE с stub GE): apply GE that uses this MMC — Health changes per formula.
- [ ] No hardcoded armor / damage values inside MMC (all read from attributes).
- [ ] No clamp leak — `DamageResistance` clamped to `[0, 1]` inside calculation.

## Playtest / Validation Note
Debug GE temporarily applied to a test actor. Set Source.Damage=100, Target.Armor=20, Target.DamageResistance=0.25 → expect Health delta = -(100 - 20) * (1 - 0.25) = -60. Logged via `UE_LOG(LogGAS, Verbose, ...)`.

## Risks / Edge Cases
- Negative `Armor` → defensive clamp to ≥ 0.
- `DamageResistance` > 1 — clamp to 1 to avoid healing instead of damage.
- Source attribute capture mode (`Snapshot` vs `Source`) — use `Source` to read live attacker stats at apply time.

## Linked
- [`../../TDD/02_GAS_Architecture`](../../TDD/02_GAS_Architecture.md) §Damage Effects.
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §GPGASRuntime.

## Stop Condition
STOP. Slice 1 complete. Await approval before Slice 2 (`GP-S06 AGP_GameState`).
