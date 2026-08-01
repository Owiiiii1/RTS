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
- Override `InitAbilityActorInfo` to log warning if `OwnerActor != AvatarActor` (catches misconfig).
- Helper `SetProjectReplicationMode(EGameplayEffectReplicationMode)` — wraps engine setter, logs verbose.
- Override `GetReplicatedAnimMontage()` only if needed (probably defer).
- No abilities granted.

## Out of Scope
- AbilitySystemInterface usage on actors (next slices).
- Specific ability classes.
- Attribute initialization GameplayEffects.

## Required Skill Pass
- `ue5-gas`

## Files Touched
- `GP/Source/GPGASRuntime/Public/AbilitySystem/GPAbilitySystemComponent.h` — new
- `GP/Source/GPGASRuntime/Private/AbilitySystem/GPAbilitySystemComponent.cpp` — new

## Acceptance Criteria
- [ ] Compiles clean.
- [ ] `UGP_AbilitySystemComponent` selectable у actor component picker.
- [ ] No new replicated UPROPERTYs added (engine ones inherited).
- [ ] Init log fires if `OwnerActor != AvatarActor`.

## Playtest / Validation Note
Add `UGP_AbilitySystemComponent` to a temporary debug actor. PIE. Verify `ShowDebug AbilitySystem` works.

## Risks / Edge Cases
- Replication mode set after `InitAbilityActorInfo` may invalidate replicated effects — document call order у comments.

## Linked
- [`../../TDD/02_GAS_Architecture`](../../TDD/02_GAS_Architecture.md).
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §GPGASRuntime classes.

## Stop Condition
STOP. Await approval before GP-S05.
