# Cursor Work Report

## Status

**SWARM_CONCEPT_ADR_PILLAR7_LANGUAGE_RECONCILED**

Documentation-only correction on `docs/swarm-concept`. Approved SWARM gameplay concept is unchanged. Runtime implementation **not started**. ADR-0006 was **not** modified.

## Branch / SHAs

- Branch: `docs/swarm-concept`
- Worktree: `D:\Progects\RTS-worktrees\docs-swarm-concept`
- Base `origin/main`: `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Previous branch head: `bcdd2f9b4eab5bf88419ec93d819c62f40d49868`
- Correction commit: `b2fe522e2e59ee8e82ebc5eda460f7615fbf08c8` (`docs: reconcile swarm concept with architecture constraints`)
- This report is a follow-up commit on the same branch

## Corrected documents

- `Docs/TDD/17_SWARM_Architecture.md`
- `Docs/GDD/14_SWARM.md`
- `Docs/GDD/01_Game_Pillars.md`
- `Docs/TDD/13_Architecture_Proposal.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/TDD/README.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/GDD/04_Units.md`
- `Docs/GDD/Lore_Setting.md`
- `Docs/GDD/Out_Of_Scope.md`
- `Docs/Development/Cursor_Work_Report.md` (this file)

ADR-0006 was not changed.

## Mass / ADR-0006 reconciliation

ADR-0006 remains Accepted. ECS-like abstraction and UE Mass Entity gameplay stay hard-banned.

Corrected:

- Lightweight group simulation is the **only allowed** SWARM gameplay backend under current ADRs.
- Mass Entity is **forbidden** in SWARM implementation while ADR-0006 stands — not a reserve, not “not chosen”, not an unlocked later backend.
- A performance spike may show lightweight groups miss prototype targets.
- Even then Mass **must not** be adopted automatically.
- Mass requires a **new ADR** that explicitly changes / supersedes the relevant ADR-0006 ban, with migration scope and profiling evidence.
- Niagara / VAT / skeletal are **visual renderer** choices, not a gameplay ECS backend.
- Gameplay backend and visual renderer are documented as **separate decisions**.

## Pillar 7 reconciliation

Pillar 7 no longer claims every gameplay-active object is a simple machine with a total creature-skeletal ban.

Corrected:

- Simple machines / mechanical primitives / no humanoid-military-cinematic animation apply to **corporate / player-controlled** units and buildings.
- SWARM is an explicit **environmental biological exception**.
- SWARM may use organic skeletal animation.
- SWARM animation stays production-bounded, RTS-camera readable, LOD-friendly.
- No hero-quality bespoke sets, cinematic complexity, or heavy unique AnimBP per visual member.
- Large: full skeletal in small count. Medium / Small: sharing / VAT / Niagara / LOD where a prototype confirms it.

Approved SWARM pressure / spawn / class / crush / blood / networking concept is unchanged.

## Language audit

In `Docs/GDD/14_SWARM.md`:

- «кроваве пятно» → «кривава пляма»
- «пятно» / «пятна» → «пляма» / «плями»
- «Заповільнення» → «Сповільнення»

`rg` after edits: no remaining `кровав`, `пятн`, or `Заповільнення` in Docs.

## Validation

### rg

- `Mass` / `MassEntity` / `ADR-0006`: Mass is forbidden under current ADR-0006; allowed path requires a new ADR. TDD/00, GDD/01 Pillar 9, and Out_Of_Scope already reject MassEntity gameplay.
- `reserve`: remaining hits are grid/inventory/minimap “reserve”, or the explicit phrase that Mass is **not** a reserve.
- `backend`: gameplay backend vs visual renderer vs navigation/obstacle approach are split.
- `creature-like` / `skeletal`: player bans retained; SWARM organic skeletal allowed and bounded.
- Language terms: cleared as above.

### Links

New/changed relative links to GDD/14, TDD/17, ADR-0006 resolve. Pre-existing broken `SKILLS/` links were not in scope.

### Diff / status

Correction commit is **Markdown only**. No C++, Blueprint, `Content/`, `Config/`, `GP.uproject`, or authored assets. ADR-0006 untouched. Main working copy left on `ui/gp-minimap` with dirty Content/Config.

### Build / tests

**Not required.** Documentation-only correction. Unreal builds and tests were not run.

## NEXT

Unchanged: leftover production HUD / minimap, then terrain, AI. SWARM runtime still **not started** and is **not** NEXT.
