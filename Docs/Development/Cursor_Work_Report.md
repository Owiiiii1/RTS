# Cursor Work Report

## Status

**VOXEL_PLUGIN_UE58_INTEGRATION_PROVEN_READY_FOR_RUNTIME_CRATER_PROBE**

**INTERMEDIATE / NOT MERGE READY**

Voxel Plugin Free Legacy installed locally, UE 5.8.1 compile + load + GPRuntime header compile probe proven. No production terrain service. Runtime crater not demonstrated. Do not start Worker leveling / Foundation / placement migration. Do not vendor `GP/Plugins/VoxelFree` yet.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Remote | `origin/terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Merge-base with `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent before this checkpoint | `5667277bee05d3818873bb885866c4183f7800ec` |
| Checkpoint commit | see git HEAD after push (must match `origin/terrain/gp-voxel-foundation`) |

No rebase, reset, stash, or clean. Operator dirty/untracked preserved.

## Plugin folder structure

Canonical: `D:\Progects\RTS\GP\Plugins\VoxelFree\VoxelFree.uplugin`

Not nested `VoxelPluginFreeLegacy-master`.

Top-level: `Source/`, `Binaries/`, `Content/`, `Config/`, `Resources/`, `Shaders/`, `Intermediate/` (generated), `.gitignore`, `README.md`, leftover `VoxelFree-434-159fd19a0-5.8-Binaries.zip` (~1.56 GB; do not vendor).

## Version metadata

| Field | Value |
| --- | --- |
| FriendlyName | Voxel Plugin Free Legacy |
| Version | 434 |
| VersionName | 159fd19a0 |
| EngineVersion | 5.8.0 |
| EnabledByDefault | absent |
| CanContainContent | true |
| Installed | true |
| Modules | Voxel, VoxelGraph, VoxelHelpers (Runtime); VoxelEditor, VoxelGraphEditor, VoxelEditorDefault (Editor); VoxelExamples (Runtime Win64/Linux) |
| Plugin deps | Niagara, ProceduralMeshComponent |

## Binary verification

`Binaries/Win64/UnrealEditor.modules` BuildId **`55116800`** = UE 5.8.1 CompatibleChangelist.

Win64 DLLs: UnrealEditor-Voxel, VoxelEditor, VoxelEditorDefault, VoxelExamples, VoxelGraph, VoxelGraphEditor, VoxelHelpers (+ PDBs).

UBT compiled plugin from source on first GPEditor after plugin add. Precompiled editor modules then loaded. Missing binaries would trigger source compile (observed).

## Editor / build result

| Step | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **Succeeded** (~337s first; probe rebuild 5.8s) |
| Plugin discovered without `.uproject` Plugins[] entry | Yes (`VoxelFree.uplugin has been added`) |
| Vendor warnings | C4305 double→float in plugin headers only. Not patched. |
| UnrealEditor-Cmd `L_PrototypeArena` `-game -NullRHI` | Mounted VoxelFree; loaded Voxel/VoxelHelpers/VoxelGraph/VoxelExamples; `LogVoxel: VOXEL_DEBUG=0`; no Fatal / Assertion |
| `gp.Voxel.RunPluginCompileProbeContractTest` | **Complete Failures=0** (A_WorldClass, B_SphereTools, C_DataTools, D_IsVoxelPluginProFalse) |
| GP Dev/Shipping | not run (not required) |

`GP.uproject` Plugins[] left unchanged (ModelingToolsEditorMode, GameplayAbilities, EnhancedInput, CommonUI, ModelViewViewModel). No VoxelFree entry added.

## Exact runtime API symbols

