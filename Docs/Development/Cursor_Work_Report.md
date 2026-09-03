# Cursor Work Report

## Status

**VOXEL_RUNTIME_CRATER_PROVEN_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Runtime `RemoveSphere` crater proven on a transient C++ `UVoxelFlatGenerator` `AVoxelWorld` (density, local bounds, mesh, collision). No production terrain service. Do not start 3B. Do not vendor `GP/Plugins/VoxelFree`. Stage 3A is not complete until operator visual validation and the event-layer decision.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Remote | `origin/terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Merge-base with `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent before this checkpoint | `6bc950a116ff93418b7cbc33ec6e80c241745337` |
| Checkpoint commit | see git HEAD after push |

No rebase, reset, stash, or clean. Operator dirty/untracked preserved.

## Exact world initialization API

C++ only. No UAsset generator. No map change.

1. `SpawnActor<AVoxelWorld>` (transient, tag `GP_VoxelRuntimeProbe`)
2. `SetGeneratorClass(UVoxelFlatGenerator::StaticClass())`
3. `VoxelSize = 100`
4. `SetRenderOctreeDepth(1)` → 64³ voxels
5. `MaxLOD = 0`, `DataOctreeInitialSubdivisionDepth = 1`
6. `bEnableCollisions = true`, `CTF_UseComplexAsSimple`, `ECC_WorldDynamic` block-all
7. `UVoxelSimpleInvokerComponent` (`LODRange`/`CollisionsRange` = 20000 cm)
8. `VoxelMaterial = UMaterial::GetDefaultMaterial(MD_Surface)`
9. `CreateWorld()` then wait loaded + proc-mesh count > 0 + mesh task count 0

## Exact generator used

`UVoxelFlatGenerator` (plugin `UCLASS`, density `Z + 0.001` in voxel space). Negative = solid.

## Exact RemoveSphere call

```
UVoxelSphereTools::RemoveSphere(
    VoxelWorld,
    Request.WorldLocation,  // world cm
    300.f,                  // radius cm
    nullptr,
    &EditedBounds,
    false,  // bMultiThreaded
    true,   // bConvertToVoxelSpace
    true);  // bUpdateRender
```

Private request: `FGPVoxelSphereSubtractRequest { FVector WorldLocation; float RadiusCm; }`. Shape = SphereSubtract. **No Depth** — deeper crater = center offset below surface.

## Coordinate / radius contract

| Item | Space |
| --- | --- |
| Position / Radius into `RemoveSphere` | world cm (`bConvertToVoxelSpace=true`) |
| `VoxelSize` | 100 cm/voxel → 300 cm = 3 voxels |
| `EditedBounds` | voxel integer box |
| `GlobalToLocal` / `LocalToGlobal` | world cm ↔ voxel index (round-trip proven) |

## EditedBounds

`(-6/7, -6/7, -6/7)` Size **(13,13,13)** vs world **(64,64,64)**. Valid, finite, local.

## Density / query before/after

| Sample | Before | After |
| --- | --- | --- |
| Crater voxel `(0,0,-1)` | -0.999 (solid) | empty (positive) |
| Far voxel `(20,0,-1)` | -0.999 | -0.999 unchanged |
| Density-column surface Z | 100 (actor Z=200) | — |

## Collision before/after

Downward line trace vs the probe actor (`ECC_WorldDynamic` / Visibility fallback):

| Sample | Before Z | After Z |
| --- | --- | --- |
| Crater | 199.9 | **-100.0** (delta 299.9 cm) |
| Far | 199.9 | unchanged (< 80 cm) |

Collision poll after mesh idle: `waitTicks=0`.

## Render update proof

`IsLoaded=true`, mesh tasks 0, **4** `UVoxelProceduralMeshComponent` before and after edit (no full-world rebuild). `bUpdateRender=true` → `UpdateWorld` → LOD `UpdateBounds`.

## Locality

Edited 13³ vs 64³ world. Far density and far collision unchanged. Mesh count stayed 4.

## Surface query findings

No `QuerySurfaceZ(XY)`. Best later GP candidates: (1) downward world trace after collision idle; (2) density sign-change along Z. `FindClosestNonEmptyVoxel` is **neighbor-only**, not XY→Z.

## Tests

| Test | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **Succeeded** |
| `gp.Voxel.RunPluginCompileProbeContractTest` | Failures=0 |
| `gp.Voxel.RunRuntimeCraterProbeContractTest` | Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | Failures=0 |
| Fatal / assertion | none |

## Operator commands

Do **not** save `L_PrototypeArena`.

1. Open PIE on `L_PrototypeArena`
2. Console: `gp.Voxel.SpawnRuntimeProbe` — small flat voxel slab (near pawn, else `(25000,0,200)`)
3. Fly to the logged location
4. Console: `gp.Voxel.ApplyProbeCrater` — `RemoveSphere` hole
5. Inspect mesh; walk / cursor-trace the crater

## Files changed (this checkpoint)

- `GP/Source/GPRuntime/Private/Voxel/GPVoxelRuntimeProbeAdapter.h/.cpp` — private Voxel adapter
- `GP/Source/GPRuntime/Public/Debug/GPVoxelRuntimeCraterProbeContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPVoxelRuntimeCraterProbeContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPVoxelRuntimeVisualProbe.cpp`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md` — `RUNTIME_CRATER_PROVEN`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- this report

## Protected audit

Unchanged / not committed:

- `GP/Config/`, `GP/Content/` including `L_PrototypeArena`, `GP/GP.uproject`, `Tools/`
- **`GP/Plugins/` (`VoxelFree`) still untracked**

## Exact next action

Operator PIE visual pass of `SpawnRuntimeProbe` + `ApplyProbeCrater`. Then decide GP authoritative deformation event layer. Do not vendor the plugin. Do not start 3B.
