# GP-S27A2 Editor Generator Foundation

## Status
**GP-S27A2_CODE_AND_BASE_MAP_READY_OPERATOR_VALIDATION_PENDING**

## Baseline
`main` @ `326c881ae0578973b79b92de2043976bfbcd6121`

Architecture sources:
- `Docs/Development/Claude_Tasks/GP-S27A_Prototype_Arena_Analysis.md`
- `Docs/Development/Claude_Tasks/GP-S27A1_Resource_Node_Foundation.md`

Branch: `feature/gp-s27a2-editor-generator-foundation`  
Implementation: `7508fc8eca2acc7f277fe3d9ed7965db15df5711`  
Nav bounds correction: `bf98e85a69971767cf44b990ac54701d3da46d1e`

## Goal
Editor-only module + one-shot tool that creates and saves infrastructure-only persistent map `/Game/GrimProtocol/Maps/L_PrototypeArena`. No gameplay population. No S27A3.

## Operator blocker (fixed)
Initial generator used `CubeBuilder::Build` without `UActorFactory::CreateBrushForVolumeActor`, leaving empty brush geometry (SphereRadius=0, MapCheck collision 0 radius, no Recast).

### Correction
- Use `UActorFactory::CreateBrushForVolumeActor` + `UCubeBuilder` 4500×4500×500
- Pre-save validation: BrushComponent, non-zero SphereRadius, positive BoxExtent
- Unlock `AsyncLoadLock` + `InitialLock`; commandlet uses `NavSys->Build()` (full Editor uses `FEditorBuildUtils::EditorBuild`)
- Map Check after save/load: **0 Error(s), 0 Warning(s)**
- Inspect: `NavBoundsValid=true`, Extent≈(2250,2250,250), `RecastNavMeshCount=1`, `ReadyForPopulation=true`

## Shipped

### Editor module `GPEditor`
- Type: Editor (`.uproject` + `GPEditor.Target.cs`)
- Absent from `GP` Game target / packaged Shipping graph

### Commands / menu
- `gp.Editor.GeneratePrototypeArena` / `gp.Editor.InspectPrototypeArena`
- Tools → Grim Protocol → Generate Prototype Arena
- Service: `FGPPrototypeArenaGenerator`
- Commandlet: `-run=GPPrototypeArenaGenerate` (`-InspectOnly`)

### Map / nav
- Non–World-Partition `L_PrototypeArena.umap` (LFS)
- Valid `GP_Arena_NavMeshBounds` brush; Recast present after generation/reload
- Abort-if-exists unchanged

## Intentionally not done
Units, ResourceNodes, combat pairs, runtime generator, rebuild user command, Blueprint/DataAsset/materials, default map switch, S27A3.

## Build / generation (candidate + correction)
- GPEditor Win64 Development — **PASSED**
- Defective umap deleted and regenerated in branch
- Second generate — ExistingMapAbort
- GP Dev/Shipping — deferred to finalization

## Operator validation
See `Docs/Development/Cursor_Work_Report.md` (recheck P / green nav).
