# GP-S05 Damage Calculation MMC

## Slice Group
Slice 1 — Foundation

## Code Allowed
Yes — after GP-S04 approval.

## Depends On
- GP-S04.

## Goal
Implement `UGP_DamageCalculation : UGameplayModMagnitudeCalculation` — reads source `Damage`, target `Armor` and `DamageResistance`; outputs negative Health delta. Used by `GE_GP_Damage_Basic` у later Combat slice.

## Scope
- Create `UGP_DamageCalculation : UGameplayModMagnitudeCalculation`.
- `DECLARE_ATTRIBUTE_CAPTUREDEF` / `DEFINE_ATTRIBUTE_CAPTUREDEF` для:
  - Source `Damage`, snapshot=`false` (live);
  - Target `Armor`, snapshot=`false` (live);
  - Target `DamageResistance`, snapshot=`false` (live).
- Override `CalculateBaseMagnitude_Implementation` (UE 5.8 BlueprintNativeEvent):
  - `RawDamage = max(0, CapturedDamage)`
  - `EffectiveArmor = max(0, CapturedArmor)`
  - `Resistance = clamp(CapturedDamageResistance, 0, 1)`
  - `DamageAfterArmor = max(0, RawDamage - EffectiveArmor)`
  - `EffectiveDamage = DamageAfterArmor * (1 - Resistance)`
  - Return `-EffectiveDamage` (Health modifier is negative).
- Place у `GPGASRuntime/Public|Private/Calculations/`.

## Out of Scope
- `GE_GP_Damage_Basic` `.uasset` (Combat slice).
- Temporary GE / debug actor / Blueprint / map for runtime Health delta.
- Critical hit / armor penetration / damage types.
- GE Magnitude picker validation until a real GE exists.

## Required Skill Pass
- `ue5-gas`

## Files Touched
- `GP/Source/GPGASRuntime/Public/Calculations/GPDamageCalculation.h` — new
- `GP/Source/GPGASRuntime/Private/Calculations/GPDamageCalculation.cpp` — new

## Implementation status (2026-08-01)
Status: **DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S06 until explicitly assigned.

### Capture definitions — correct
| Attribute | Side | Snapshot |
| --- | --- | --- |
| `UGP_UnitAttributeSet::Damage` | Source | `false` (live) |
| `UGP_UnitAttributeSet::Armor` | Target | `false` (live) |
| `UGP_UnitAttributeSet::DamageResistance` | Target | `false` (live) |

Clarification: **Source/Target** = capture side; **snapshot=false** = live/non-snapshot at apply/eval (not “Snapshot vs Source”).

### Exact formula — correct
```
RawDamage = max(0, CapturedDamage)
EffectiveArmor = max(0, CapturedArmor)
Resistance = clamp(CapturedDamageResistance, 0, 1)
DamageAfterArmor = max(0, RawDamage - EffectiveArmor)
EffectiveDamage = DamageAfterArmor * (1 - Resistance)
return -EffectiveDamage
```
Control: Damage=100, Armor=20, Resistance=0.25 → AfterArmor=80, Effective=60, return **-60**.

### Defensive clamps — present
- Damage / Armor floored to ≥ 0
- DamageResistance clamped to `[0, 1]`
- Missing capture → Warning + **0** (no hardcoded balance fallbacks)

### No hardcoded balance values
Confirmed — all inputs from captured attributes.

### Deferred
- GE Magnitude picker validation — **Combat slice** (when GE exists).
- Actual Health change — **deferred** to `GE_GP_Damage_Basic` integration (Combat slice).

### UE 5.8 API note
MMC uses `GetCapturedAttributeMagnitude` (wraps capture AttemptCalculate). `AttemptCalculateCapturedAttributeMagnitude` is on ExecutionCalculation parameters — not available on MMC.

## Acceptance Criteria
- [x] Compiles clean (GPEditor / GP Dev / GP Shipping).
- [x] Capture definitions correct (three live captures as above).
- [x] Formula correct (control −60 documented).
- [x] Defensive clamps present.
- [x] No hardcoded armor / damage balance values inside MMC.
- [x] Operator Editor/module load + PIE PASSED; GP-S05 related errors ABSENT.
- [x] GE Magnitude picker validation — **deferred** to Combat slice (accepted for GP-S05 close).
- [x] Actual Health change — **deferred** to `GE_GP_Damage_Basic` integration (accepted for GP-S05 close).
- [x] Tech lead accepted GP-S05.
- [x] Operator accepted GP-S05.

## Playtest / Validation Note
**GP-S05 (closed):** Open project; GPGASRuntime loads; PIE clean; no GP-S05 related errors. No temporary GE/Blueprint created for this slice.

**Combat slice (deferred):** Apply `GE_GP_Damage_Basic` with Source.Damage=100, Target.Armor=20, Target.DamageResistance=0.25 → Health delta = -60. Confirm class in Magnitude picker.

## Risks / Edge Cases
- Negative `Armor` → defensive clamp to ≥ 0.
- `DamageResistance` > 1 — clamp to 1 to avoid healing instead of damage.
- Without a Spec that captured RelevantAttributes, GetCapturedAttributeMagnitude fails → 0 + Warning (expected until Combat wires GE).
- DirectoryWatcher empty-Content warning was a local project-folder issue (fixed by restoring empty `GP/Content`); unrelated to MMC.

## Linked
- [`../../TDD/02_GAS_Architecture`](../../TDD/02_GAS_Architecture.md) §Damage Effects.
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §GPGASRuntime.

## Stop Condition
STOP. GP-S05 closed as DONE. Slice 1 Foundation complete. Await explicit assignment before GP-S06 (`AGP_GameState`). Do not start GP-S06 here.
