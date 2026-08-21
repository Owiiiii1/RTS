# Terrain Engineering And Foundations

> **Canonical WHAT document** for destructible voxel terrain, Worker site preparation, and foundation slabs.
> Engineering direction: [`../TDD/16_Voxel_Terrain_And_Foundations.md`](../TDD/16_Voxel_Terrain_And_Foundations.md).
> Decision record: [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md).
>
> This page does **not** implement Voxel Plugin, commands, or classes. Exact names, tunables, and APIs are **TBD / DESIGN REQUIRED** unless stated otherwise.

## Fantasy

The planet is not a static board. Explosions, missed shells, collapsing assets, and later environmental hazards tear the ground. Players do not operate a Barracks. They **prepare a site**, then call a READY orbital building down onto an engineered landing/support surface.

Foundation is not an arbitrary “you need green cells before building” restriction. It is an **engineered landing/support surface for heavy orbital structures**:

```
raw / deformed planetary terrain
  → Worker prepares the site
  → Worker installs delivered engineered foundation
  → orbital DropPod / rocket can safely lower / deploy a completed structure
  → the structure operates on the prepared surface
```

This explicitly links Terrain Engineering to the Orbital Delivery pillar ([ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)).

Worker remains an engineer, not a factory:

- Worker does **not** manufacture Logistics Hubs, Turrets, or any READY building.
- Worker **does** perform local field engineering: terrain leveling, foundation installation, foundation repair, and Wall construction.
- READY buildings still originate from orbit, arrive complete, and are immediately operational after DropPod delivery.
- There is still no Barracks / factory / local building-production queue.

## Orbital Completed Asset vs Local Engineering

Make this distinction explicit. Do not collapse them.

### Orbital completed asset

- purchased / ordered;
- delivered **complete**;
- **no Worker construction phase** after landing.

Examples: Logistics Hub, Defensive Turret, future READY orbital buildings. Units also arrive complete (separate Unit Drop Zone flow).

### Local engineering

- a **plan / job** can be placed first;
- requires **physical Worker arrival**;
- progresses through Worker labor;
- supports **multiple Workers**;
- uses generic **work-presentation pulse** hooks.

Examples: Terrain Leveling, Foundation Installation, Foundation Repair, Wall Construction.

This does **not** mean every gameplay action needs a Worker. It applies specifically to **field engineering / local construction** that is not the delivery of an already-completed orbital asset.

Do not accidentally reintroduce Barracks / factory / local production of READY buildings.

## Local Engineering Requires Physical Workers

**Canonical principle:** any FIELD ENGINEERING / LOCAL CONSTRUCTION operation that is not the delivery of an already-completed orbital asset requires physical Worker participation.

Canonical examples:

- Terrain Leveling;
- Foundation Installation;
- Foundation Repair;
- Wall Construction;
- future local Demolish / engineering operations where applicable.

READY orbital buildings remain different: they arrive already complete by DropPod and do **not** require a Worker to construct them after landing.

## Plan First, Work Second

Local construction / engineering uses a **planned-job model**.

The player may place / define the work **before** Workers are present. Exact runtime representation of “job / site / blueprint” is an **implementation design decision**. Do not treat any class name on this page as existing API.

### Example — Wall

1. Player enters Build Wall mode.
2. Player places / draws the intended Wall construction.
3. A planned construction site / blueprint / job exists.
4. Construction does **not** progress automatically.
5. Player assigns one or more Workers to that job.
6. Workers physically travel to the site.
7. Work begins only when at least one assigned Worker reaches a valid work position.
8. Work progresses while Workers are actively working.
9. On completion, the final Wall segment(s) become operational.

Use the same conceptual pattern where appropriate for **Foundation installation**.

For **Terrain Leveling**, the selected leveling zone itself is the planned job.

## Multiple Workers

A local engineering job may accept multiple Workers. Multiple Workers accelerate completion.

Canonical behavior:

- **0** active Workers = **0** progress;
- **1** active Worker = baseline progress;
- **multiple** Workers = faster progress.

**DATA-DRIVEN / DESIGN REQUIRED** (do not invent):

- max Workers per job;
- linear vs diminishing-return speed scaling;
- contribution rate per Worker;
- worker assignment / cancel behavior;
- reservation of work positions.

The design should support several Workers working around one construction site **without occupying exactly the same point**.

## Worker Physical Work Behavior

When actively performing a local engineering job, the Worker must **visibly interact** with the target.

Desired MVP behavior:

