# Voxel Terrain And Foundations

> Technical direction for deformable voxel terrain, Worker leveling, per-cell foundation coverage, and generic local engineering jobs.
> Gameplay WHAT: [`../GDD/13_Terrain_Engineering_And_Foundations.md`](../GDD/13_Terrain_Engineering_And_Foundations.md).
> Decision: [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md).
>
> This page is **not** a full implementation spec. `UGP_TerrainDeformationComponent` is a real class.
> Other sketched owner names are not classes.
> Stage 3A local audit (2026-09-04): **Voxel Plugin Free Legacy** is installed locally at
> `GP/Plugins/VoxelFree` (Version 434 / `159fd19a0`, EngineVersion 5.8.0). UE 5.8.1 compile, load,
> and runtime `RemoveSphere` crater (mesh + collision) are proven on a transient C++ `UVoxelFlatGenerator`
> world. Operator visual check on authored `L_VoxelArena_2P` (2026-09-27) confirmed deformation and
> collision, and a shallow radius/depth crater. Units descend and climb that deformed surface.
> The spherical cap is the implemented ShallowSphereCap profile. Further profiles, material scars,
> and presentation debris are not implemented. `UGP_TerrainDeformationComponent` on `AGP_GameState`
> is the authority service: it validates a generic request, applies that cap locally, and replicates
> a bounded event log. Unit XY still comes from the nav/straight path; actor Z follows the physical
> surface. Dynamic traversability and NavMesh rebuild stay Stage 3E.
> Stage 3A deformation foundation is complete on UE 5.8.3. Two-player PIE Listen Server
> (2026-09-28) showed the host crater, the client reconstructing it from the replicated
> event, no duplicate application, and Worker terrain-follow still correct. The 32-event
> log is not a late-join snapshot.
> See [`../Development/Voxel_Plugin_Technical_Spike.md`](../Development/Voxel_Plugin_Technical_Spike.md).

## Intended Backend

**Voxel Plugin** is the intended terrain / deformation backend.

Stage 3A (2026-09-04) installed **Voxel Plugin Free Legacy** locally and proved UE 5.8.1 compile/load.
Plugin-native TCP multiplayer is **Pro-only** (Free stubs return false). This documentation still
does not claim plugin replication as the GP path. Preferred reconstruction remains a server-authored
compact deformation event log with local apply (Option B). Option A is unavailable on Free. Option C
(chunk deltas) is not required for crater events. Runtime `RemoveSphere` crater is proven on a
transient probe world (density, local `EditedBounds` 13³ vs 64³, mesh update, collision deepen).
`UGP_TerrainDeformationComponent` (default subobject of `AGP_GameState`) is the production authority
service. Only `ShallowSphereCap` is implemented. `AGP_CameraPawn` owns one `UVoxelSimpleInvokerComponent`
(`LODRange`/`CollisionsRange` 20000 cm, navmesh off). The plugin registers every invoker and only
gates LOD with `IsLocalInvoker()`; GP enables the component only while the pawn is locally controlled.
Authored maps should keep VoxelWorld camera-invoker fallback off. Stage 3A deformation foundation is complete.
The voxel-map minimap looking 90° off the 3D view was inherited PlayerStart yaw on `AGP_CameraPawn`, not the minimap XY transform. Startup yaw is `CameraConfig.DefaultYaw` (90°).

Editor Build Paths flag `0x20` is `ENavigationBuildLock::AsyncLoadLock` (`1 << 5`) in UE 5.8.3
`NavigationSystem.h`. `UNavigationSystemV1::DoInitialSetup` adds it in editor mode while
`bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically` is true (engine default). A ticker removes
it after asset compilation is idle, 16 frames, and 2 seconds, with `NoRebuild`. It is not Initial
Building Locked and not a Voxel or GP GameMode lock. `UVoxelProceduralMeshComponent` exports Recast
triangles only for sections with `bEnableNavmesh`. `AVoxelWorld::bEnableNavmesh` defaults false, so
collision alone does not feed Recast. Camera `bUseForNavmesh` stays false; visible-chunk navmesh
(`bComputeVisibleChunksNavmesh`, default true) is the static baseline once the world flag is on.
Dynamic crater nav remains Stage 3E.

