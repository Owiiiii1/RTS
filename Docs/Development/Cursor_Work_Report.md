# Cursor Work Report

## Status

**TERRAIN_DESTRUCTION_PRESENTATION_AND_ENVIRONMENT_RULES_DOCUMENTED**

**INTERMEDIATE / NOT MERGE READY**

Docs only. No gameplay or source implementation. Stage 3A is not complete. Dynamic traversability remains Stage 3E.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Parent | `2ae1e27ca1101bff812f3ea7c1ab27a4a7e188a4` |
| Checkpoint commit | `29919a0c121e1b8752dd992de3b82216a22cb5c8` |
| Engine | UE 5.8.3 |

## Canonical decisions

Operator PIE on `L_VoxelArena_2P` (2026-09-27) passed authored VoxelWorld deformation, collision update, the shallow radius/depth crater, and units descending and climbing the deformed surface.

The perfect sphere / spherical cap is the current technical probe. Production impacts use a small data-driven catalog of about 3–5 crater profiles (wide shallow, asymmetric, elongated, ragged, compound, and similar). Fully random voxel noise is not required. The authority stores profile, seed, and rotation on the deformation event so every client reconstructs the same result.

A fresh deformation may change voxel material in the footprint (dark soil, scorch, fresh rock, Ferronite variant). The scar belongs on the physical surface. A match-long scar is acceptable for MVP.

Niagara explosion, dust, and mesh-particle stones are presentation only. Small rocks are not replicated gameplay Actors. Blocking debris, if added later, is a separate gameplay representation.

Hills, mountains, ridges, cliffs, embankments, and major rock masses that should take impacts are authored in the VoxelWorld. The same generic deformation request covers flat ground and those landforms. There is no separate mountain-destruction system. Static Mesh props that need destruction use their own behavior.

Vegetation is not voxel terrain. It sits on the surface and may react to the same footprint: remove or swap, optional short fall, then cheap static wreck. Permanently simulating large numbers of physics trees is out of scope.

The world-impact pipeline keeps consumers separate: combat damage, terrain geometry, terrain material scar, foundation cells, vegetation, then presentation. Terrain does not own damage. Niagara does not own terrain. Vegetation does not own deformation.

Conceptual event fields, not an implemented API: WorldLocation, Radius, Depth/Strength, Shape/ProfileId, Seed, Rotation, SurfaceScarType, SourceIdentity.

## Unresolved

- Profile catalog contents, seed/rotation scheme, and the voxel generation algorithm.
- Material channels, scar types, fading, and persistence past the match.
- Debris pool, lifetime, budget, and the boundary with gameplay debris.
- Vegetation technology, pooling, persistence, and strength thresholds.
- Which authored landforms are inside the VoxelWorld versus decorative Static Meshes.
- Production deformation service and multiplayer event log. Dynamic traversability stays Stage 3E.

## Docs changed

- `Docs/GDD/13_Terrain_Engineering_And_Foundations.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`
- `Docs/Development/Voxel_Plugin_Technical_Spike.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected audit

Not modified and not committed: `GP/Source/`, `GP/Content/`, `GP/Config/`, `GP/GP.uproject`, `GP/Plugins/`, `Tools/`.
