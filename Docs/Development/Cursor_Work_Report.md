# Cursor Work Report

## Task
GP-S26B2A — Editable Primitive Visual Profiles

## Status
GP-S26B2A_CODE_AND_ASSETS_READY_OPERATOR_VALIDATION_PENDING

## Branch
feature/gp-s26b2a-editable-visual-profiles

## Base
main @ 215b4b603e7fd333ef9b379103329bfac03edbf4

## DataAsset architecture
- Runtime `UGP_PrimitiveVisualProfile : UPrimaryDataAsset` (PrimaryAssetType `GPPrimitiveVisualProfile`)
- Cosmetic-only Parts array; native definitions remain authoritative fallback
- Soft CDO defaults on visual components; AlwaysCook directory + AssetManager scan for cook reliability
- Seed commandlet: `-run=GPVisualProfileSeed`

## Exact profile type
`UGP_PrimitiveVisualProfile`

## Editable fields
Profile: ProfileId, DisplayName, Category, ProfileVersion, Parts  
Part: PartName, Shape, ParentPartName, RelativeLocation/Rotation/Scale, bPresentationRoot, role flags (Body/Facing/Weapon/Turret/Animated), bTeamTintEligible, bCastShadow, bVisible, CollisionPolicy, VisibilityPolicy

## Validation policy
`ValidateProfile` / `GetValidatedDefinition` / `SanitizeDefinition` → `ValidateAndSanitizeDefinition`  
Hard fail → native fallback. Warnings for missing parents / PresentationRoot promotion. Max 32 parts; unique names; finite transforms; non-zero scale; no self-parent/cycles; supported shapes only.

## Fallback policy
Missing soft asset / load fail / invalid profile → native `MakeInfantryMeleeDefinition` / `MakeOreNodeDefinition`. Never crash actor or disable gameplay.

## Assignment policy
**Soft path constants on component CDO (policy B)** + native fallback. Not hard CDO hard-refs; not Blueprint-only. Per-instance EditAnywhere; class defaults inherit soft path.

## Cook / reference policy
- Soft paths: `/Game/GrimProtocol/VisualProfiles/DA_Visual_InfantryMelee.DA_Visual_InfantryMelee` and `.../DA_Visual_Ore.DA_Visual_Ore`
- `DefaultGame.ini`: `DirectoriesToAlwaysCook` for `/Game/GrimProtocol/VisualProfiles`
- AssetManager PrimaryAssetType `GPPrimitiveVisualProfile` with `CookRule=AlwaysCook`

## Unit integration
`UGP_UnitVisualComponent::VisualProfile` soft ptr; `SetVisualProfile` / `RebuildVisual` (CallInEditor); PostEditChangeProperty; dedicated suppress; team tint eligibility from profile; TeamId from actor.

## ResourceNode integration
`UGP_ResourceNodeVisualComponent::VisualProfile` soft ptr; same rebuild APIs; Ore tint disabled; no effect on collision / amounts / ResourceType / nav / replication.

## Editor rebuild workflow
Change profile on actor → auto rebuild. Edit DataAsset → Save → **Rebuild Visual** button (or reassign / reopen / PIE). Old parts destroyed; NoCollision preserved.

## Hierarchy policy
Two-pass builder; re-apply relative transforms after reparent; unresolved parent → actor root; empty parent → presentation root; cycle → validation fail + fallback. Caution: non-uniform parent scale inherits.

## Team tint policy
Unit: eligibility/role from profile; TeamId from gameplay. Ore: `bTeamTintEligible=false`. No material assets this stage (Engine basic material tint may be invisible).

## Inspector changes
`gp.UnitVisual.Inspect` and `gp.ResourceNode.Inspect` append: VisualProfile, VisualSource, ProfileValid, ProfileValidationErrors, ProfilePartCount, BuiltPartCount, IsUsingFallback, DuplicatePartNames, HierarchyValid (existing fields retained).

## Console rebuild command
Non-shipping: `gp.Visual.Rebuild [Unit|Resource|All]` — local cosmetic only.

## Created assets
- `/Game/GrimProtocol/VisualProfiles/DA_Visual_InfantryMelee` (3 parts)
- `/Game/GrimProtocol/VisualProfiles/DA_Visual_Ore` (5 parts)

## LFS status
`.uasset` filter=lfs in `.gitattributes`; VisualProfiles assets tracked via LFS on commit.

## Files changed
- `GP/Source/GPRuntime/Public/Visual/GPPrimitiveVisualProfile.h` (new)
- `GP/Source/GPRuntime/Private/Visual/GPPrimitiveVisualProfile.cpp` (new)
- `GP/Source/GPRuntime/Public/Visual/GPPrimitiveVisualTypes.h`
- `GP/Source/GPRuntime/Private/Visual/GPPrimitiveVisualTypes.cpp`
- `GP/Source/GPRuntime/Private/Visual/GPPrimitiveVisualBuilder.cpp`
- `GP/Source/GPRuntime/Public/Visual/GPUnitVisualComponent.h`
- `GP/Source/GPRuntime/Private/Visual/GPUnitVisualComponent.cpp`
- `GP/Source/GPRuntime/Public/Visual/GPResourceNodeVisualComponent.h`
- `GP/Source/GPRuntime/Private/Visual/GPResourceNodeVisualComponent.cpp`
- `GP/Source/GPRuntime/Private/Resources/GPResourceNode.cpp`
- `GP/Source/GPEditor/Public/Visual/GPVisualProfileSeedCommandlet.h` (new)
- `GP/Source/GPEditor/Private/Visual/GPVisualProfileSeedCommandlet.cpp` (new)
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/VisualProfiles/DA_Visual_InfantryMelee.uasset` (new, LFS)
- `GP/Content/GrimProtocol/VisualProfiles/DA_Visual_Ore.uasset` (new, LFS)
- `Docs/Development/Claude_Tasks/GP-S26B2A_Editable_Primitive_Visual_Profiles.md` (new)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Build result
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Win64 Development | Not run (candidate) |
| GP Win64 Shipping | Not run (candidate) |

## Operator validation steps
See task doc A–E (Unit edit/rebuild, Ore edit/rebuild, per-instance, fallback None, listen+client). Do not commit map placements.

## Known limitations
- Team tint on Engine basic materials unverified
- DataAsset edits require explicit Rebuild / reopen for actors
- No full unit visual catalog

## Commit SHA
(filled after commit)

## Git state
Branch `feature/gp-s26b2a-editable-visual-profiles` pushed; main untouched; no PR.