- Worker approaches a valid work position near the job;
- Worker works for a short interval;
- Worker may reposition to another valid side / point around the job;
- Worker continues alternating movement and work until job completion.

For larger sites / multiple Workers: Workers should naturally approach from different available sides / positions where possible.

Do **not** implement sophisticated formation / path planning. Simple deterministic or pseudo-random valid work-point selection is enough later.

Exact cadence / movement algorithm remains **implementation design**.

## Generic Work Presentation Hooks

Local engineering follows the same presentation philosophy as the authored **mining** pattern:

- native / gameplay code owns **work state**;
- Blueprint owns **authored Niagara presentation**.

The owner will attach the actual Niagara system in the Worker Blueprint.

Conceptual pulse (exact event / function names are **not** canonical yet):

```
WORK PULSE START
  → Blueprint may activate authored Niagara / sound / animation

approximately short work interval (~1 second presentation target)

WORK PULSE END
  → Blueprint stops / deactivates the work Niagara
```

Required boundary:

- gameplay emits start / end work-presentation events;
- Blueprint decides which Niagara / VFX to use;
- **no hardcoded project Niagara asset dependency** in Worker native gameplay;
- presentation events do **not** own authoritative progress.

Mining presentation is the **reference pattern**, not necessarily a class / API to copy verbatim. Do not hard-reference a Niagara asset in C++ documentation.

## Canonical Building Sequence

For **normal player-deployed orbital buildings**:

```
raw terrain
  → Worker levels terrain
  → player plans Foundation installation
  → delivered Foundation stock is available
  → one or more Workers travel to the site
  → Workers progressively install Foundation cells
  → intact Foundation coverage becomes available
  → READY orbital building may be deployed (DropPod; immediately operational)
```

Foundation does **not** instantly appear merely because stock exists and the player clicks the cells.

Exact moment when Foundation stock is consumed / reserved is **DESIGN REQUIRED**. Do not invent whether stock is consumed on planning, on work start, per completed cell, or on completion.

Initial MainBase is the match-start exception (see below).

**Wall** does not follow this foundation sequence (see Walls).

## Destructible Terrain

Terrain is no longer conceptually immutable. Gameplay events may deform / destroy terrain.

Canonical **future** deformation sources (not implemented by this documentation slice):

1. Projectile impacts that **miss** their intended unit/building target (later: tank shell, artillery, other physical projectile weapons).
2. Explosions.
3. Unit destruction explosions.
4. Building destruction explosions.
5. **Earthquakes** (future / post-MVP environmental hazard — same generic architecture; see below).

Exact projectile gameplay is deferred until projectile-based units exist. Terrain destruction must **not** be wired to one weapon class.

The design contract is generic:

- a gameplay event may request terrain deformation at a world location;
- radius / depth / strength / shape are **data-driven**;
- exact crater radius, depth, and damage formula are **TBD**.

## Worker Terrain Leveling

Worker gains an engineering role **in addition to** mining / transport / repair:

**terrain leveling / site preparation.**

This is local engineering, not construction of the READY building.

The selected leveling zone itself is the **planned job**. Player may define the zone before Workers are present. Work does not progress until assigned Workers reach valid work positions.

### Command concept

Player initiates a **Level Terrain** / site-preparation mode.

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

After confirmation of the planned zone and Worker assignment:

- Workers move into the selected leveling zone;
- Workers move around inside the zone using simple pseudo-random / coverage waypoints;
- terrain does **not** become flat instantly;
- terrain **progressively** converges toward the target construction plane while Workers perform the job;
- visual terrain changes occur progressively;
- Workers must remain **visibly involved** (work pulses + reposition).

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
- installation consumes **already-delivered** stock (consume / reserve moment **DESIGN REQUIRED**);
- **no second Orbital spend** at installation.

Do **not** copy the Wall Package quantity of 5 automatically.

| Field | Status |
| --- | --- |
| Package cost | **TBD** (data-driven) |
| Package quantity | **TBD** (data-driven; not automatically 5) |
| Transport / delivery tuning | **TBD** (data-driven) |
| Exact slab footprint (BuildGrid cells covered by one physical slab/panel) | **TBD** (data-driven). Owner example of ~8 cells is an **example only**, not canonical balance. |
| Stock consume / reserve moment | **DESIGN REQUIRED** |

Installation is **local engineering**: player plans the install job; Workers travel to the site; cells appear **progressively** through labor. Worker still does not construct the READY building.

## Foundation Grid Model

Installed foundation state is tracked **per BuildGrid cell**.