| Capability | Symbol | Header | Module |
| --- | --- | --- | --- |
| World owner | `AVoxelWorld` | `VoxelWorld.h` | Voxel |
| World↔voxel | `GlobalToLocal` / `LocalToGlobal` (+ Float) | same | Voxel |
| Density | `UVoxelDataTools::GetValue` / `SetValue` / `GetInterpolatedValue` | `VoxelTools/VoxelDataTools.h` | Voxel |
| Crater | `UVoxelSphereTools::RemoveSphere` | `VoxelTools/Gen/VoxelSphereTools.h` | Voxel |
| Fill | `UVoxelSphereTools::AddSphere` / `SetValueSphere` | same | Voxel |
| Surface-ish | `UVoxelDataTools::FindClosestNonEmptyVoxel` | VoxelDataTools.h | Voxel |
| Bounds | `FVoxelIntBox* OutEditedBounds` | sphere tools | Voxel |
| Save | `GetSave` / `GetCompressedSave` / `LoadFromSave` | VoxelDataTools.h | Voxel |
| Pro check | `UVoxelBlueprintLibrary::IsVoxelPluginPro()` → false | VoxelBlueprintLibrary | Voxel |
| TCP MP | `UVoxelMultiplayerTcpInterface::ConnectToServer` / `StartServer` | **Pro stub, returns false** | Voxel |

C++ crater signature:

`UVoxelSphereTools::RemoveSphere(AVoxelWorld*, const FVector& Position, float Radius, TArray<FModifiedVoxelValue>*, FVoxelIntBox*, bool bMultiThreaded=true, bool bConvertToVoxelSpace=true, bool bUpdateRender=true)`

## Crater feasibility

**Yes at API level.** Chain: server deformation request → optional `GlobalToLocal` → `RemoveSphere` with `bConvertToVoxelSpace=true`, `bUpdateRender=true` → `FVoxelToolHelpers::UpdateWorld` → LOD `UpdateBounds` → component `UpdateCollision` / optional `UpdateNavigation`.

One default `RemoveSphere`: writes values, triggers render update, then collision recook on affected chunks. Sync overload is not async; collision cook may still be async. Optional `OutEditedBounds`.

**Not PIE-demonstrated.** Next checkpoint.

## Collision / render behavior

Local to edited `FVoxelIntBox` / chunk mesh components. Not world-wide. `UpdateNavigation` calls `FNavigationSystem::UpdateComponentData` on that component only.

## Event replay feasibility

**EVENT_REPLAY_FEASIBLE** (not bit-identical).

`SphereEdit` has no Rand and no time input. Risks: `bMultiThreaded` write order, float density, async LOD/collision timing. Prefer `bMultiThreaded=false` for experiments. Plugin TCP is Pro-only — use GP event log + local apply (Option B).

## License / vendor recommendation

**D. DO NOT VENDOR; DOCUMENT INSTALL.** Not executed.

Marketplace plugin (Voxel Plugin SAS). FastNoise MIT only for that subtree. Plugin `.gitignore` already excludes Binaries/Intermediate. Leftover 1.56 GB ZIP must never be committed.

## Plugin folder size

| Tree | Size |
| --- | --- |
| Total `GP/Plugins/VoxelFree` | ~3.9 GB |
| Source | ~4.4 MB |
| Binaries | ~639 MB (DLLs ~23 MB + PDBs ~616 MB) |
| Content | ~206 MB |
| Intermediate | ~1.53 GB (generated) |
| leftover ZIP | ~1.56 GB |

## Source changes (committed this checkpoint)

- `GP/Source/GPRuntime/GPRuntime.Build.cs` — PrivateDependency `Voxel`
- `GP/Source/GPRuntime/Private/Debug/GPVoxelPluginCompileProbe.cpp` — `#if !UE_BUILD_SHIPPING` compile/link probe; does not spawn terrain

## Docs changes

- `Docs/Development/Voxel_Plugin_Technical_Spike.md` — status `PLUGIN_INSTALLED_AND_AUDITED`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md` — factual install/API update
- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md` — factual install update
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md` — 3A in progress, crater probe next
- this report

## Protected audit

Unchanged operator dirty/untracked (not committed):

- `GP/Config/DefaultEngine.ini`, `DefaultGame.ini`
- `GP/Content/` including `L_PrototypeArena`, authored Blueprints/DataAssets, VFX packs
- `GP/GP.uproject`
- `Tools/`

**Separate, not committed:** `GP/Plugins/` (`VoxelFree` operator-local).

## Exact next action

Runtime crater probe on `terrain/gp-voxel-foundation`: use `AVoxelWorld` + `UVoxelSphereTools::RemoveSphere`, confirm mesh + collision on affected bounds. Do not vendor the plugin. Do not start 3B.
