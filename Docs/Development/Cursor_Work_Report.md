# Cursor Work Report

## Status

**SWARM_CONCEPT_APPROVED_DOCUMENTED**

Documentation-only checkpoint. Runtime SWARM implementation **not started**. Do **not** start SWARM implementation from this branch. NEXT remains leftover production HUD / minimap (then terrain, AI, …) per the MVP roadmap.

## Branch / SHAs

- Branch: `docs/swarm-concept`
- Worktree: `D:\Progects\RTS-worktrees\docs-swarm-concept` (clean checkout from `origin/main`; main working copy left untouched)
- Base `origin/main`: `3b1d3aff293049cd3014f03e047b21a3dd2e6665`
- Docs commit SHA: `6ea8600218068abec694efaf6c72051920b804f0`
- Report commit SHA: recorded after this file is committed (follow-up on the same branch)

## Changed documents

### New canonical

- `Docs/GDD/14_SWARM.md`
- `Docs/TDD/17_SWARM_Architecture.md`

### GDD reconciliation

- `Docs/GDD/00_Project_Overview.md`
- `Docs/GDD/01_Game_Pillars.md`
- `Docs/GDD/02_Core_Gameplay_Loop.md`
- `Docs/GDD/03_Factions.md`
- `Docs/GDD/04_Units.md`
- `Docs/GDD/05_Buildings.md`
- `Docs/GDD/06_Resources.md`
- `Docs/GDD/07_Match_Flow.md`
- `Docs/GDD/08_Win_Lose_Conditions.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/GDD/11_Fog_of_War.md`
- `Docs/GDD/12_Session_Tuning_And_Calibration.md`
- `Docs/GDD/First_Playable_Match.md`
- `Docs/GDD/Lore_Setting.md`
- `Docs/GDD/Backlog.md`
- `Docs/GDD/Out_Of_Scope.md`
- `Docs/GDD/README.md`

### TDD reconciliation

- `Docs/TDD/03_Multiplayer_Architecture.md`
- `Docs/TDD/05_Unit_Architecture.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/07_Resource_Architecture.md`
- `Docs/TDD/10_Data_Assets.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/13_Architecture_Proposal.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/TDD/README.md`

### Indexes / ops

- `Docs/README.md`
- `README.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Claude_Tasks/GP-0306_AI_Opponent.md`
- `Docs/Development/Claude_Work_Plan.md`
- `Docs/Development/Next_Slice_Audit_Post_S29R.md`
- `Docs/Development/Orbital_Delivery_Mechanics_Review.md`
- `Docs/Development/Cursor_Work_Report.md` (this file)

## Superseded placeholder rules

- Discrete numbered waves as the production model (`WaveInterval`, `WaveSize`, `WaveStartDelay` as a required wave model, `WaveSpawnPoints` as known fixed entries).
- `ThreatToWaveSize` / `ThreatToWaveFrequency` as production schema (`UGP_SwarmThreatCurves`).
- Predetermined wave schedule / first-wave delay / wave-size tables as approved balance.
- `SwarmAggressionLevel` / `AggressionPerUnitShipped` / `AggressionPerUnitMined` (already deprecated; restated).
- Claim that SWARM cannot finish the match.
- Two almost-non-overlapping wars (Small/Medium as harmless to combat machines).
- Nearest-asset targeting as the strategic goal (replaced by that team's MainBase).
- Grunt-only roster / `SpawnSwarmWave` as approved production API.
- Mass Entity as required baseline.
- Thousands of persistent Decal Actors as the blood architecture.
- Mandatory runtime NavMesh rebuild for corpses.

If code or Data Assets still use old names, they are **legacy / current-runtime placeholders**. This checkpoint did **not** rename code.

## Canonical decisions

- SWARM is environmental pressure, not a playable third faction.
- Per-team pressure from current raw Planetary Ferronite in that team's MainBase containers.
- `FerroniteScore` and `OrbitalFerronite` do not drive SWARM.
- Player cannot control or aim SWARM at the opponent.
- MainBase is the strategic target and primary store; MainBase death is immediate annihilation; no Ferronite-eating.
- Continuous flow; threat bands (budget, spawn directions, replenishment, roster, Large cap); existing creatures do not despawn on threat drop.
- Closed outer spawn spline; circular defense, not known spawn-point forts.
- Large / Medium group / Small group presentation vs gameplay authority split.
- AoE hits a gameplay group once; attack damage scales with living visual members.
- Living crush vs corpse crush split; Small vehicle avoidance is local-only.
- Blood is cosmetic accumulated mask; not thousands of Decal Actors.
- Lightweight group simulation is the baseline; Mass considered, not chosen.
- Prototype targets: ~500 visuals per player represented as dozens of groups + few Large Actors; server replicates group state, not per-member transforms.
- Per-team `FerroniteThreatValue` already exists on `AGP_GameState`. Per-team **director / spawn stream is not implemented**.

## Prototype / TBD

- Exact DA schema and director class.
- Concrete threat-band numbers (none invented).
- Renderer mix (Niagara / VAT / skeletal / far sprites) after visual/performance prototype.
- Navigation/obstacle backend (transient traversability vs local repath; no mandatory NavMesh rebuild).
- Reservation/queue algorithm for Medium/Large chokes.
- Mass as a later reserve after prototype/profile — not forbidden forever, not baseline.
- Performance numbers are targets, not measured guarantees. Mandatory performance spike before backend lock.

## Validation

### rg

Remaining hits for `WaveInterval`, `WaveSize`, `WaveStartDelay`, `WaveSpawnPoints`, `ThreatToWaveSize`, `ThreatToWaveFrequency`, `SwarmAggressionLevel` are explicitly **superseded / legacy / current-runtime placeholders**. No unmarked production-schema claims found in active GDD/TDD.

`wave` / `waves` remain only as contrast (“not numbered waves”), historical audit banners, or explicitly superseded examples.

### Links

New/changed GDD/TDD/index relative links to `14_SWARM` / `17_SWARM_Architecture` resolve. Pre-existing broken `SKILLS/` links were not in scope.

### Diff / status

Docs commit contains **only Markdown**. No C++, Blueprint, `Content/`, `Config/`, `GP.uproject`, or authored assets. Main working copy dirty Content/Config files were not touched (worktree from `origin/main`).

### Build / tests

**Not required** for this documentation-only checkpoint. Unreal builds and tests were not run.

## NEXT

Not SWARM implementation. Remaining queue: leftover production HUD / minimap, then terrain, AI opponent, later stages. SWARM remains the **final gameplay implementation stage** after the now-complete design/docs gate.

This checkpoint does **not** re-verify unrelated HUD/minimap branches as merged. Factual on `origin/main` @ `3b1d3af`: Bottom HUD finalization is present. Do not treat `feature/gp-hud-threat-normalized` or minimap as completed from this pass.
