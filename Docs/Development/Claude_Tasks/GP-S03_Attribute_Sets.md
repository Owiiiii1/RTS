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
  - Clamp `MaxHealth` to `>= 0` у `PreAttributeChange`.
  - Clamp `CarriedFerronite` to `>= 0` (no `MaxCargo` attribute in MVP AttributeSet; upper clamp deferred to Worker/Cargo slice / WorkerCarryCapacity).
- Файл placement: `GPGASRuntime/Public/AttributeSets/`, `GPGASRuntime/Private/AttributeSets/`.

## Out of Scope
- ASC subclass (next slice).
- GameplayEffects.
- Damage calculation MMC.
- Attribute initialization GE.
- Debug actor / temporary GE / Attribute Picker runtime validation (requires ASC → **GP-S04**).

## Required Skill Pass
- `ue5-gas`

## Files Touched
- `GP/Source/GPGASRuntime/Public/AttributeSets/GPAttributeMacros.h` — new (ATTRIBUTE_ACCESSORS)
- `GP/Source/GPGASRuntime/Public/AttributeSets/GPPlayerAttributeSet.h` — new
- `GP/Source/GPGASRuntime/Private/AttributeSets/GPPlayerAttributeSet.cpp` — new
- `GP/Source/GPGASRuntime/Public/AttributeSets/GPUnitAttributeSet.h` — new
- `GP/Source/GPGASRuntime/Private/AttributeSets/GPUnitAttributeSet.cpp` — new
- `GP/Config/DefaultEngine.ini` — CommonUI `GameViewportClientClassName`

## Implementation status (2026-08-01)
Status: **DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S04 until explicitly assigned.

### Implemented
- Full GAS accessors + `ReplicatedUsing` + `GAMEPLAYATTRIBUTE_REPNOTIFY` + `DOREPLIFETIME_CONDITION_NOTIFY` (`REPNOTIFY_Always`).
- Player replication: OwnerOnly for OrbitalFerronite / MaxUnits / CurrentUnits; COND_None for FerroniteScore.
- Unit replication: COND_None for all attributes (TDD/13 Mixed / standard GAS).
- Clamps in `PreAttributeChange` as above.
- Defaults remain 0; no hardcoded balance values.
- Builds: GPEditor Development, GP Development, GP Shipping — **PASSED** (UE 5.8).
- Operator: Editor restart / GPGASRuntime startup / PIE — **PASSED**.
- CommonUI viewport: `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`; `LogUIActionRouter` error **ABSENT** after restart + PIE.

### Deferred to GP-S04 (no ASC in this slice)
- Assign AttributeSets to an ASC host.
- GAS Attribute Picker visibility after ASC assignment.
- PIE debug actor + temporary GE + `ShowDebug AbilitySystem` runtime validation.
- Acceptance items that require an ASC-backed actor are **not blocking GP-S03 close**; they move to GP-S04.

## Acceptance Criteria
- [x] Editor compiles clean (GPEditor / GP Dev / GP Shipping).
- [x] Both attribute sets visible у GAS attribute pickers (after assigning to an ASC) — **deferred to GP-S04** (no ASC in GP-S03; not blocking close).
- [x] Each `UPROPERTY(Replicated...)` has explicit `DOREPLIFETIME_CONDITION_NOTIFY` з documented condition.
- [x] No hardcoded balance values у `.cpp`. All init via external GE або PostInitProperties default = 0.
- [x] Clamps implemented in `PreAttributeChange` (`Health` `[0, MaxHealth]`, `MaxHealth >= 0`, `CarriedFerronite >= 0`). Runtime stub GE clamp test — **deferred to GP-S04**.
- [x] No abilities, effects, RPCs added.
- [x] Operator Editor restart / module startup / PIE PASSED; CommonUI viewport error ABSENT.
- [x] Tech lead accepted GP-S03.
- [x] Operator accepted GP-S03.

## Playtest / Validation Note
**GP-S03 (closed):** open project, confirm no module/load errors, PIE clean, CommonUI viewport error absent. No ASC in this slice.

**GP-S04 (deferred — do not start here):** Spawn a debug actor with ASC у PIE, add `UGP_UnitAttributeSet`. Open `ShowDebug AbilitySystem`. Verify attributes appear with default values. Apply temporary debug GE that adds 50 Health — value updates. Confirm Attribute Picker lists both sets.

## Risks / Edge Cases
- Replication mode mismatch — must align з ASC mode (set per actor у later slice).
- Forgotten clamp creates negative HP — covered у `PreAttributeChange`.
- `CarriedFerronite` upper clamp / MaxCargo — defer до Worker/Cargo slice (WorkerCarryCapacity); floor clamp only in GP-S03.
- TDD/02 §AttributeSets still lists pre-pivot attributes; GP-S03 followed TDD/13 + task + stage prompt. Docs sync recommended later.

## Linked
- [`../../TDD/02_GAS_Architecture`](../../TDD/02_GAS_Architecture.md).
- [`../../TDD/07_Resource_Architecture`](../../TDD/07_Resource_Architecture.md) §Attribute Model.
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §Attributes.

## Stop Condition
STOP. GP-S03 closed as DONE. Await explicit assignment before GP-S04.