## Authority Boundary

- Terrain deformation is **server-authoritative**.
- Clients receive / reconstruct authoritative terrain changes.
- Clients do **not** author gameplay terrain destruction.
- Local engineering **progress** is server-authoritative. Presentation pulses do not own progress.

Production reconstruction is a replicated compact event log on `UGP_TerrainDeformationComponent`
(last 32 accepted events). That cap is an intermediate buffer. It is not a late-join solution
and it is not guaranteed once a match has more than 32 historical deformations. Authority applies
once, then appends. Each machine applies a SequenceId at most once. Clients cannot author events.
Dense voxel payloads are not replicated. Two-player PIE Listen Server on `L_VoxelArena_2P`
(2026-09-28): the host `gp.Voxel.CraterUnderCursor` created the crater, the client received the
compact event and reconstructed the same deformation automatically, and no duplicate application
was observed. Worker terrain-follow stayed correct. Snapshot compaction is not implemented.
Installed Free source: TCP MP is a Pro stub; `GetSave` / `LoadFromSave` exist for a future
snapshot. Plugin multiplayer is not the GP path.

## Orbital Completed Asset vs Local Engineering

Do not mix these in APIs.

| Kind | Examples | Worker construction after delivery? |
| --- | --- | --- |
| **Orbital completed asset** | Logistics Hub, Defensive Turret, future READY buildings | **No.** DropPod lands operational. |
| **Local engineering** | Terrain Leveling, Foundation Installation, Foundation Repair, Wall Construction | **Yes.** Plan / job first; progress only while assigned Workers are in valid work positions. |

Wall **Package** and Foundation Slab **package** are orbital **material**. The field work that turns stock into Wall segments or foundation cells is local engineering.

Do not resurrect Barracks / factory / `UGP_ConstructionComponent` as a READY-building production path. A future generic **engineering job** owner is a different problem; exact class names are **TBD**.

## Generic Local Engineering Job Contract (future)

Terrain / Foundation stage should establish this contract **before** final Wall implementation.

Conceptual responsibilities (names TBD):

- persist a **planned job** after player confirm, even if no Worker is assigned yet;
- accept Worker assignment / unassignment;
- start authoritative progress only when ≥1 assigned Worker is in a valid work position;
- 0 active Workers → 0 progress; 1 → baseline; multiple → faster (scaling formula **TBD**);
- reserve distinct work positions so Workers do not occupy the same point;
- complete → spawn / mark operational results (foundation cells, Wall segments, leveled cells);
- emit work-presentation start / end pulses without owning progress.

Exact runtime representation of job / site / blueprint is an **implementation design decision**.

**DATA-DRIVEN / DESIGN REQUIRED:** max Workers per job, linear vs diminishing returns, contribution rate, assignment / cancel, work-position reservation.

## Worker Work-Presentation Hooks (future)

Follow the mining presentation philosophy:

- gameplay owns work state;
- Blueprint owns authored Niagara / sound / animation;
- owner attaches Niagara on the Worker Blueprint;
- **no hardcoded project Niagara asset** in Worker native gameplay.

Conceptual events (names **not** canonical): WORK PULSE START → ~1 s presentation target → WORK PULSE END.

Mining’s authored Mining-only Niagara hook is the **reference pattern**, not an API to copy verbatim.

## Generic Deformation-Event Contract

Gameplay systems must not call “destroy terrain as this tank gun.” They emit a **generic deformation request**:

| Field | Runtime |
| --- | --- |
| SequenceId | Assigned by authority, starting at 1. Clients do not allocate ids |
| WorldLocation | Surface impact. Not the sphere center |
| RadiusCm | Finite and > 0, then clamped to 50..1500 cm |
| DepthCm | Finite and > 0, then clamped to 10..RadiusCm. Depth > Radius becomes a hemisphere |
| Profile | `EGP_TerrainDeformationProfile`. Only `ShallowSphereCap` is accepted |
| Seed | Stored. Ignored by the current cap. Future irregular profiles must be deterministic from Profile + Seed + Rotation + geometry |
| RotationDegrees | Stored. The spherical cap does not change when rotated |
| SurfaceScar | `EGP_TerrainSurfaceScarType::None` only. Not painted |
| SourceIdentity | Diagnostic `FName`. Not a weapon class |

