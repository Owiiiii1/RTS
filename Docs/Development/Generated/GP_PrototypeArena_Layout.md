# GP Prototype Arena Layout Manifest

- GeneratorVersion: 1
- MapPath: `/Game/GrimProtocol/Maps/L_PrototypeArena`
- MapType: non-World-Partition compact umap
- FloorDimensions: 4000 x 4000 uu (Engine Cube scale 40x40x1)
- NavPolicy: UActorFactory::CreateBrushForVolumeActor + CubeBuilder 4500x4500x500; EditorBuild AIPaths; pre-save bounds validation
- GameplayPopulation: none (GP-S27A2 infrastructure only)
- ExistingMapAbort: false
- RecastNavMeshCount: 1
- NavDataCount: 2
- NavigationBuildSucceeded: true
- GenerationTimestampUTC: 2026-08-04T15:40:19.859Z
- SourceCommit: `bf98e85a69971767cf44b990ac54701d3da46d1e`

| Label | Class | Location | Rotation | Scale | Tag | Extra |
| --- | --- | --- | --- | --- | --- | --- |
| GP_Arena_Floor | StaticMeshActor | (0.0, 0.0, -50.0) | (P0.0 Y0.0 R0.0) | (40.00, 40.00, 1.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_Wall_North | StaticMeshActor | (0.0, 2050.0, 150.0) | (P0.0 Y0.0 R0.0) | (42.00, 1.00, 3.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_Wall_South | StaticMeshActor | (0.0, -2050.0, 150.0) | (P0.0 Y0.0 R0.0) | (42.00, 1.00, 3.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_Wall_East | StaticMeshActor | (2050.0, 0.0, 150.0) | (P0.0 Y0.0 R0.0) | (1.00, 42.00, 3.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_Wall_West | StaticMeshActor | (-2050.0, 0.0, 150.0) | (P0.0 Y0.0 R0.0) | (1.00, 42.00, 3.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_DirectionalLight | DirectionalLight | (0.0, 0.0, 800.0) | (P-40.0 Y35.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_SkyLight | SkyLight | (0.0, 0.0, 0.0) | (P0.0 Y0.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_SkyAtmosphere | SkyAtmosphere | (0.0, 0.0, 0.0) | (P0.0 Y0.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_PlayerStart | PlayerStart | (0.0, -1500.0, 100.0) | (P0.0 Y90.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` | - |
| GP_Arena_NavMeshBounds | NavMeshBoundsVolume | (0.0, 0.0, 100.0) | (P0.0 Y0.0 R0.0) | (1.00, 1.00, 1.00) | `GP.GeneratedPrototypeArena` | BrushExtent=(2250.0,2250.0,250.0) SphereRadius=3191.8 |

## Notes
- No AGP_Unit / AGP_ResourceNode / combat pairs in A2.
- Global GameDefaultMap / DefaultGameMode intentionally unchanged.
- World GameMode override: AGP_GameMode.
