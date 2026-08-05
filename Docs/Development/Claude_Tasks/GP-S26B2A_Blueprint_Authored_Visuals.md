# GP-S26B2A — Blueprint Authored Visuals

## Status
**GP-S26B2A_CODE_AND_BLUEPRINT_EXAMPLES_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `215b4b603e7fd333ef9b379103329bfac03edbf4`

Branch: `feature/gp-s26b2a-blueprint-authored-visuals`

## Abandoned experiment
`feature/gp-s26b2a-editable-visual-profiles` @ `54bfe62d5c6b54edfa7cdff02ff48e221f9a98ff` — **abandoned, never merged**. Do not cherry-pick / merge / continue that branch. No DataAsset visual profiles in this stage.

## Goal
Standard Unreal workflow: C++ gameplay actors + Blueprint subclasses for authored presentation (Viewport mesh editing). Native generated primitives must not stack on top of authored Blueprint presentation.

## Visual source enum
`EGP_VisualSourceMode`
- `NativeFallback` — build current native Engine basic-shape parts into `BuiltVisual`
- `AuthoredComponents` — clear generated native parts only; keep Blueprint/SCS meshes

## Unit / Resource integration
- `UGP_UnitVisualComponent::VisualSourceMode` (`EditDefaultsOnly`, default `NativeFallback`)
- `UGP_ResourceNodeVisualComponent::VisualSourceMode` (same)
- APIs: `GetVisualSourceMode`, `UsesAuthoredComponents`, `SetVisualSourceMode`, `RefreshVisualMode` (`CallInEditor`)
- Authored mode: no native build; team tint skipped
- Dedicated server: native render construction still suppressed
- No tick

## Generated ownership / cleanup
- Generated parts live only in `BuiltVisual`
- `ClearVisual()` → `DestroyBuiltParts(BuiltVisual)` only
- Never walks all actor meshes; never deletes SCS / capsule / box / selection

## Authored component contract
Presentation meshes should use:
- Collision = NoCollision
- Generate Overlap Events = false
- Can Ever Affect Navigation = false
- Gameplay collision stays on C++ root capsule/box
- Authored Blueprint owns materials/tint (no auto team tint)

## Blueprint examples
- `/Game/GrimProtocol/Units/BP_Unit_AuthoredExample` → `AGP_Unit`, AuthoredComponents, VisualRoot + Body/Forward/Weapon
- `/Game/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample` → `AGP_ResourceNode`, AuthoredComponents, VisualRoot + Base/Core/AccentA/B/C
- Seed: `-run=GPAuthoredVisualExampleSeed` (+ `-VerifyOnly`)
- LFS `.uasset`

## Inspector
Extended (existing fields kept):
- `gp.UnitVisual.Inspect`
- `gp.ResourceNode.Inspect`

New fields: VisualSourceMode, GeneratedPartCount, AuthoredPrimitiveComponentCount, NativeVisualBuilt, UsesAuthoredComponents, GeneratedCollisionDisabled, AuthoredCollisionWarnings, AuthoredNavigationWarnings, DuplicateGeneratedParts, TickEnabled

## Networking
Mode is class/default content config — not a runtime RPC. Server and client load the same Blueprint class. Gameplay replication unchanged.

## Map
`L_PrototypeArena` unchanged. No generator / population / GameDefaultMap changes.

## Operator validation (pending)

### Unit
1. Open `BP_Unit_AuthoredExample` — parent `AGP_Unit`, mode AuthoredComponents
2. Viewport: scale Weapon, move Forward, scale Body → Compile/Save
3. Place temporarily in arena (do not commit map)
4. Only authored meshes; no native humanoid overlay; select/move/attack work

### Resource
1. Open `BP_ResourceNode_AuthoredExample`, edit transforms, Compile/Save
2. Place temporarily — no native crystals overlay; box collision / amounts / nav unchanged

### Fallback
- Direct `AGP_Unit` → native InfantryMelee
- Direct `AGP_ResourceNode` → native Ore

### Mode toggle
- Copy BP → NativeFallback → native appears with authored still present
- Back to AuthoredComponents → Refresh → native cleared, authored remain

### Network
- Listen + client: same authored presentation; no duplicate native parts; gameplay replication unchanged

## Known limitations
- Authored team tint not automatic (Blueprint materials later)
- Example BPs use Engine BasicShapes only (not final art)
- DataAsset profile approach abandoned
