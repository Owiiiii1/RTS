# GP-S02 Native Gameplay Tags Registry

## Slice Group
Slice 1 — Foundation

## Code Allowed
Yes — after GP-S01 approval.

## Depends On
- GP-S01.

## Goal
Register the MVP `GP.*` gameplay tag taxonomy natively у `GPGASRuntime`. Provide single accessor struct `FGPGameplayTags::Get()`. No magic-string tags anywhere downstream.

## Scope
- Create `FGPGameplayTags` struct у `GPGASRuntime/Public/Tags/GPGameplayTags.h`.
- Register tags via `UE_DEFINE_GAMEPLAY_TAG_STATIC` or `FGameplayTag::AddNativeGameplayTag` у `.cpp`.
- Cover full taxonomy per [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §Gameplay Tags (Match.State.*, Unit.Type/State.*, Building.Type/Role.*, Resource.*, Command.*, Ability.*, Capability.*, Selection.Type.*, Faction.*, Team.*, Effect.Source.*, Notify.*).
- Provide static accessor `FGPGameplayTags::Get()` returning const ref to singleton init in module startup.
- Wire tag initialization in `FGPGASRuntimeModule::StartupModule()`.

## Out of Scope
- Tag-based gameplay logic.
- Tag editor utility windows.
- Localization of tag display names.

## Required Skill Pass
- `ue5-gas`
- `gp-mechanics-validator`

## Files Touched
- `GP/Source/GPGASRuntime/Public/Tags/GPGameplayTags.h` — new
- `GP/Source/GPGASRuntime/Private/Tags/GPGameplayTags.cpp` — new
- `GP/Source/GPGASRuntime/Private/GPGASRuntime.cpp` (module file) — call `FGPGameplayTags::InitializeNativeTags()` в `StartupModule`

## Acceptance Criteria
- [ ] All tags registered with descriptions.
- [ ] `FGPGameplayTags::Get().Command_Move` returns valid tag.
- [ ] Tag Manager editor window shows full `GP.*` tree.
- [ ] No magic-string tag references у project codebase (CI grep gate passes).
- [ ] No DataAssets reference tags via string (use `FGameplayTagContainer` + editor picker).
- [ ] Replication-safe: tags registered before any actor accesses them (StartupModule order).

## Playtest / Validation Note
Open Tag Manager (`Project Settings → GameplayTags`). Verify full `GP.*` tree present. Pick any tag у an editor field (e.g., `UnitTags` placeholder on a temporary DA) — autocomplete works.

## Risks / Edge Cases
- Tag rename later — must avoid via early-locked taxonomy.
- Tag double-registration — guard via `bRegistered` static flag.

## Linked
- [`../../TDD/09_Gameplay_Tags`](../../TDD/09_Gameplay_Tags.md).
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §Gameplay Tags.
- [`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md) — magic-string tag ban.

## Stop Condition
STOP. Await approval before GP-S03.
