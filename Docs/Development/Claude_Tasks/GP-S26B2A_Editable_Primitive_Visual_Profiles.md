# GP-S26B2A — Editable Primitive Visual Profiles

## Status
**GP-S26B2A_CODE_AND_ASSETS_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `215b4b603e7fd333ef9b379103329bfac03edbf4`

Branch: `feature/gp-s26b2a-editable-visual-profiles`  
Implementation: `54bfe62d5c6b54edfa7cdff02ff48e221f9a98ff`

## Goal
Editable DataAsset visual profiles for InfantryMelee and Ore, with native fallback. Editor rebuild without C++ rebuild. No gameplay / collision / nav / replication coupling.

## Shipped (implementation candidate)

### DataAsset type
- `UGP_PrimitiveVisualProfile : UPrimaryDataAsset`
- Paths: `GP/Source/GPRuntime/Public/Visual/GPPrimitiveVisualProfile.h` (+ `.cpp`)
- Fields: `ProfileId`, `DisplayName`, `Category` (Unit/Resource/Building/Generic), `ProfileVersion`, `Parts`
- No gameplay stats in profile

### Editable parts
- `FGP_PrimitiveVisualPart` fully `EditAnywhere`
- Fields: PartName, Shape, ParentPartName, RelativeLocation/Rotation/Scale, role flags, `bTeamTintEligible`, `bCastShadow`, `bVisible`, `bPresentationRoot`, collision/visibility policies
- Shapes: Cube / Sphere / Cylinder / Cone / Capsule (resolver-backed)

### Validation
- `ValidateProfile` / `GetValidatedDefinition` / `SanitizeDefinition`
- Shared: `GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition`
- Checks: non-empty unique PartNames, max 32, finite transforms, non-zero scale, supported shape, self-parent/cycle, PresentationRoot fallback, missing parent → warning + actor root
- Invalid asset → warning + native fallback (no crash / no gameplay impact)

### Assignment policy (chosen: B + native fallback + cook always)
- Soft default paths on visual component CDOs:
  - `/Game/GrimProtocol/VisualProfiles/DA_Visual_InfantryMelee.DA_Visual_InfantryMelee`
  - `/Game/GrimProtocol/VisualProfiles/DA_Visual_Ore.DA_Visual_Ore`
- If asset missing/invalid → native InfantryMelee / Ore definitions (authoritative fallback retained)
- Cook: `DirectoriesToAlwaysCook` + AssetManager `GPPrimitiveVisualProfile` AlwaysCook
- No Blueprint subclasses

### Integration
- `UGP_UnitVisualComponent::VisualProfile` (EditAnywhere, non-replicated)
- `UGP_ResourceNodeVisualComponent::VisualProfile` (EditAnywhere, non-replicated)
- APIs: `SetVisualProfile`, `RebuildVisual` (`CallInEditor`), `GetActiveVisualSource`, `IsUsingFallback`
- Dedicated server: no render parts
- Team tint: eligibility from profile; TeamId from gameplay actor; Ore tint disabled

### Hierarchy / builder
- Two-pass create then reparent + re-apply relative transforms
- Unresolved parent → actor root
- Empty parent (non-root) → presentation root
- Documented caution: non-uniform parent scale inherits

### Editor workflow
- Profile change on placed actor → `PostEditChangeProperty` rebuild
- Editor `OnRegister` rebuild (non-game world)
- **Rebuild Visual** CallInEditor button on both components
- DataAsset edit: Save → Rebuild Visual (or reassign / reopen map / PIE)

### Inspector / console
- `gp.UnitVisual.Inspect` — added profile/source/validation fields (existing fields kept)
- `gp.ResourceNode.Inspect` — same profile fields appended
- `gp.Visual.Rebuild [Unit|Resource|All]` — non-shipping local cosmetic rebuild

### Content assets
- `/Game/GrimProtocol/VisualProfiles/DA_Visual_InfantryMelee` (Body/Forward/Weapon)
- `/Game/GrimProtocol/VisualProfiles/DA_Visual_Ore` (Base/Core/AccentA/B/C)
- Seeded via `-run=GPVisualProfileSeed` (GPEditor commandlet)
- Tracked as `.uasset` via Git LFS

## Intentionally not done
- No Prototype Arena generator / map population / GameDefaultMap change
- No new gameplay archetypes / unit catalog / projectiles / combat animations
- No custom meshes / materials / Niagara / skeletal / construction / selection UI for Ore
- No runtime profile replication
- No GP Dev/Shipping builds (candidate: GPEditor + UHT only)

## Operator validation (pending)

### A. Unit
1. Place `AGP_Unit` temporarily (do not commit map).
2. Confirm default `DA_Visual_InfantryMelee`.
3. Edit Weapon length / Forward offset / Body scale in DataAsset → Save.
4. Press **Rebuild Visual** on component.
5. Visual changes without C++ rebuild.

### B. Ore
1. Place `AGP_ResourceNode`.
2. Edit Core height / Accent tilt in `DA_Visual_Ore` → Save → Rebuild Visual.
3. Shape changes; collision / CurrentAmount unchanged.

### C. Per-instance
1. Two units: one default profile, one None or alternate.
2. Visuals may differ; gameplay class remains `AGP_Unit`.

### D. Fallback
1. Profile=None → Rebuild → native visual; Inspect shows `NativeFallback`.

### E. Network
1. Listen + client: same saved profile visual locally; profile not replicated via RPC; gameplay replication unchanged.

## Known limitations
- Engine default material team tint still unverified (no material assets this stage)
- DataAsset live-edit does not auto-push to all actors until Rebuild / reassign / reopen
- Soft path requires cooked/always-cook directory for packaged builds
