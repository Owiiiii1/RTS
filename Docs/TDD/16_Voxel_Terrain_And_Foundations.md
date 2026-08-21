# Voxel Terrain And Foundations

> Technical direction for deformable voxel terrain, Worker leveling, and per-cell foundation coverage.
> Gameplay WHAT: [`../GDD/13_Terrain_Engineering_And_Foundations.md`](../GDD/13_Terrain_Engineering_And_Foundations.md).
> Decision: [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md).
>
> This page is **not** an implementation spec. Do not treat sketched owner names as existing classes. Exact Voxel Plugin version, edition, and API are **not decided** here and must be validated in a later technical spike.

## Intended Backend

**Voxel Plugin** is the intended terrain / deformation backend.

Exact plugin version / edition / API integration is **TECH-SPIKE REQUIRED**. This documentation must not claim a specific Voxel Plugin replication API unless already verified.

## Authority Boundary

- Terrain deformation is **server-authoritative**.
- Clients receive / reconstruct authoritative terrain changes.
- Clients do **not** author gameplay terrain destruction.

Exact multiplayer synchronization strategy must be proven in a technical spike before production implementation.

## Generic Deformation-Event Contract

Gameplay systems must not call “destroy terrain as this tank gun.” They emit a **generic deformation request**:

| Field (conceptual) | Notes |
| --- | --- |
| World location | Impact / explosion origin |
| Radius | Data-driven; exact values **TBD** |
| Depth / strength | Data-driven; exact formula **TBD** |
| Shape | Data-driven; exact catalog **TBD** |
| Source identity | Optional for diagnostics; not a hard coupling to one weapon class |

Canonical future producers (not implemented now):

- projectile miss / terrain hit;
- explosion;
- unit death explosion;
- building death explosion.

The terrain service consumes the request. Projectile implementation is deferred until projectile-based units exist.

## BuildGrid vs Voxel Terrain

Do **not** merge the two concepts.

| Owner | Role |
| --- | --- |
| **BuildGrid** | Discrete planning / occupancy / foundation grid. Asks terrain questions: height, slope / flatness, foundation coverage, support validity. |
| **Voxel terrain** | Continuous / deformable world geometry. Owns physical terrain shape. |

Use the existing BuildGrid as the logical planning grid for leveling and foundation unless a later technical design proves a separate grid is required. BuildGrid cell size remains the occupancy grid (currently 200 cm); FoW grid remains a separate visibility grid.

## Foundation Per-Cell State

Installed foundation is tracked **per BuildGrid cell**, not as one actor-equals-one-slab object.

Conceptual cell state (names TBD):

- not prepared;
- leveled (sufficiently flat toward the construction plane — tolerance **TBD**);
- foundation installed / intact;
- foundation destroyed.

A delivered physical slab / panel may mark multiple cells. Later placement queries those cells, not the original package identity.

Partial destruction: an explosion / deformation footprint clears only the affected cells. Neighboring intact cells remain valid. Destroyed cells fail future placement validation.

**DESIGN REQUIRED:** behavior of a still-alive building that loses some supporting foundation from an external explosion. No option is approved.

## Leveling Service Responsibilities (future)

A future leveling / site-preparation service (exact class TBD) should:

- accept a rectangular BuildGrid-aligned zone from a Worker Level Terrain command (command/tag names TBD);
- query voxel terrain for per-cell height / slope vs the target construction plane (algorithm **TBD**);
- present grey / yellow cell feedback to the local placement-style overlay;
- reject start when every cell is already grey;
- after confirm, keep the Worker visibly working inside the zone with coverage waypoints (pattern **TBD**);
- apply **progressive** terrain convergence — not an instant flatten at job complete;
- remain server-authoritative for the actual deformation.

Do not implement a completion-only mesh swap.

## Placement-Query Contract (future)

For normal player-deployed READY buildings, server deploy validation must eventually require, for **every** footprint cell:

1. sufficiently leveled terrain (tolerance **TBD**);
2. intact installed foundation;
3. no conflicting occupancy / reservation;
4. existing placement requirements (nav / overlap / FoW as already specified).

Initial MainBase uses an authored / prepared starting site; exact starter-foundation implementation is deferred.

**Wall / foundation interaction** is **DESIGN REQUIRED** at the Building System design gate. Do not assume Walls use the same rule.

## Navigation Implications

Terrain implementation must account for:

- crater creation;
- leveling;
- height changes;
- walkability changes;
- Worker access to leveling areas;
- unit navigation after deformation.

Exact NavMesh vs voxel-navigation update strategy is **DESIGN / TECH-SPIKE REQUIRED**. Do not assume a full NavMesh rebuild after every explosion is acceptable.

## Multiplayer Tech-Spike Requirements

Before production implementation, a spike must prove:

- Voxel Plugin version / edition / licensing / UE 5.8 compatibility;
- server-authoritative deformation apply path;
- client reconstruction / replication approach **without claiming an unverified plugin API**;
- bandwidth / determinism / listen-server host+client behavior;
- interaction with existing BuildGrid occupancy;
- failure modes (desync, late join — if in spike scope).

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

Budgets and strategies are **TECH-SPIKE REQUIRED**. Do not invent numbers here.

## Explicit Unresolved Decisions

- Leveling zone sizing UX (fixed vs drag).
- Target elevation algorithm, slope tolerance, leveling speed, Worker pathing, interrupt/resume.
- Foundation package cost, quantity, slab footprint.
- Blast radius / depth / damage formula.
- Voxel Plugin version / API.
- Voxel replication mechanism.
- Dynamic navigation strategy.
- Surviving building after foundation loss.
- Whether Walls require foundation.
- Starter-foundation implementation for initial MainBase.

## References

- GDD — [`../GDD/13_Terrain_Engineering_And_Foundations`](../GDD/13_Terrain_Engineering_And_Foundations.md)
- BuildGrid — [`06_Building_Architecture`](06_Building_Architecture.md)
- Orbital delivery — [`14_Orbital_Delivery`](14_Orbital_Delivery.md)
- Commands — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md)
- Fog of War — [`15_Fog_of_War`](15_Fog_of_War.md)
- ADR-0010 — [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md)
- ADR-0009 — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)
- ADR-0004 — [`../Architecture_Decisions/ADR_0004_Multiplayer_First`](../Architecture_Decisions/ADR_0004_Multiplayer_First.md)
