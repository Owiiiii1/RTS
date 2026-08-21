# Cursor Work Report — Voxel Terrain / Foundation Documentation

## Status

**VOXEL_TERRAIN_FOUNDATION_DOCUMENTATION_READY_FOR_REVIEW**

**NOT MERGED.**

## Branch / base / head

- Branch: `docs/gp-voxel-terrain-foundations`
- Base: `origin/main` @ `26e0dfa2ec2ff8ff9eb84c9702f38036b1db3e2f` (`Finalize FoW world visualization after operator acceptance.`)
- Head: branch tip of `docs/gp-voxel-terrain-foundations` (`Document destructible voxel terrain, Worker leveling, and per-cell foundations.`)

## New canonical design decisions

- Terrain is no longer conceptually immutable. Gameplay may deform it via a **generic** location + data-driven radius/depth/strength/shape contract. Not tied to one weapon class.
- Intended backend: **Voxel Plugin**. Exact version/edition/API is **not** decided; later technical spike.
- Deformation is **server-authoritative**; clients reconstruct; clients do not author gameplay destruction. Replication mechanism is **spike-required**.
- Worker gains **terrain leveling / site preparation** in addition to mine/transport/repair. Worker does **not** construct the READY building.
- Canonical sequence for normal buildings: raw terrain → level → install foundation coverage → deploy orbital READY building (still DropPod, still immediately operational).
- Foundation material uses **Wall Package procurement philosophy** (Orbital spend once → MainBase inventory → consume on install). Quantity/cost/footprint are **TBD** (do **not** copy 5).
- Foundation state is **per BuildGrid cell**. A physical slab may cover multiple cells. Partial destruction is canonical.
- Initial MainBase remains the authored starter-site exception.
- BuildGrid stays the discrete planning/occupancy/foundation grid. Voxel terrain stays continuous geometry. Do not merge them.
- Current world FoW stays as merged planar/fixed-projection presentation. Terrain-surface FoW adaptation is a **required Terrain-stage integration task**, not a FoW reopen.

## Roadmap insertion

FoW world visualization is **MERGED / operator accepted**, not pending.

Reconciled execution order:

1. Production UI foundation / HUD
2. Minimap + FoW minimap presentation
3. **NEW — Terrain / Voxel / Foundation system**
   - 3A. Voxel Plugin technical spike + authoritative terrain deformation foundation
   - 3B. Worker terrain leveling / site-preparation loop
   - 3C. Orbital Foundation Slab procurement + MainBase inventory + installation
   - 3D. Building placement migration to leveled + intact foundation requirement
   - 3E. navigation + current world-FoW terrain-surface integration
4. RTS AI Opponent
5. Remaining bounded core-loop gaps (Stop, Worker Repair, Logistics Hub storage-cap bonus, necessary feedback)
6. Building-system design gate (Wall/foundation rule, Wall surface, connection, Wall Turret, Sell/Demolish)
7. Steam multiplayer product flow
8. Match completion product flow
9. SWARM design/reconciliation gate
10. SWARM implementation
11. Full MVP validation/stabilization

Reason: terrain/foundation must exist before AI and final building/wall design because both depend on construction-site rules and navigation.

## Documents created

- `Docs/GDD/13_Terrain_Engineering_And_Foundations.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`

## Documents updated

- `Docs/GDD/02_Core_Gameplay_Loop.md`
- `Docs/GDD/04_Units.md`
- `Docs/GDD/05_Buildings.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/GDD/README.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/README.md`
- `Docs/Architecture_Decisions/README.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/README.md`

## Unresolved design questions preserved as TBD

- Leveling zone sizing UX (fixed footprint vs drag rectangle)
- Target leveling elevation algorithm
- Flatness / slope tolerance
- Leveling duration / speed
- Worker movement pattern and interrupt/resume
- Foundation package quantity
- Foundation package cost
- Exact slab footprint
- Exact blast crater radius / depth / terrain damage formula
- Voxel Plugin version / API
- Multiplayer voxel replication mechanism
- Dynamic navigation strategy
- Surviving-building behavior after foundation loss
- Whether Walls require foundation
- Exact starter-foundation implementation for initial MainBase

## Documentation-only confirmation

- Documentation-only slice.
- No runtime (`GP/Source`) changes.
- No `GP/Content`, `GP/Config`, `Tools`, maps, Blueprints, DataAssets, or authored-content changes.
- No tests. No Unreal builds.

## Merge

**NOT MERGED.**
