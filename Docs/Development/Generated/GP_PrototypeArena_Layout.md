# GP Prototype Arena Layout Manifest

- GeneratorVersion: 1
- MapPath: `/Game/GrimProtocol/Maps/L_PrototypeArena`
- MapType: non-World-Partition compact umap
- FloorDimensions: 4000 x 4000 uu (Engine Cube scale 40x40x1)
- NavPolicy: NavMeshBoundsVolume spawned; editor Build Paths may be required
- GameplayPopulation: none (GP-S27A2 infrastructure only)
- ExistingMapAbort: false
- GenerationTimestampUTC: 2026-08-04T15:19:31.562Z
- SourceCommit: `7508fc8eca2acc7f277fe3d9ed7965db15df5711`

| Label | Class | Location | Rotation | Scale | Tag |
| --- | --- | --- | --- | --- | --- |
| GP_Arena_Floor | StaticMeshActor | (0.0, 0.0, -50.0) | (P0.0 Y0.0 R0.0) | (40.00, 40.00, 1.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_Wall_North | StaticMeshActor | (0.0, 2050.0, 150.0) | (P0.0 Y0.0 R0.0) | (42.00, 1.00, 3.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_Wall_South | StaticMeshActor | (0.0, -2050.0, 150.0) | (P0.0 Y0.0 R0.0) | (42.00, 1.00, 3.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_Wall_East | StaticMeshActor | (2050.0, 0.0, 150.0) | (P0.0 Y0.0 R0.0) | (1.00, 42.00, 3.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_Wall_West | StaticMeshActor | (-2050.0, 0.0, 150.0) | (P0.0 Y0.0 R0.0) | (1.00, 42.00, 3.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_DirectionalLight | DirectionalLight | (0.0, 0.0, 800.0) | (P-40.0 Y35.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_SkyLight | SkyLight | (0.0, 0.0, 0.0) | (P0.0 Y0.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_SkyAtmosphere | SkyAtmosphere | (0.0, 0.0, 0.0) | (P0.0 Y0.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_PlayerStart | PlayerStart | (0.0, -1500.0, 100.0) | (P0.0 Y90.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` |
| GP_Arena_NavMeshBounds | NavMeshBoundsVolume | (0.0, 0.0, 100.0) | (P0.0 Y0.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` |

## Notes
- No AGP_Unit / AGP_ResourceNode / combat pairs in A2.
- Global GameDefaultMap / DefaultGameMode intentionally unchanged.
- World GameMode override: AGP_GameMode.
