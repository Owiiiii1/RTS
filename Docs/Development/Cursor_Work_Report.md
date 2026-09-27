# Cursor Work Report

## Status

**TERRAIN_DEFORMATION_PRODUCTION_LAYER_READY_FOR_OPERATOR_VALIDATION**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A is not complete. Do not start Worker leveling, Foundation, or placement migration.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `f9c3296c33d9a6673334049341fd95634a39a164` |
| Checkpoint commit | recorded in the follow-up commit |
| Engine | UE 5.8.3 |

## Production owner

`UGP_TerrainDeformationComponent` is a default subobject of `AGP_GameState`.

`AGP_GameState` already owns match-global authority services. Deformation is match-global, so it lives there. Unlike `UGP_FogOfWarComponent`, this component replicates (`SetIsReplicatedByDefault(true)`). The event log uses the project `TArray` + `ReplicatedUsing` pattern. There is no FastArray.

## Authority API

`RequestDeformation(const FGP_TerrainDeformationRequest&)`.

Authority validates, assigns `SequenceId`, applies the profile locally, appends the log, then broadcasts `OnTerrainDeformationApplied`. A client or simulated proxy returns `NoAuthority` and does not edit. Gameplay code does not call `UVoxelSphereTools`.

## Event fields

`FGP_TerrainDeformationRequest` / `FGP_TerrainDeformationEvent`:

- `SequenceId` (authority, starts at 1; 0 means none)
- `WorldLocation` (surface impact, not the sphere center)
- `RadiusCm`
- `DepthCm`
- `Profile` (`EGP_TerrainDeformationProfile::ShallowSphereCap` only)
- `Seed`
- `RotationDegrees`
- `SurfaceScar` (`EGP_TerrainSurfaceScarType::None` only)
- `SourceIdentity` (`FName`)

`FGP_TerrainDeformationResult` returns accepted/rejected, `SequenceId`, reject reason, applied radius/depth, edit center, edited bounds, and the resolved voxel actor.

Future irregular profiles must be deterministic from Profile + Seed + Rotation + radius + depth. The spherical cap ignores seed and rotation. No noise was added.

## Validation

- Non-finite location, radius, depth, or rotation: `NonFinite`
- Radius or depth ≤ 0: `InvalidRadius` / `InvalidDepth`
- Any other profile: `UnsupportedProfile`
- Finite radius is then clamped to 50..1500 cm
- Finite depth is then clamped to 10..RadiusCm, so depth greater than radius becomes a hemisphere
- No owning voxel world, or the world is not created: `NoVoxelWorld`
- `RemoveSphere` did not return finite bounds: `ApplyFailed` (sequence is not consumed)

## Voxel world resolution

Vertical trace, ±8000 cm, complex, channels `WorldStatic`, `WorldDynamic`, and `Visibility`. The closest blocking hit that `ResolveVoxelWorldFromHit` accepts is the owner. A nearer static mesh is ignored. There is no scan of every `AVoxelWorld`, and there is no single cached world. The project does not guarantee one gameplay VoxelWorld per match.

## Geometry

`ShallowSphereCap` only. Center Z = `WorldLocation.Z + (RadiusCm - DepthCm)`. The private adapter calls `RemoveSphere` with `bConvertToVoxelSpace=true`, `bUpdateRender=true`, `bMultiThreaded=false`. The adapter is the Voxel isolation seam and now compiles in every build configuration, including Shipping. Shipping itself was not built.

## Event log

Replicated `TArray<FGP_TerrainDeformationEvent>` on the component, `OnRep_EventLog` → `ApplyPendingAuthoritativeEvents`. History cap is 32. Oldest entries drop off the front. The replicated record is the compact event, not voxel density.

`ApplyPendingAuthoritativeEvents` applies a `SequenceId` only if this machine has not applied it. A failed local apply stays pending. Authority `RequestDeformation` applies once and records the id before the log can be replayed, so a listen-server `OnRep` or a later `ApplyPending` does not edit again.

Late join only receives the retained 32 events. Snapshot compaction is not implemented. A two-process client was not run.

## Debug command

`gp.Voxel.CraterUnderCursor` traces the cursor, builds a normal request (`SourceIdentity=DebugCursor`), and calls `RequestDeformation`. It does not call `RemoveSphere`. The delayed collision message still uses the adapter trace helper.

