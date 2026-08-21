# Cursor Work Report — Voxel Terrain / Foundation Documentation

## Status

**VOXEL_TERRAIN_FOUNDATION_DOCUMENTATION_FINALIZED_READY_FOR_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `docs/gp-voxel-terrain-foundations`
- Base: `origin/main` @ `26e0dfa2ec2ff8ff9eb84c9702f38036b1db3e2f` (`Finalize FoW world visualization after operator acceptance.`)
- Head: this finalization commit on `docs/gp-voxel-terrain-foundations`

## Documentation-only confirmation

This slice is documentation only.

- No Unreal builds.
- No gameplay tests.
- No `GP/Source`, `GP/Content`, `GP/Config`, Tools, maps, Blueprints, or DataAssets changes in this commit.

## Canonical terrain / foundation rules

- Intended backend = **Voxel Plugin**. Exact version/API remains spike-required.
- Terrain is destructible/deformable via a **generic server-authoritative** deformation request.
- Projectiles, explosions, and future earthquakes reuse the same terrain/foundation-damage architecture.
- Normal orbital buildings arrive **COMPLETE** from orbit. Worker does **not** construct the READY building.
- Deploy requires sufficiently leveled terrain + intact Foundation coverage for **every** required BuildGrid cell (initial MainBase excepted).
- Foundation material comes from orbit to MainBase inventory. Player **plans** installation. Foundation does **not** appear instantly. Physical Workers must reach the job and install through labor. State and partial destruction are **per BuildGrid cell**. Foundation Repair is a Worker engineering job. Stock consume/reserve timing remains **DESIGN REQUIRED**.

## Local engineering / Worker job rules

- Plan/job may exist before Workers arrive.
- 0 active Workers = 0 progress; 1 = baseline; multiple accelerate.
- Exact scaling / max / reservation remain TBD.
- Worker approaches valid work positions and alternates work pulse / reposition.
- Authoritative progress is independent from presentation.
- Gameplay exposes conceptual work start/end pulse hooks. Blueprint owns Niagara/sound/animation. No hardcoded project Niagara asset. Mining is the reference presentation pattern. Exact event names remain implementation detail.
- Worker does **not** build READY orbital buildings. Worker **does** perform field engineering and local Wall construction.

## Wall / Foundation resolved decision

**Wall segments do NOT require Foundation.**

Wall material comes from Wall Package stock. Walls are locally constructed by Workers, may be planned before Workers arrive, and make no progress with 0 Workers. Multiple Workers may accelerate. Terrain suitability/slope/visual adaptation remains TBD. Wall-mounted Turret follows the Wall support rule.

## Earthquake future-extension status

Earthquakes are **future / post-MVP only**. They reuse the generic terrain/foundation damage system. Live earthquake gameplay and tuning remain TBD.

## Final roadmap position

1. Production UI foundation / HUD
2. Minimap + FoW minimap presentation
3. Terrain / Voxel / Foundation system
   - 3A. technical spike + deformation foundation
   - 3B. Worker leveling + generic local engineering job/work hooks
   - 3C. Foundation procurement/install/repair foundation support
   - 3D. building placement migration
   - 3E. navigation + terrain-surface FoW integration
4. RTS AI Opponent
5. bounded core-loop gaps
6. Building-system design gate / Walls (**Wall/Foundation is RESOLVED**, not an open question)
7. Steam multiplayer
8. Match completion
9. SWARM design gate
10. SWARM
11. full MVP stabilization

## Remaining TBD / DESIGN REQUIRED items

- Leveling zone UX, elevation algorithm, slope/flatness tolerance, leveling speed, interrupt/resume
- Max Workers per job, speed scaling, contribution rate, assignment/cancel, work-position reservation
- Foundation package cost, quantity, slab footprint
- Foundation and Wall stock consume/reserve moment
- Foundation Repair thresholds, cost, duration, replacement-stock vs repair, damaged-cell support validity
- Blast/crater radius/depth/formula
- Voxel Plugin version/edition/API and multiplayer replication mechanism
- Dynamic navigation strategy
- Surviving-building behavior after foundation loss
- Wall slope, visual adapt, auto-level vs manual, voxel interaction at wall bases
- Exact starter-foundation implementation for initial MainBase
- Earthquake frequency/intensity/area/warning/damage/building interaction

## Exact changed files (this branch vs `origin/main`)

- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`
- `Docs/Architecture_Decisions/README.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/GDD/02_Core_Gameplay_Loop.md`
- `Docs/GDD/04_Units.md`
- `Docs/GDD/05_Buildings.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/GDD/13_Terrain_Engineering_And_Foundations.md`
- `Docs/GDD/README.md`
- `Docs/README.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/TDD/README.md`

## Runtime / Content / Config / Tools

**No runtime, Content, Config, or Tools changes in this documentation slice.**

## Merge

**NOT MERGED.**
