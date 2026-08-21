# ADR-0010 — Voxel Terrain And Foundation System

## Status
Accepted (2026-08-21) — documentation decision. Production implementation is gated on a Voxel Plugin technical spike and remaining DESIGN REQUIRED items in GDD/13 and TDD/16.

## Context

MVP terrain has been treated as an effectively immutable, planar play surface. Building deploy currently validates occupancy, nav, and (where implemented) FoW, then drops a READY orbital building onto raw ground.

The owner now requires:

- destructible / deformable terrain as a gameplay capability;
- Worker site preparation (leveling) without Worker-constructed buildings;
- orbital foundation material installed before normal building deploy;
- foundation tracked per BuildGrid cell, with partial destruction;
- Voxel Plugin as the intended terrain backend.

This must not revive local building production (ADR-0009). Exact plugin API, replication, and navigation strategy are not yet proven.

## Decision

1. **Deformable terrain is now a project architectural capability.** Gameplay events may request terrain deformation; the world is no longer conceptually immutable.
2. **Voxel Plugin is the intended terrain / deformation backend.** Exact version, edition, and API are **deferred to a technical spike**. This ADR does not claim a specific Voxel Plugin replication API.
3. **Terrain deformation is server-authoritative.** Clients reconstruct authoritative changes. Clients do not author gameplay destruction.
4. **Normal player-deployed buildings require a prepared foundation surface.** Canonical sequence: raw terrain → level terrain → install foundation coverage → deploy READY orbital building. Buildings remain orbital, not Worker-constructed.
5. **Foundation state is per BuildGrid cell.** A physical slab may cover multiple cells. Placement and destruction query cells, not an all-or-nothing slab actor.
6. **Worker engineering is site preparation, not construction.** Worker may level terrain and install already-delivered foundation stock. Worker does not manufacture the building.
7. **Foundation procurement follows Wall Package philosophy:** Orbital Ferronite purchase → one delivery to MainBase inventory → consume stock on install → no second Orbital spend. Quantity and cost are **not** copied from Wall Package (5) and remain data-driven / TBD.
8. **Initial MainBase** remains a match-start exception on an authored / prepared starting site. Exact starter-foundation implementation is deferred.
9. **Exact plugin API, network strategy, and dynamic navigation strategy are deferred to spike / design.** Wall/foundation interaction and surviving-building-after-foundation-loss are **DESIGN REQUIRED**.

## Consequences

### Positive

- Construction sites become a readable engineering fantasy without Barracks / local queues.
- Combat and missed projectiles can leave lasting world scars.
- Partial foundation destruction supports mixed footprints and shared slab areas.
- BuildGrid stays the discrete planning layer; voxels stay continuous geometry.

### Negative / risks

- Voxel Plugin integration, replication, and performance are unproven.
- Placement, nav, and world FoW must later follow a non-planar surface.
- More DESIGN REQUIRED items before implementation (zone UX, tolerances, slab balance, wall rule).

### Hard rules

- Do not add `UGP_ConstructionComponent` / local building production to satisfy foundations.
- Do not tie terrain destruction to a single weapon class.
- Do not treat a slab as one all-or-nothing destructible.
- Do not silently decide Wall/foundation or surviving-building-after-support-loss.

## Alternatives Considered

| Alternative | Why rejected |
| --- | --- |
| Keep terrain immutable | Conflicts with owner direction for craters, missed shells, and site prep. |
| Worker constructs the building locally | Violates ADR-0009 orbital pillar. |
| Heightfield-only deformation | Insufficient for the intended voxel destruction / leveling fantasy; Voxel Plugin is the selected backend. |
| One actor per slab, destroy all-or-nothing | Cannot support shared cells, multi-slab footprints, or partial blast damage. |
| Claim a specific Voxel Plugin replication API now | Unverified; must wait for the spike. |

## References

- GDD — [`../GDD/13_Terrain_Engineering_And_Foundations.md`](../GDD/13_Terrain_Engineering_And_Foundations.md)
- TDD — [`../TDD/16_Voxel_Terrain_And_Foundations.md`](../TDD/16_Voxel_Terrain_And_Foundations.md)
- ADR-0009 — [`ADR_0009_Orbital_Delivery_Pillar.md`](ADR_0009_Orbital_Delivery_Pillar.md)
- ADR-0004 — [`ADR_0004_Multiplayer_First.md`](ADR_0004_Multiplayer_First.md)
