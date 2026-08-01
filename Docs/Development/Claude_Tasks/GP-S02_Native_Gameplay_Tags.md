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
- Register tags via `UGameplayTagsManager::AddNativeGameplayTag` у `.cpp`.
- Cover full Active MVP taxonomy per authoritative [`../../TDD/09_Gameplay_Tags`](../../TDD/09_Gameplay_Tags.md) (TDD/13 §Gameplay Tags synchronized).
- Provide static accessor `FGPGameplayTags::Get()` returning const ref to singleton init in module startup.
- Wire tag initialization in `FGPGASRuntimeModule::StartupModule()`.

## Out of Scope
- Tag-based gameplay logic.
- Tag editor utility windows.
- Localization of tag display names.
- Deprecated / Pre-Pivot tags and rejected aliases (`Team.A/B`, `Notify.InsufficientOrbital`).

## Required Skill Pass
- `ue5-gas`
- `gp-mechanics-validator`

## Files Touched
- `GP/Source/GPGASRuntime/Public/Tags/GPGameplayTags.h` — new
- `GP/Source/GPGASRuntime/Private/Tags/GPGameplayTags.cpp` — new
- `GP/Source/GPGASRuntime/Private/GPGASRuntime.cpp` — call `FGPGameplayTags::InitializeNativeTags()` в `StartupModule`
- `Docs/TDD/09_Gameplay_Tags.md` — authoritative taxonomy + tech-lead lock
- `Docs/TDD/13_Architecture_Proposal.md` — §Gameplay Tags synchronized

## Tech-lead decisions (2026-08-01 UNBLOCK)

1. Teams: `GP.Team.Neutral`, `GP.Team.Player.One`, `GP.Team.Player.Two` — not A/B.
2. Commands include `GP.Command.AttackMove`.
3. Keep `GP.Command.Sell`, `Demolish`, `CancelOrder`.
4. Keep `GP.Unit.Type.SalvageWalker`.
5. Notify: `GP.Notify.InsufficientOrbitalFerronite` — not `InsufficientOrbital`.
6. TDD/09 is authoritative; TDD/13 tag section must mirror it.

## Acceptance Criteria
- [x] All Active MVP tags registered with descriptions.
- [x] `FGPGameplayTags::Get().Command_Move` is a typed registered member.
- [x] Tag Manager editor window shows full `GP.*` tree (operator).
- [x] No magic-string tag references у project codebase outside native registration (`RequestGameplayTag` absent).
- [x] No temporary DataAssets created for autocomplete checks.
- [x] Replication-safe: tags registered in `GPGASRuntime` `StartupModule` before actors access them.
- [x] Duplicate registration guarded by static flag.
- [x] Builds: GPEditor Development, GP Development, GP Shipping.
- [x] Editor OPENED; PIE PASSED; rejected aliases not active (operator).
- [x] Tech lead accepted. Operator accepted.

## Output
- Status: **DONE**
- Initialization: `FGPGASRuntimeModule::StartupModule()` → `FGPGameplayTags::InitializeNativeTags()` (idempotent).
- Builds (UE 5.8):
  - `GPEditor Win64 Development` — PASSED
  - `GP Win64 Development` — PASSED
  - `GP Win64 Shipping` — PASSED
- Operator: Editor OPENED; GP.* tree present; PIE PASSED; rejected aliases unused.
- Tech lead accepted. Operator accepted.
- STOP. Do **not** start GP-S03.

## Playtest / Validation Note
Open Tag Manager (`Project Settings → Gameplay Tags`). Verify full `GP.*` tree present. Confirm no startup/module errors. Run PIE. Do **not** create temporary DataAssets solely for tag autocomplete checks.

## Risks / Edge Cases
- Tag rename later — must avoid via early-locked taxonomy.
- Tag double-registration — guarded via `bNativeTagsRegistered` static flag.
- TDD/12 Feedback Matrix updated to canonical `GP.Notify.InsufficientOrbitalFerronite` (rejected alias removed from active UI docs).

## Linked
- [`../../TDD/09_Gameplay_Tags`](../../TDD/09_Gameplay_Tags.md).
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §Gameplay Tags.
- [`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md) — magic-string tag ban.

## Stop Condition
DONE. Closed after tech lead + operator acceptance. Do **not** start GP-S03 here.
