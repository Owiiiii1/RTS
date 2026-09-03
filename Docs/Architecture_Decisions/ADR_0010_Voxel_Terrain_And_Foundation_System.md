# ADR-0010 — Voxel Terrain And Foundation System

## Status
Accepted (2026-08-21) — documentation decision. Refined 2026-08-21: Wall does not require Foundation; local engineering is Worker-labor with planned jobs. Production implementation is gated on a Voxel Plugin technical spike and remaining DESIGN REQUIRED items in GDD/13 and TDD/16.

Stage 3A local audit (2026-09-04): **Voxel Plugin Free Legacy** is installed at `GP/Plugins/VoxelFree` (Version 434 / `159fd19a0`, EngineVersion 5.8.0). UE 5.8.1 compile and load are proven. Runtime crater apply is not demonstrated; Stage 3A is not complete. See [`../Development/Voxel_Plugin_Technical_Spike.md`](../Development/Voxel_Plugin_Technical_Spike.md).

## Context

MVP terrain has been treated as an effectively immutable, planar play surface. Building deploy currently validates occupancy, nav, and (where implemented) FoW, then drops a READY orbital building onto raw ground.

The owner now requires:

- destructible / deformable terrain as a gameplay capability;
- a hard split between **completed orbital assets** and **local field engineering performed by Workers**;
- orbital foundation material installed by Workers before normal building deploy;
- foundation tracked per BuildGrid cell, with partial destruction;
- Walls constructed by Workers directly on terrain, without Foundation Slabs;
- Voxel Plugin as the intended terrain backend.

This must not revive Barracks / factory / local production of READY buildings (ADR-0009). Exact plugin API, replication, and navigation strategy are not yet proven.

## Decision

1. **Deformable terrain is now a project architectural capability.** Gameplay events may request terrain deformation; the world is no longer conceptually immutable. Future earthquakes reuse the same generic deformation / foundation-damage contract.
2. **Voxel Plugin is the intended terrain / deformation backend.** Installed locally: Voxel Plugin Free Legacy 434 / `159fd19a0` on UE 5.8.1. This ADR still does not claim plugin-native replication as the GP path (Free TCP MP is a Pro stub). Preferred reconstruction remains a server event log + local apply (`UVoxelSphereTools::RemoveSphere` on each machine). Runtime crater PIE is not yet proven.
3. **Terrain deformation is server-authoritative.** Clients reconstruct authoritative changes. Clients do not author gameplay destruction.
4. **Normal player-deployed buildings require a prepared foundation surface.** Canonical sequence: raw terrain → Worker levels → plan foundation install → Workers progressively install cells → deploy READY orbital building. Buildings remain orbital, not Worker-constructed.
5. **Foundation state is per BuildGrid cell.** A physical slab may cover multiple cells. Placement and destruction query cells, not an all-or-nothing slab actor. Foundation Repair is a future Worker engineering job.
6. **Orbital completed asset vs local engineering is canonical.** READY buildings arrive complete (no Worker construction after landing). Field engineering (leveling, foundation install/repair, Wall construction, future applicable demolish) requires physical Worker participation on a planned job. Multiple Workers accelerate progress (formula TBD).
7. **Foundation procurement follows Wall Package philosophy:** Orbital Ferronite purchase → one delivery to MainBase inventory → consume already-delivered stock during install (consume/reserve moment **DESIGN REQUIRED**) → no second Orbital spend. Quantity and cost are **not** copied from Wall Package (5) and remain data-driven / TBD.
8. **Initial MainBase** remains a match-start exception on an authored / prepared starting site. Exact starter-foundation implementation is deferred.
9. **Wall Foundation Rule — RESOLVED:** Wall segments do **not** require Foundation Slabs. They are locally assembled from delivered Wall Package stock by Workers and may be constructed directly on terrain. Terrain suitability (slope, visual adapt, auto-level vs manual, voxel base interaction) remains **DESIGN REQUIRED**. Wall-mounted Turret follows Wall and does not independently require ground Foundation.
10. **Plan first, work second.** Player may define a local engineering job before Workers are present. Progress starts only when an assigned Worker reaches a valid work position. Exact job/site/blueprint class names are implementation decisions.
11. **Reusable Worker work-presentation hooks:** gameplay emits work-pulse start/end; Blueprint owns authored Niagara. No hardcoded project Niagara asset in Worker native gameplay. Mining presentation is the reference pattern.
12. **Exact plugin API, network strategy, and dynamic navigation strategy are deferred to spike / design.** Surviving-building-after-foundation-loss remains **DESIGN REQUIRED**.

## Consequences

### Positive

- Construction sites become a readable engineering fantasy without Barracks / local READY-building queues.
- Combat, missed projectiles, and later earthquakes can leave lasting world scars and maintenance work.
- Partial foundation destruction supports mixed footprints and shared slab areas.
- BuildGrid stays the discrete planning layer; voxels stay continuous geometry.
- Wall implementation no longer waits on a foundation-yes/no decision.

### Negative / risks

- Voxel Plugin integration, replication, and performance are unproven.
- Placement, nav, and world FoW must later follow a non-planar surface.
- Local engineering jobs add assignment, multi-Worker, and presentation-hook work before Walls.
- Remaining DESIGN REQUIRED items: zone UX, tolerances, slab balance, Wall slope, stock consume moment, support-loss rule.

### Hard rules

- Do not add Barracks / factory / `UGP_ConstructionComponent` as a READY-building production path.
- Do not tie terrain destruction to a single weapon class.
- Do not treat a slab as one all-or-nothing destructible.
- Do not require Foundation under Wall segments.
- Do not silently decide surviving-building-after-support-loss.
- Do not hardcode a project Niagara asset into Worker native gameplay for engineering pulses.

## Alternatives Considered

| Alternative | Why rejected |
| --- | --- |
| Keep terrain immutable | Conflicts with owner direction for craters, missed shells, and site prep. |
| Worker constructs the READY building locally | Violates ADR-0009 orbital pillar. |
| Instant foundation / Wall spawn on click | Conflicts with physical Worker labor and plan-first jobs. |
| Walls require Foundation Slabs | Rejected: Wall segments are field-assembled material, not completed orbital drops. |
| Heightfield-only deformation | Insufficient for the intended voxel destruction / leveling fantasy; Voxel Plugin is the selected backend. |
| One actor per slab, destroy all-or-nothing | Cannot support shared cells, multi-slab footprints, or partial blast damage. |
| Claim a specific Voxel Plugin replication API now | Unverified; must wait for the spike. |
| Separate earthquake terrain system | Rejected; earthquakes must reuse the generic deformation / foundation-damage contract. |

## References

- GDD — [`../GDD/13_Terrain_Engineering_And_Foundations.md`](../GDD/13_Terrain_Engineering_And_Foundations.md)
- TDD — [`../TDD/16_Voxel_Terrain_And_Foundations.md`](../TDD/16_Voxel_Terrain_And_Foundations.md)
- ADR-0009 — [`ADR_0009_Orbital_Delivery_Pillar.md`](ADR_0009_Orbital_Delivery_Pillar.md)
- ADR-0004 — [`ADR_0004_Multiplayer_First.md`](ADR_0004_Multiplayer_First.md)