Implemented types: `FGP_TerrainDeformationRequest`, `FGP_TerrainDeformationEvent`, `UGP_TerrainDeformationComponent::RequestDeformation`. Non-finite inputs are rejected. Radius or depth ≤ 0 is rejected. Any other profile is rejected. The service resolves the authored `AVoxelWorld` with a vertical trace and the existing hit-ownership resolver. It does not take the first `AVoxelWorld` in the world.

`ShallowSphereCap` places the sphere center at `WorldLocation.Z + (RadiusCm - DepthCm)` and calls `UVoxelSphereTools::RemoveSphere` through the private adapter (`bConvertToVoxelSpace=true`, `bUpdateRender=true`, `bMultiThreaded=false`). Gameplay callers do not include Voxel types. Seed and rotation are on the event so a future irregular profile can be rebuilt the same way on every machine. That generator is not implemented. Fully random voxel noise is not used.

`OnTerrainDeformationApplied` is the subscription point for a future scar, debris, or vegetation consumer. The terrain service does not own those.

### World-impact pipeline

Consumers stay separate. Names below are roles, not classes:

```
Impact / Explosion event
  → damage / combat reaction
  → generic TerrainDeformationRequest
  → terrain geometry deformation
  → terrain material scar
  → Foundation footprint reaction where applicable
  → vegetation / prop reaction where applicable
  → presentation event for explosion / dust / debris
```

The terrain service does not own combat damage. Niagara does not own authoritative terrain. Vegetation reaction does not own terrain deformation.

### Crater profiles, scars, debris, landforms, vegetation

`ShallowSphereCap` (`EditCenter` above the surface by `Radius - Depth`) is the only implemented profile. The remaining catalog (wide shallow, asymmetric, elongated, ragged, compound, and similar) is not implemented. Profile, seed, and rotation already travel with the event.

A deformation may also change voxel material in the footprint (dark soil, scorch, fresh rock, Ferronite variant). Prefer a surface material layer on the voxel terrain. Exact channels, fading, and persistence are **TECH / DESIGN REQUIRED**. A match-long scar is acceptable for MVP.

Niagara explosion, dust, and mesh-particle stones are presentation only. Do not replicate each small rock as a gameplay Actor. Persistent blocking debris, if added later, is a separate gameplay representation.

Hills, mountains, ridges, cliffs, embankments, and major rock masses that should take impacts are authored in the VoxelWorld. The same deformation request covers flat ground and those landforms. There is no separate mountain-destruction system. Static Mesh props that need destruction use their own replacement behavior.

Vegetation is not voxel terrain. It sits on the surface as foliage, actors, or instances and may react to the same footprint: remove or swap the intact instance, optionally a short fall, then a cheap static wreck. Permanently simulating large numbers of physics trees is out of scope. Vegetation technology, pooling, persistence, and thresholds are **DESIGN / TECH REQUIRED**.

Canonical future producers (not implemented now):

- projectile miss / terrain hit;
- explosion;
- unit death explosion;
- building death explosion;
- **earthquakes** (post-MVP; same contract — do not build a separate earthquake terrain system).

The terrain service consumes the request. Projectile implementation is deferred until projectile-based units exist.

## BuildGrid vs Voxel Terrain

Do **not** merge the two concepts.

| Owner | Role |
| --- | --- |
| **BuildGrid** | Discrete planning / occupancy / foundation grid. Asks terrain questions: height, slope / flatness, foundation coverage, support validity, Wall terrain suitability. |
| **Voxel terrain** | Continuous / deformable world geometry. Owns physical terrain shape. |

Use the existing BuildGrid as the logical planning grid for leveling and foundation unless a later technical design proves a separate grid is required. BuildGrid cell size remains the occupancy grid (currently 200 cm); FoW grid remains a separate visibility grid.