A delivered physical slab / panel may cover **multiple** BuildGrid cells. Placement validation does not care which original package / slab item supplied each cell. It only cares whether each required cell currently has intact installed foundation.

This allows:

- one slab covering several cells (example: 8 — not canonical);
- two smaller buildings sharing different cells of the same installed slab area;
- a larger building spanning cells supplied by multiple slabs;
- arbitrary building footprints over **contiguous intact** foundation coverage.

## Foundation Repair

**Foundation Repair** is a future / canonical Worker engineering capability.

If Foundation cells are damaged / destroyed:

- they no longer provide valid support **as defined by their state**;
- player can create / issue a Foundation Repair job (plan first);
- Worker must physically travel there and perform repair;
- multiple Workers may accelerate repair;
- repair uses the same generic Worker work-presentation hooks.

**Do not decide yet (TBD):**

- exact damaged-state thresholds;
- repair material / currency cost;
- repair duration;
- whether Destroyed cells require replacement stock vs simple repair;
- whether damaged-but-not-destroyed cells remain valid for building support.

## Building Placement Rule

For **normal player-deployed orbital buildings**, **every** BuildGrid cell in the building footprint must have:

- sufficiently leveled terrain;
- intact installed foundation;
- no conflicting occupancy;
- all other existing placement requirements (including FoW / NavMesh / clearance as those systems already require).

Only then may the READY orbital building be deployed.

The building still arrives by DropPod and is immediately operational after delivery. There is still **no** local construction phase **of that building**.

Initial MainBase is excluded (next section).

**Wall segments do not require Foundation Slabs.** Wall-mounted Turret follows the Wall system and does not independently require ground Foundation beneath the wall.

## Initial MainBase Exception

Initial MainBase remains the match-start deployment exception.

- It should begin on an authored / prepared starting site / starter foundation as part of map initialization.
- The player is **not** required to level terrain before the initial MainBase exists.
- Exact starter-foundation implementation is **deferred**.

## Foundation Damage / Destruction

Foundation must react to terrain-destruction events (explosions, future projectile misses, future earthquakes).

**Foundation destruction is per cell.**

Do **not** model a placed slab as one all-or-nothing destructible object.

Examples:

- A building occupies four foundation cells and explodes on death: only foundation cells affected by that explosion / deformation footprint are destroyed. Do **not** automatically destroy the entire original slab those cells came from.
- An artillery / tank impact may destroy terrain cells and foundation cells in the affected area while neighboring foundation cells remain intact.

Destroyed foundation cells no longer satisfy future building-placement validation.

This partial-destruction rule is **canonical**.

### Open question — surviving building after foundation loss

**DESIGN REQUIRED. Do not invent the rule.**

If a still-alive building loses some supporting foundation because of an **external** explosion (or later earthquake), later design may choose among outcomes such as:

- building remains until destroyed normally;
- building receives structural damage;
- unsupported building collapses.

**No option is approved yet.**

## Walls

**Wall Foundation Rule — RESOLVED:** Wall segments **do not** require Foundation Slabs.

Reason:

- Wall material arrives from orbit as Wall Package stock;
- individual wall segments are **not** completed orbital building drops;
- Wall segments are locally assembled / constructed in the field by Workers;
- therefore they may be constructed **directly on terrain**.

Wall placement must still validate **terrain suitability**.

**TBD / DESIGN REQUIRED** (do not invent):

- maximum slope for Wall placement;
- how Wall adapts visually to uneven terrain;
- whether Worker locally levels only the wall footprint automatically or the player must level difficult terrain manually;
- exact voxel interaction around wall bases.

Wall construction uses the planned-job model (plan / draw first, assign Workers, physical work, then operational segments). Exact Wall inventory consume / reserve moment is **DESIGN REQUIRED**.

Wall-mounted Turret follows the Wall system and does **not** independently require ground Foundation beneath the wall.

## Earthquakes — Future Environmental Hazard

**Earthquakes** are a future / post-MVP environmental hazard concept.

They reuse the **same generic terrain / foundation damage architecture** as explosions. Do **not** build a separate earthquake terrain system.

Earthquakes should eventually emit generic terrain / foundation deformation / damage requests. They may:

- deform voxel terrain;
- create uneven ground;
- damage or destroy individual Foundation cells;
- create new local engineering / maintenance jobs;
- force Workers to repair infrastructure.

**Do not decide (future design questions):**

- earthquake frequency;
- intensity;
- affected area;
- warning system;
- exact damage;
- whether buildings take direct earthquake damage;
- whether foundation loss under an existing building causes collapse / damage (same open question as explosion support-loss).

