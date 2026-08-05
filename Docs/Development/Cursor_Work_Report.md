# Cursor Work Report

## Task
GP-S26B2A — Blueprint Authored Visuals

## Status
GP-S26B2A_CODE_AND_BLUEPRINT_EXAMPLES_READY_OPERATOR_VALIDATION_PENDING

## Branch
feature/gp-s26b2a-blueprint-authored-visuals

## Base
main @ 215b4b603e7fd333ef9b379103329bfac03edbf4

## Abandoned DataAsset branch
`feature/gp-s26b2a-editable-visual-profiles` @ `54bfe62d5c6b54edfa7cdff02ff48e221f9a98ff` — abandoned experiment, **never merged**. Not cherry-picked. Remote branch left in place; not used.

## Visual source enum
`EGP_VisualSourceMode::{ NativeFallback, AuthoredComponents }`

## Unit integration
`UGP_UnitVisualComponent`: EditDefaultsOnly `VisualSourceMode` (default NativeFallback); `GetVisualSourceMode` / `UsesAuthoredComponents` / `SetVisualSourceMode` / `RefreshVisualMode` (CallInEditor); PostEditChangeProperty + editor OnRegister; Authored clears BuiltVisual only; team tint only for NativeFallback.

## ResourceNode integration
Same mode API on `UGP_ResourceNodeVisualComponent`. Ore native build skipped in AuthoredComponents. Gameplay Box / amounts / ResourceType / nav / replication untouched.

## Generated ownership / cleanup
`ClearVisual` destroys only `BuiltVisual` parts via `GPPrimitiveVisualBuilder::DestroyBuiltParts`. Authored SCS meshes never enter BuiltVisual.

## Authored component contract
Examples set NoCollision, no overlaps, not nav-relevant. Documented: gameplay collision stays on C++ root; no mesh-bounds-as-radius; no presentation RPC.

## Blueprint example assets
| Asset | Parent | Mode | Parts |
| --- | --- | --- | --- |
| `/Game/GrimProtocol/Units/BP_Unit_AuthoredExample` | AGP_Unit | AuthoredComponents | VisualRoot, Body, Forward, Weapon |
| `/Game/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample` | AGP_ResourceNode | AuthoredComponents | VisualRoot, Base, Core, AccentA/B/C |

Seed commandlet: `-run=GPAuthoredVisualExampleSeed` (`-VerifyOnly` supported).

## Inspector fields
Appended to `gp.UnitVisual.Inspect` and `gp.ResourceNode.Inspect` without removing prior fields: VisualSourceMode, GeneratedPartCount, AuthoredPrimitiveComponentCount, NativeVisualBuilt, UsesAuthoredComponents, GeneratedCollisionDisabled, AuthoredCollisionWarnings, AuthoredNavigationWarnings, DuplicateGeneratedParts (+ TickEnabled retained).

## Networking policy
Class/default content configuration; no mode RPC; identical Blueprint class on listen+client; dedicated still suppresses native render parts.

## Team tint limitation
NativeFallback keeps existing DMI attempt on generated parts. AuthoredComponents does not auto-tint Blueprint meshes; no materials added this stage.

## Map unchanged
No `.umap` edits; no Prototype Arena generator / population / GameDefaultMap changes.

## Files changed
- `GP/Source/GPRuntime/Public/Visual/GPPrimitiveVisualTypes.h`
- `GP/Source/GPRuntime/Private/Visual/GPPrimitiveVisualTypes.cpp`
- `GP/Source/GPRuntime/Public/Visual/GPUnitVisualComponent.h`
- `GP/Source/GPRuntime/Private/Visual/GPUnitVisualComponent.cpp`
- `GP/Source/GPRuntime/Public/Visual/GPResourceNodeVisualComponent.h`
- `GP/Source/GPRuntime/Private/Visual/GPResourceNodeVisualComponent.cpp`
- `GP/Source/GPRuntime/Private/Resources/GPResourceNode.cpp`
- `GP/Source/GPEditor/GPEditor.Build.cs`
- `GP/Source/GPEditor/Public/Visual/GPAuthoredVisualExampleSeedCommandlet.h` (new)
- `GP/Source/GPEditor/Private/Visual/GPAuthoredVisualExampleSeedCommandlet.cpp` (new)
- `GP/Content/GrimProtocol/Units/BP_Unit_AuthoredExample.uasset` (new, LFS)
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset` (new, LFS)
- `Docs/Development/Claude_Tasks/GP-S26B2A_Blueprint_Authored_Visuals.md` (new)
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## LFS status for Blueprint assets
`.uasset` → `filter=lfs` via `.gitattributes`; both example BPs tracked as LFS on commit.

## GPEditor build / UHT result
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Win64 Development | Not run (candidate) |
| GP Win64 Shipping | Not run (candidate) |

Seed verify: Unit/Resource `VisualSourceMode=AuthoredComponents` **PASSED**

## Operator validation steps
See task doc: Unit Viewport edit → place; Resource edit → place; direct native fallbacks; mode toggle; listen+client. Do not commit map placements.

## Known limitations
- Authored team materials not provided
- Examples are Engine BasicShapes placeholders, not production archetypes
- Prior DataAsset profile experiment abandoned

## Commit SHA
(filled after commit)

## Git state
Branch `feature/gp-s26b2a-blueprint-authored-visuals` pushed; main untouched; no PR; abandoned DataAsset remote branch not deleted.