`gp.Voxel.FillUnderCursor` and `gp.Voxel.ApplyProbeCrater` remain direct adapter spike paths. The runtime crater contract still calls the adapter directly so that seam stays covered.

`OnTerrainDeformationApplied` is the hook for a future scar, debris, or vegetation consumer. This service does not reference Niagara, paint materials, or own foliage.

## Tests / build

`gp.Terrain.RunDeformationContractTest` at 2026.09.27-22.42.12: **Failures=0**.

Covered on one authority process (`L_PrototypeArena`, `-game -NullRHI`):

- A. Valid shallow crater accepted, sequence 1, event stored, bounds valid, edit center = surface + 320 cm
- B. `SetRole(ROLE_SimulatedProxy)` rejected, log unchanged, role restored
- C. NaN / infinity rejected; radius ≤ 0 and depth ≤ 0 rejected; radius 2000 and depth 9000 clamped to 1500 / 1500
- D. Local apply went through the adapter; edited bounds valid on the transient probe
- E. Sequences 1 then 2
- F. `ApplyPendingAuthoritativeEvents` twice did not apply sequence 1 or 2 again
- G. Authority apply plus `ApplyPending` broadcast once. This is the listen-server guard in a standalone authority process, not a second game process
- H. `gp.Voxel.CraterUnderCursor` is registered. The cursor click itself was not automated. The command's crater branch returns through `RequestDeformation` before any adapter edit

Regressions, all **Failures=0**:

- `gp.Voxel.RunPluginCompileProbeContractTest` 2026.09.27-22.43.28
- `gp.Voxel.RunRuntimeCraterProbeContractTest` 2026.09.27-22.43.38
- `gp.Movement.RunGroundFollowContractTest` 2026.09.27-22.43.46
- `gp.Movement.RunRTSMovementReconciliationContractTest` 2026.09.27-22.44.10
- `gp.Worker.RunCommandIntentContractTest` 2026.09.27-22.44.20
- `gp.Mining.RunContractTest` 2026.09.27-22.44.30
- `gp.Match.RunWinLoseContractTest` 2026.09.27-22.44.40

Build: **GPEditor Win64 Development succeeded** (UHT included). GP Win64 Development was not run. Shipping was not run. This checkpoint is not a Stage 3A merge candidate.

Two-process visual reconstruction was **not** run and is not claimed.

## Stage 3A still open

Proven earlier and still in place: authored deformation, collision, shallow radius/depth, terrain-follow movement.

Proven in this checkpoint: authority service, event contract, local apply seam, bounded replicated log, duplicate guard.

Still deferred:

- Irregular crater profiles
- Terrain material scars
- Niagara debris
- Vegetation reaction
- Foundation reaction
- Dynamic traversability (Stage 3E)
- Full late-join snapshot / compaction
- A real second-client visual replay

## Operator retest

1. PIE `L_VoxelArena_2P` on the authority viewport. The map references `GP_GameMode`, which spawns `AGP_GameState`.
2. Point at voxel ground and run `gp.Voxel.CraterUnderCursor 400 80`.
3. Confirm the crater appears.
4. Drive a Worker through it.
5. Confirm terrain-follow Z is still correct.
6. Two-player check, not yet automated: host runs the command; the client should not run it (the client command is refused). The client should form the same crater once from the replicated event. If the host prints `no terrain service`, that viewport has no `AGP_GameState`.

## Files changed

- `GP/Source/GPRuntime/Public/Terrain/GPTerrainDeformationTypes.h`
- `GP/Source/GPRuntime/Public/Terrain/GPTerrainDeformationComponent.h`
- `GP/Source/GPRuntime/Private/Terrain/GPTerrainDeformationComponent.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPTerrainDeformationContractTest.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPVoxelAuthoredCraterProbe.cpp`
- `GP/Source/GPRuntime/Public/Game/GPGameState.h`
- `GP/Source/GPRuntime/Private/Game/GPGameState.cpp`
- `GP/Source/GPRuntime/Private/Voxel/GPVoxelRuntimeProbeAdapter.h`
- `GP/Source/GPRuntime/Private/Voxel/GPVoxelRuntimeProbeAdapter.cpp`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not staged: `GP/Content/` (including `L_VoxelArena_2P` and `L_PrototypeArena`), `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.