## Projectiles And Explosions

Do not implement projectile gameplay now.

Future contract only:

- projectile-based weapons later emit impact / explosion events;
- if a projectile hits terrain / misses its target, terrain deformation occurs at impact;
- unit / building destruction may emit an explosion / deformation event;
- the terrain system consumes a **generic deformation request** rather than knowing about every weapon type.

Exact blast crater radius / depth / formula: **TBD**.

## Navigation

Dynamic terrain affects pathing. Implementation must account for crater creation, leveling, height changes, walkability, Worker access to leveling / construction areas, and unit navigation after deformation.

Exact NavMesh / voxel-navigation update strategy is **DESIGN / TECH-SPIKE REQUIRED**. Do not assume rebuilding the entire NavMesh after every explosion is acceptable.

## Fog Of War

Current world FoW presentation was finalized against effectively **planar** terrain and a fixed ground-projection assumption.

Once Voxel terrain deformation is implemented, world FoW presentation must be adapted to the **actual terrain surface**.

Do **not** reopen FoW implementation in this documentation slice. That adaptation is a required integration task inside the Terrain / Voxel stage.

FoW gameplay visibility grid remains conceptually independent from terrain rendering unless a later design explicitly adds terrain occlusion.

## MVP Scope

In MVP this system is intended to provide:

- deformable voxel terrain as a gameplay surface;
- generic deformation requests from explosions / future projectile misses (earthquakes later reuse the same contract);
- generic **local engineering job** contract (plan first, Worker assignment, physical work, completion);
- Worker assignment / contribution model (multi-Worker acceleration);
- reusable Worker **work-presentation** start / end hooks (Blueprint-owned Niagara);
- Worker leveling / site preparation on the existing BuildGrid;
- orbital Foundation Slab procurement + MainBase inventory + Worker installation;
- per-cell foundation coverage as a placement prerequisite for normal orbital buildings;
- per-cell foundation destruction;
- Wall constructed on terrain **without** Foundation (terrain suitability still TBD);
- navigation and world-FoW surface integration as part of the Terrain stage.

Out of this slice / later:

- projectile weapons themselves;
- exact Voxel Plugin edition / API;
- surviving-building-after-foundation-loss rule;
- Foundation Repair tunables;
- Earthquakes as a live hazard;
- Wall slope / visual-adapt / auto-level details;
- starter-foundation implementation details.

## Open Design Questions (must remain TBD)

- Leveling zone sizing UX (fixed footprint vs drag rectangle).
- Target leveling elevation algorithm.
- Flatness / slope tolerance.
- Leveling duration / speed.
- Worker movement pattern, interrupt / resume, work-point selection.
- Max Workers per job; speed scaling; contribution rate; assignment / cancel; work-position reservation.
- Foundation package quantity / cost / slab footprint.
- Foundation stock consume / reserve moment.
- Foundation Repair thresholds, cost, duration, replacement-stock vs repair, damaged-but-intact support validity.
- Exact blast crater radius / depth / terrain damage formula.
- Voxel Plugin version / edition / API.
- Multiplayer voxel replication mechanism.
- Dynamic navigation strategy.
- Surviving-building behavior after partial foundation loss.
- Wall maximum slope, visual adapt to uneven terrain, auto-level vs manual level, voxel interaction at wall bases.
- Wall inventory consume / reserve moment.
- Exact starter-foundation implementation for initial MainBase.
- Earthquake frequency / intensity / area / warning / damage / building interaction (post-MVP).

**Resolved (not TBD):** Wall segments do **not** require Foundation Slabs.

## Examples (illustrative, not balance)

- A physical slab might cover about 8 BuildGrid cells. **8 is an example.**
- Two 4-cell buildings may sit on different cells of one previously installed slab area.
- A 10-cell building may span cells supplied by more than one slab.
- An explosion that covers two of four supporting cells destroys only those two foundation cells.
- A Wall line planned across uneven ground still needs terrain-suitability validation, but **no** Foundation slab.

## References

- Core loop — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md)
- Units / Worker — [`04_Units`](04_Units.md)
- Buildings / placement — [`05_Buildings`](05_Buildings.md)
- Orbital delivery — [`10_Orbital_Delivery`](10_Orbital_Delivery.md)
- Fog of War — [`11_Fog_of_War`](11_Fog_of_War.md)
- Engineering — [`../TDD/16_Voxel_Terrain_And_Foundations`](../TDD/16_Voxel_Terrain_And_Foundations.md)
- ADR — [`../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System`](../Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md)
- Orbital pillar — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)
