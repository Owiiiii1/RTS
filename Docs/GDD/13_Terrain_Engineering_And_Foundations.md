# Terrain Engineering And Foundations

> **Canonical WHAT document** for destructible voxel terrain, Worker site preparation, and foundation slabs.
> Engineering direction: [`../TDD/16_Voxel_Terrain_And_Foundations.md`](../TDD/16_Voxel_Terrain_And_Foundations.md).
> Decision record: [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md).
>
> This page does **not** implement Voxel Plugin, commands, or classes. Exact names, tunables, and APIs are **TBD / DESIGN REQUIRED** unless stated otherwise.

## Fantasy

The planet is not a static board. Explosions, missed shells, and collapsing assets tear the ground. Players do not construct buildings on the planet — they **prepare a site** and then call a READY orbital building down onto an engineered foundation.

Worker remains an engineer, not a factory:

- Worker does **not** manufacture Logistics Hubs, Turrets, or any READY building.
- Worker **does** level terrain and install already-delivered foundation material.
- Buildings still originate from orbit, arrive complete, and are immediately operational after DropPod delivery.
- There is still no Barracks / factory / local building-production queue.

Orbital building philosophy from [ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md) is unchanged. This system adds a **prepared construction surface** between raw terrain and deploy.

## Canonical Building Sequence

For normal player-deployed buildings:

```
raw terrain
  → level terrain (Worker site preparation)
  → install foundation slab coverage (consume delivered stock; track per BuildGrid cell)
  → deploy READY orbital building (DropPod; immediately operational)
```

Initial MainBase is the match-start exception (see below).

## Destructible Terrain

Terrain is no longer conceptually immutable. Gameplay events may deform / destroy terrain.

Canonical **future** deformation sources (not implemented by this documentation slice):

1. Projectile impacts that **miss** their intended unit/building target (later: tank shell, artillery, other physical projectile weapons).
2. Explosions.
3. Unit destruction explosions.
4. Building destruction explosions.

Exact projectile gameplay is deferred until projectile-based units exist. Terrain destruction must **not** be wired to one weapon class.

The design contract is generic:

- a gameplay event may request terrain deformation at a world location;
- radius / depth / strength / shape are **data-driven**;
- exact crater radius, depth, and damage formula are **TBD**.

## Worker Terrain Leveling

Worker gains an engineering role **in addition to** mining / transport / repair:

**terrain leveling / site preparation.**

This is not construction of the building itself.

### Command concept

Player initiates a **Level Terrain** / site-preparation mode with a Worker.

Exact command / tag / class names are **implementation decisions**. Documented now as a gameplay command concept only. Do not treat any `GP.Command.*` identifier on this page as an existing registered tag.

### Leveling UX

- Cursor uses a grid similar to the existing building-placement grid.
- Use the **existing BuildGrid** as the logical planning grid unless a later technical design proves a separate grid is required.
- The selected area is a **rectangular BuildGrid-aligned leveling zone**.
- Exact UX for sizing the zone (fixed footprint vs drag rectangle) is **DESIGN REQUIRED** and must not be invented here.

For every cell under the selected leveling region:

| Visual | Meaning |
| --- | --- |
| **GREY** | sufficiently level / no work required |
| **YELLOW** | terrain is not sufficiently level and needs leveling |

Rules:

- if **at least one** cell is yellow, the player may start terrain leveling;
- if **all** cells are grey, there is nothing to level.

Additional invalid / blocked visual states may be added later. Grey / yellow are the required MVP concept.

### Leveling execution

After confirmation:

- Worker moves into the selected leveling zone;
- Worker moves around inside the zone using simple pseudo-random / coverage waypoints;
- terrain does **not** become flat instantly;
- terrain **progressively** converges toward the target construction plane while the Worker performs the job;
- visual terrain changes occur progressively;
- the Worker must remain **visibly involved**.

Do not design this as a progress bar that instantly replaces terrain at completion.

**DATA-DRIVEN / DESIGN REQUIRED** before implementation:

- target elevation determination;
- slope / flatness tolerance;
- leveling speed / duration;
- worker movement pattern;
- interruption / resume rules.

## Foundation Slabs

Buildings cannot be deployed directly onto raw terrain anymore.

Foundation material is acquired from **orbit**, using the same procurement philosophy as Wall Package ([`10_Orbital_Delivery`](10_Orbital_Delivery.md)):

- player orders Foundation Slab material with **Orbital Ferronite**;
- one orbital delivery brings foundation stock to **MainBase inventory**;
- installation consumes **already-delivered** stock;
- **no second Orbital spend** at installation.

Do **not** copy the Wall Package quantity of 5 automatically.

| Field | Status |
| --- | --- |
| Package cost | **TBD** (data-driven) |
| Package quantity | **TBD** (data-driven; not automatically 5) |
| Transport / delivery tuning | **TBD** (data-driven) |
| Exact slab footprint (BuildGrid cells covered by one physical slab/panel) | **TBD** (data-driven). Owner example of ~8 cells is an **example only**, not canonical balance. |

Worker installs already-delivered foundation material onto leveled cells. Worker still does not construct the building.

## Foundation Grid Model

Installed foundation state is tracked **per BuildGrid cell**.

A delivered physical slab / panel may cover **multiple** BuildGrid cells. Placement validation does not care which original package / slab item supplied each cell. It only cares whether each required cell currently has intact installed foundation.

This allows:

- one slab covering several cells (example: 8 — not canonical);
- two smaller buildings sharing different cells of the same installed slab area;
- a larger building spanning cells supplied by multiple slabs;
- arbitrary building footprints over **contiguous intact** foundation coverage.