## Foundation Per-Cell State

Installed foundation is tracked **per BuildGrid cell**, not as one actor-equals-one-slab object.

Conceptual cell state (names TBD):

- not prepared;
- leveled (sufficiently flat toward the construction plane — tolerance **TBD**);
- foundation installed / intact;
- foundation damaged (support validity **TBD**);
- foundation destroyed.

A delivered physical slab / panel may mark multiple cells. Later placement queries those cells, not the original package identity.

Installation is progressive Worker labor on a planned job. Stock consume / reserve moment is **DESIGN REQUIRED**.

Partial destruction: an explosion / deformation / later earthquake footprint clears only the affected cells. Neighboring intact cells remain valid. Destroyed cells fail future placement validation.

Foundation Repair is a future local-engineering job using the same Worker / presentation contract. Repair tunables are **TBD**.

**DESIGN REQUIRED:** behavior of a still-alive building that loses some supporting foundation from an external explosion or earthquake. No option is approved.

## Leveling Service Responsibilities (future)

A future leveling / site-preparation service (exact class TBD) should:

- treat the selected rectangular BuildGrid-aligned zone as the planned job;
- query voxel terrain for per-cell height / slope vs the target construction plane (algorithm **TBD**);
- present grey / yellow cell feedback to the local placement-style overlay;
- reject planning when every cell is already grey;
- apply **progressive** terrain convergence only while assigned Workers work — not an instant flatten at job complete;
- remain server-authoritative for the actual deformation.

Do not implement a completion-only mesh swap.

## Placement-Query Contract (future)

For normal player-deployed READY buildings, server deploy validation must eventually require, for **every** footprint cell:

1. sufficiently leveled terrain (tolerance **TBD**);
2. intact installed foundation;
3. no conflicting occupancy / reservation;
4. existing placement requirements (nav / overlap / FoW as already specified).

Initial MainBase uses an authored / prepared starting site; exact starter-foundation implementation is deferred.

**Wall Foundation Rule — RESOLVED:** Wall segments do **not** require foundation cells.

Wall placement must still validate **terrain suitability**. Exact slope limit, visual adapt, auto-level vs player-level, and voxel interaction at wall bases are **TBD / DESIGN REQUIRED**.

Wall-mounted Turret follows Wall and does not independently require ground Foundation beneath the wall.

## Navigation Implications

Terrain implementation must account for:

- crater creation;
- leveling;
- height changes;
- walkability changes;
- Worker access to leveling / construction areas;
- unit navigation after deformation.

Exact NavMesh vs voxel-navigation update strategy is **DESIGN / TECH-SPIKE REQUIRED**. Do not assume a full NavMesh rebuild after every explosion is acceptable.

Current production path: `UGP_MovementComponent` uses `UNavigationSystemV1::FindPathSync` / `ProjectPointToNavigation` (Recast when `NavMeshBoundsVolume` coverage exists, else straight-line fallback) for **XY**. While moving, actor Z follows a downward `WorldStatic`/`WorldDynamic` trace, ignoring `AGP_UnitBase` hits (units and buildings), offset by the root capsule half-height, smoothed at `GroundVerticalSpeedCmPerSec`. A missed trace keeps the current Z. Path point Z is still the owner Z and is not the physical height. Slope rejection, unit-class walkability, and dynamic crater NavMesh remain Stage 3E.

SWARM Medium/Large corpses as temporary obstacles (see [`17_SWARM_Architecture`](17_SWARM_Architecture.md)) must **not** mandate a runtime NavMesh rebuild. Preferred direction: transient obstacle data / traversability layer, or local check + repath, aligned with this voxel terrain / traversability work. Concrete **navigation/obstacle** approach remains prototype / profile TBD. This is not a Mass / gameplay-backend choice.

## Multiplayer Tech-Spike Requirements

Before production implementation, a spike must prove:

