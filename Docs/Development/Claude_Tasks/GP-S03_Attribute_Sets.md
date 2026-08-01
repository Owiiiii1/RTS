# GP-S03 Attribute Sets

## Slice Group
Slice 1 — Foundation

## Code Allowed
Yes — after GP-S02 approval.

## Depends On
- GP-S02.

## Goal
Implement `UGP_PlayerAttributeSet` and `UGP_UnitAttributeSet` із all attributes per spec, replication conditions, and GAS boilerplate. No abilities/effects yet — pure attribute infrastructure.

## Scope
- Create `UGP_PlayerAttributeSet : UAttributeSet`:
  - Attributes: `OrbitalFerronite`, `FerroniteScore`, `MaxUnits`, `CurrentUnits` (post-pivot canon — no single `Ferronite`/`MaxFerronite` pool; Planetary Ferronite lives as `UGP_StorageComponent` container state, and swarm pressure `FerroniteThreatValue` is GameState-side).
  - `GAMEPLAYATTRIBUTE_VALUE_INITTER` boilerplate.
  - `OnRep_*` for each replicated attribute.
  - `GetLifetimeReplicatedProps`:
    - `OrbitalFerronite, MaxUnits, CurrentUnits` — `COND_OwnerOnly`.
    - `FerroniteScore` — `COND_None` (cumulative shipped victory score, visible to all).
- Create `UGP_UnitAttributeSet : UAttributeSet`:
  - Attributes: `Health`, `MaxHealth`, `Armor`, `DamageResistance`, `AttackCooldown`, `Damage`, `AttackRange`, `AttackSpeed`, `MoveSpeed`, `CarriedFerronite`.
  - Same boilerplate.
  - Replication conditions per [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §Replication Conditions.
  - Clamp `Health` to `[0, MaxHealth]` у `PreAttributeChange`.
  - Clamp `CarriedFerronite` to `[0, MaxCargo]` (server uses external param; for now clamp ≥ 0).
- Файл placement: `GPGASRuntime/Public/AttributeSets/`, `GPGASRuntime/Private/AttributeSets/`.

## Out of Scope
- ASC subclass (next slice).
- GameplayEffects.
- Damage calculation MMC.
- Attribute initialization GE.

## Required Skill Pass
- `ue5-gas`

## Files Touched
- `GP/Source/GPGASRuntime/Public/AttributeSets/GPPlayerAttributeSet.h` — exists or new
- `GP/Source/GPGASRuntime/Private/AttributeSets/GPPlayerAttributeSet.cpp` — exists or new
- `GP/Source/GPGASRuntime/Public/AttributeSets/GPUnitAttributeSet.h` — new
- `GP/Source/GPGASRuntime/Private/AttributeSets/GPUnitAttributeSet.cpp` — new

## Acceptance Criteria
- [ ] Editor compiles clean.
- [ ] Both attribute sets visible у GAS attribute pickers (after assigning to an ASC).
- [ ] Each `UPROPERTY(Replicated...)` has explicit `DOREPLIFETIME_CONDITION_NOTIFY` з documented condition.
- [ ] No hardcoded balance values у `.cpp`. All init via external GE або PostInitProperties default = 0.
- [ ] Clamps tested with editor-time stub: `Health` cannot go below 0 or above `MaxHealth`.
- [ ] No abilities, effects, RPCs added.

## Playtest / Validation Note
Spawn a debug actor with ASC у PIE, add `UGP_UnitAttributeSet`. Open `ShowDebug AbilitySystem`. Verify attributes appear with default values. Apply temporary debug GE that adds 50 Health — value updates.

## Risks / Edge Cases
- Replication mode mismatch — must align з ASC mode (set per actor у later slice).
- Forgotten clamp creates negative HP — covered у `PreAttributeChange`.
- `CarriedFerronite` clamp на client потребує MaxCargo replication — defer до Worker slice.

## Linked
- [`../../TDD/02_GAS_Architecture`](../../TDD/02_GAS_Architecture.md).
- [`../../TDD/07_Resource_Architecture`](../../TDD/07_Resource_Architecture.md) §Attribute Model.
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §Attributes.

## Stop Condition
STOP. Await approval before GP-S04.