## Building Placement Rule

For **normal player-deployed buildings**, **every** BuildGrid cell in the building footprint must have:

- sufficiently leveled terrain;
- intact installed foundation;
- no conflicting occupancy;
- all other existing placement requirements (including FoW / NavMesh / clearance as those systems already require).

Only then may the READY orbital building be deployed.

The building still arrives by DropPod and is immediately operational after delivery. There is still **no** local building construction phase.

Initial MainBase is excluded (next section). **Wall / foundation interaction is DESIGN REQUIRED** (see Walls).

## Initial MainBase Exception

Initial MainBase remains the match-start deployment exception.

- It should begin on an authored / prepared starting site / starter foundation as part of map initialization.
- The player is **not** required to level terrain before the initial MainBase exists.
- Exact starter-foundation implementation is **deferred**.

## Foundation Damage / Destruction

Foundation must react to terrain-destruction events.

**Foundation destruction is per cell.**

Do **not** model a placed slab as one all-or-nothing destructible object.

Examples:

- A building occupies four foundation cells and explodes on death: only foundation cells affected by that explosion / deformation footprint are destroyed. Do **not** automatically destroy the entire original slab those cells came from.
- An artillery / tank impact may destroy terrain cells and foundation cells in the affected area while neighboring foundation cells remain intact.

Destroyed foundation cells no longer satisfy future building-placement validation.

This partial-destruction rule is **canonical**.

### Open question — surviving building after foundation loss

**DESIGN REQUIRED. Do not invent the rule.**

If a still-alive building loses some supporting foundation because of an **external** explosion, later design may choose among outcomes such as:

- building remains until destroyed normally;
- building receives structural damage;
- unsupported building collapses.

**No option is approved yet.**

## Walls

Do **not** silently decide whether Wall segments require foundation slabs. This directly affects the later Wall redesign.

**`Wall/Foundation interaction = DESIGN REQUIRED at Building System design gate.`**

The generic phrase “buildings require foundation” applies to **normal orbital buildings**. Wall remains an explicit unresolved exception until that gate.

Wall-mounted Turret naturally depends on whatever final wall rule is chosen.

## Projectiles And Explosions

Do not implement projectile gameplay now.

Future contract only:

- projectile-based weapons later emit impact / explosion events;
- if a projectile hits terrain / misses its target, terrain deformation occurs at impact;
- unit / building destruction may emit an explosion / deformation event;
- the terrain system consumes a **generic deformation request** rather than knowing about every weapon type.

Exact blast crater radius / depth / formula: **TBD**.

## Navigation

Dynamic terrain affects pathing. Implementation must account for crater creation, leveling, height changes, walkability, Worker access to leveling areas, and unit navigation after deformation.

Exact NavMesh / voxel-navigation update strategy is **DESIGN / TECH-SPIKE REQUIRED**. Do not assume rebuilding the entire NavMesh after every explosion is acceptable.

## Fog Of War

Current world FoW presentation was finalized against effectively **planar** terrain and a fixed ground-projection assumption.

Once Voxel terrain deformation is implemented, world FoW presentation must be adapted to the **actual terrain surface**.

Do **not** reopen FoW implementation in this documentation slice. That adaptation is a required integration task inside the Terrain / Voxel stage.

FoW gameplay visibility grid remains conceptually independent from terrain rendering unless a later design explicitly adds terrain occlusion.

## MVP Scope

In MVP this system is intended to provide:

- deformable voxel terrain as a gameplay surface;
- generic deformation requests from explosions / future projectile misses;
- Worker leveling / site preparation on the existing BuildGrid;
- orbital Foundation Slab procurement + MainBase inventory + installation;
- per-cell foundation coverage as a placement prerequisite for normal orbital buildings;
- per-cell foundation destruction;
- navigation and world-FoW surface integration as part of the Terrain stage.

Out of this slice (still later / DESIGN REQUIRED):

- projectile weapons themselves;
- exact Voxel Plugin edition / API;
- surviving-building-after-foundation-loss rule;
- whether Walls require foundation;
- starter-foundation implementation details.

## Open Design Questions (must remain TBD)

- Leveling zone sizing UX (fixed footprint vs drag rectangle).
- Target leveling elevation algorithm.
- Flatness / slope tolerance.
- Leveling duration / speed.
- Worker movement pattern and interruption / resume rules.
- Foundation package quantity.
- Foundation package cost.
- Exact slab footprint in BuildGrid cells.
- Exact blast crater radius / depth / terrain damage formula.
- Voxel Plugin version / edition / API.
- Multiplayer voxel replication mechanism.
- Dynamic navigation strategy.
- Surviving-building behavior after partial foundation loss.
- Whether Walls require foundation.
- Exact starter-foundation implementation for initial MainBase.

## Examples (illustrative, not balance)

- A physical slab might cover about 8 BuildGrid cells. **8 is an example.**
- Two 4-cell buildings may sit on different cells of one previously installed slab area.
- A 10-cell building may span cells supplied by more than one slab.
- An explosion that covers two of four supporting cells destroys only those two foundation cells.

## References

- Core loop — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md)
- Units / Worker — [`04_Units`](04_Units.md)
- Buildings / placement — [`05_Buildings`](05_Buildings.md)
- Orbital delivery — [`10_Orbital_Delivery`](10_Orbital_Delivery.md)
- Fog of War — [`11_Fog_of_War`](11_Fog_of_War.md)
- Engineering — [`../TDD/16_Voxel_Terrain_And_Foundations`](../TDD/16_Voxel_Terrain_And_Foundations.md)
- ADR — [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md)
- Orbital pillar — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)
