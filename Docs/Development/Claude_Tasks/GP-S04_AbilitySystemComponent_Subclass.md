# GP-S04 AbilitySystemComponent Subclass

## Slice Group
Slice 1 — Foundation

## Code Allowed
Yes — after GP-S03 approval.

## Depends On
- GP-S03.

## Goal
Provide project-wide `UGP_AbilitySystemComponent` базу, що дозволяє per-actor configure replication mode. Single subclass для всіх ASCs у проєкті.

## Scope
- Create `UGP_AbilitySystemComponent : UAbilitySystemComponent`.
- Override `InitAbilityActorInfo`:
  - warning if Owner or Avatar is null;
  - diagnostic warning if `OwnerActor != AvatarActor` with text that asks to **verify this is intentional** (PlayerState Owner + Pawn Avatar is a normal GAS pattern — not an unconditional misconfiguration);
  - verbose log when Owner == Avatar.
- Helper `SetProjectReplicationMode(EGameplayEffectReplicationMode)` — wraps engine `SetReplicationMode`, logs selected mode; call **before** `InitAbilityActorInfo`.
- Do **not** override `GetReplicatedAnimMontage`.
- No abilities granted. No AttributeSet ownership inside ASC. No new replicated UPROPERTYs.

## Out of Scope
- AbilitySystemInterface usage on actors (actor integration slices).
- Specific ability classes.
- Attribute initialization GameplayEffects.
- Permanent debug actor / temp Blueprint / temp map / temp GE.
- `ShowDebug AbilitySystem` runtime validation (deferred until first real actor implements `IAbilitySystemInterface`).
- `GameplayCueNotifyPaths` configuration (Gameplay Cues out of scope; defer to Gameplay Cue slice).

## Required Skill Pass
- `ue5-gas`

## Files Touched
- `GP/Source/GPGASRuntime/Public/AbilitySystem/GPAbilitySystemComponent.h` — new
- `GP/Source/GPGASRuntime/Private/AbilitySystem/GPAbilitySystemComponent.cpp` — new

## Implementation status (2026-08-01)
Status: **DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S05 until explicitly assigned.

### Implemented
- `UGP_AbilitySystemComponent` with `BlueprintSpawnableComponent` / ClassGroup Abilities.
- `SetProjectReplicationMode` → engine `SetReplicationMode` + LogTemp; no duplicate replicated field.
- `InitAbilityActorInfo` Super + null warnings + Owner≠Avatar diagnostic ("verify this is intentional") + Owner==Avatar Verbose.
- No `GetReplicatedAnimMontage` override, no abilities/effects/RPC, no AttributeSets inside ASC.
- Builds: GPEditor Development, GP Development, GP Shipping — **PASSED**.
- Operator: Editor/module load, Component Picker (`GP Ability System Component`), temporary Blueprint compile, PIE — **PASSED**. Blocking errors: **NONE**.

### Deferred
- **Actor integration slice:** live Attribute Picker validation; PIE `ShowDebug AbilitySystem` on first real `IAbilitySystemInterface` actor.
- **Gameplay Cue slice:** `GameplayCueNotifyPaths` (warning expected until cues are used; not GP-S04 scope).

## Acceptance Criteria
- [x] Compiles clean (GPEditor / GP Dev / GP Shipping).
- [x] `UGP_AbilitySystemComponent` selectable у actor component picker (operator).
- [x] No new replicated UPROPERTYs added (engine ones inherited).
- [x] Init diagnostic warning if `OwnerActor != AvatarActor` ("verify this is intentional"; not treated as unconditional misconfig).
- [x] `ShowDebug AbilitySystem` / live Attribute Picker validation — **deferred** to actor integration slice (accepted for GP-S04 close).
- [x] Operator Editor/module load, Blueprint compile, PIE PASSED; blocking errors NONE.
- [x] Tech lead accepted GP-S04.
- [x] Operator accepted GP-S04.

## Playtest / Validation Note
**GP-S04 (closed):** Open project; confirm no module/load errors; find **GP Ability System Component** in Actor Component picker; temporary BP compile OK; PIE clean. No permanent debug actor committed.

**Actor integration (deferred):** On first real ASI actor, verify `ShowDebug AbilitySystem` and AttributeSet visibility.

**Gameplay Cue slice (deferred):** Configure `GameplayCueNotifyPaths` when cues are introduced.

## Risks / Edge Cases
- Replication mode set after `InitAbilityActorInfo` may invalidate replicated effects — call order documented in helper/class comments.
- Owner≠Avatar warning will fire for intentional PlayerState→Pawn setups; treat as diagnostic.
- Operator saw non-blocking engine/plugin warnings (`r.MotionVectorSimulation`, ModelViewViewModelBlueprint ClassViewer, No GameplayCueNotifyPaths) — not GP-S04 defects.

## Linked
- [`../../TDD/02_GAS_Architecture`](../../TDD/02_GAS_Architecture.md).
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §GPGASRuntime classes.

## Stop Condition
STOP. GP-S04 closed as DONE. Await explicit assignment before GP-S05.
