# Cursor Work Report — Voxel Terrain / Foundation Documentation (refinement)

## Status

**VOXEL_TERRAIN_FOUNDATION_DOCUMENTATION_READY_FOR_REVIEW**

**NOT MERGED.**

## Branch

- Branch: `docs/gp-voxel-terrain-foundations`
- Base: `origin/main` @ `26e0dfa` (FoW world visualization MERGED / operator accepted)
- Head: this refinement commit on the same branch

## Wall/Foundation decision resolved

**Wall segments do not require Foundation Slabs.**

Reason: Wall material arrives as Wall Package stock; segments are not completed orbital building drops; Workers assemble them in the field; they may be constructed directly on terrain.

Wall placement still must validate terrain suitability. Exact slope, visual adapt, auto-level vs manual, and voxel base interaction remain **TBD**.

Wall-mounted Turret follows Wall and does not independently require ground Foundation.

## Local engineering job principle

Any field engineering / local construction that is not delivery of an already-completed orbital asset requires physical Worker participation.

Does **not** apply to READY orbital buildings (Hub, Turret, future READY buildings): they land complete.

## Planned job → Worker assignment → physical work → completion

Player may define the work before Workers are present. Construction does not auto-progress. Work starts when at least one assigned Worker reaches a valid work position. Exact job/site/blueprint class names are **not** invented.

Terrain Leveling: the selected zone is the planned job. Foundation install uses the same conceptual pattern.

## Multi-Worker acceleration

- 0 active Workers = 0 progress
- 1 = baseline
- multiple = faster

Max Workers, scaling formula, contribution rate, assignment/cancel, work-position reservation remain **data-driven / DESIGN REQUIRED**. Workers should not occupy the same point.

## Generic Worker work-presentation start/end hooks

Gameplay emits work-pulse start/end (~1 s presentation target). Blueprint owns authored Niagara/sound/animation. No hardcoded project Niagara in Worker native gameplay. Pulses do not own authoritative progress. Mining presentation is the reference pattern, not a class to copy verbatim.

## Foundation installation requires Worker labor

Sequence: raw terrain → Worker levels → player plans Foundation install → delivered stock available → Workers travel → progressive cell install → intact coverage → READY deploy.

Foundation does not appear instantly because stock exists and the player clicks cells.

Stock consume/reserve moment is **DESIGN REQUIRED**.

## Foundation Repair

Future/canonical Worker engineering job on damaged/destroyed cells. Same planned-job + multi-Worker + presentation hooks. Thresholds, cost, duration, replacement-stock vs repair, and damaged-but-intact support validity remain **TBD**.

## Earthquake future hazard

Post-MVP. Reuses the same generic terrain/foundation deformation/damage architecture as explosions. Do not build a separate earthquake terrain system. Frequency/intensity/area/warning/damage/building interaction remain future design questions.

## Landing/support rationale

Foundation is an engineered landing/support surface for heavy orbital structures, not an arbitrary green-cell gate. Links Terrain Engineering to the Orbital Delivery pillar.

## Docs changed

- `Docs/GDD/13_Terrain_Engineering_And_Foundations.md`
- `Docs/GDD/02_Core_Gameplay_Loop.md`
- `Docs/GDD/04_Units.md`
- `Docs/GDD/05_Buildings.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/TDD/README.md`
- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`
- `Docs/Architecture_Decisions/README.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Remaining TBDs

- Leveling zone UX, elevation algorithm, slope tolerance, speed, interrupt/resume
- Max Workers / scaling / contribution / assignment / work-position reservation
- Foundation package cost/quantity/footprint
- Foundation and Wall stock consume/reserve moment
- Foundation Repair tunables
- Blast/crater formula
- Voxel Plugin version/API and replication
- Dynamic navigation
- Surviving-building after foundation loss
- Wall slope / visual adapt / auto-level / voxel base interaction
- Earthquake parameters
- Starter-foundation implementation

## Documentation-only confirmation

- No `GP/Source`, `GP/Content`, `GP/Config`, Tools, maps, Blueprints, or DataAssets modified.
- No tests. No Unreal builds.

## Merge

**NOT MERGED.**