- Voxel Plugin version / edition / licensing / UE 5.8 compatibility — **Voxel Plugin Free Legacy 434 / `159fd19a0`**, EngineVersion 5.8.0, binaries BuildId `55116800` on UE 5.8.1; compile+load proven 2026-09-04. Plugin remains operator-local / untracked (Marketplace license; do not vendor yet);
- server-authoritative deformation apply path — **implemented** as `UGP_TerrainDeformationComponent::RequestDeformation`. `ShallowSphereCap` uses `RemoveSphere` through the adapter. Probe density/mesh/collision remain proven;
- client reconstruction — replicated compact log (max 32) plus local apply, duplicate SequenceId skipped. Plugin TCP is not used. Two-player PIE Listen Server visual replay **passed** 2026-09-28. Late join and snapshot compaction were **not** tested;
- bandwidth / determinism / listen-server host+client behavior — listen-server host created the crater and the client reconstructed it once. Bandwidth was **not measured**. A client joining after more than 32 events is **not** covered;
- interaction with existing BuildGrid occupancy — occupancy stays independent; query/leveling adapter later;
- failure modes (desync, late join) — late join not implemented; save/load APIs exist; snapshot vs replay unproven in play.

## FoW Terrain-Surface Integration Requirement

Current world FoW presentation (`PerCellBlurredQuadRenderer`) was finalized against effectively planar terrain and a fixed ground-projection assumption.

**Required later (Terrain / Voxel stage, not now):** adapt world FoW presentation to the actual deformed terrain surface.

Do **not** reopen FoW implementation in this documentation slice.

FoW gameplay visibility grid remains conceptually independent from terrain rendering unless a later design explicitly adds terrain occlusion.

## Performance Risks

- Frequent voxel edits from combat + Worker leveling.
- Naive full NavMesh rebuilds.
- Replicating dense voxel deltas to clients.
- FoW overlay sampling a non-planar surface every view frame.
- Large foundation/leveling queries over big BuildGrid rectangles.
- Many concurrent engineering jobs / work pulses.

Budgets and strategies are **TECH-SPIKE REQUIRED**. Do not invent numbers here.

## Explicit Unresolved Decisions

- Leveling zone sizing UX (fixed vs drag).
- Target elevation algorithm, slope tolerance, leveling speed, Worker pathing, interrupt/resume.
- Max Workers / scaling / contribution / assignment / work-position reservation.
- Foundation package cost, quantity, slab footprint, stock consume/reserve moment.
- Foundation Repair tunables.
- Blast radius / depth / damage formula.
- Crater profile catalog beyond `ShallowSphereCap`. Seed and rotation are stored; the irregular generator is not implemented.
- Voxel material channel layout, scar types, fading, and persistence past the match.
- Debris pool, lifetime, and budget; boundary between Niagara fragments and gameplay debris.
- Vegetation technology, pooling, persistence, and reaction thresholds.
- Authored VoxelWorld coverage for every landform that must deform. Static Mesh destruction stays separate.
- Voxel Plugin version / API (Free Legacy 434; `ShallowSphereCap` is the production geometry; other profiles are not).
- Full late-join snapshot. The replicated log is a 32-event window, not a compacted match snapshot.
- Dynamic navigation strategy.
- Surviving building after foundation loss.
- Wall slope / visual adapt / auto-level / voxel base interaction; Wall stock consume moment.
- Starter-foundation implementation for initial MainBase.
- Earthquake parameters (post-MVP).

**Resolved:** Walls do not require Foundation.

## References

- GDD — [`../GDD/13_Terrain_Engineering_And_Foundations`](../GDD/13_Terrain_Engineering_And_Foundations.md)
- BuildGrid — [`06_Building_Architecture`](06_Building_Architecture.md)
- Orbital delivery — [`14_Orbital_Delivery`](14_Orbital_Delivery.md)
- Commands — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md)
- Fog of War — [`15_Fog_of_War`](15_Fog_of_War.md)
- ADR-0010 — [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md)
- ADR-0009 — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)
- ADR-0004 — [`../Architecture_Decisions/ADR_0004_Multiplayer_First`](../Architecture_Decisions/ADR_0004_Multiplayer_First.md)
