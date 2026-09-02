# Voxel Terrain And Foundations

> Technical direction for deformable voxel terrain, Worker leveling, per-cell foundation coverage, and generic local engineering jobs.
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
- Local engineering **progress** is server-authoritative. Presentation pulses do not own progress.

Exact multiplayer synchronization strategy must be proven in a technical spike before production implementation.

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

| Field (conceptual) | Notes |
| --- | --- |
| World location | Impact / explosion / later earthquake origin |
| Radius | Data-driven; exact values **TBD** |
| Depth / strength | Data-driven; exact formula **TBD** |
| Shape | Data-driven; exact catalog **TBD** |
| Source identity | Optional for diagnostics; not a hard coupling to one weapon class |

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

SWARM Medium/Large corpses as temporary obstacles (see [`17_SWARM_Architecture`](17_SWARM_Architecture.md)) must **not** mandate a runtime NavMesh rebuild. Preferred direction: transient obstacle data / traversability layer, or local check + repath, aligned with this voxel terrain / traversability work. Concrete **navigation/obstacle** approach remains prototype / profile TBD. This is not a Mass / gameplay-backend choice.

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
- Many concurrent engineering jobs / work pulses.

Budgets and strategies are **TECH-SPIKE REQUIRED**. Do not invent numbers here.

## Explicit Unresolved Decisions

- Leveling zone sizing UX (fixed vs drag).
- Target elevation algorithm, slope tolerance, leveling speed, Worker pathing, interrupt/resume.
- Max Workers / scaling / contribution / assignment / work-position reservation.
- Foundation package cost, quantity, slab footprint, stock consume/reserve moment.
- Foundation Repair tunables.
- Blast radius / depth / damage formula.
- Voxel Plugin version / API.
- Voxel replication mechanism.
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
