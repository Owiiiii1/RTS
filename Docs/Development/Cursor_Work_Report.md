# Cursor Work Report — Production HUD Layout Spec

## Status

**PRODUCTION_HUD_LAYOUT_DOCUMENTATION_READY_FOR_REVIEW**

## Branch / base / head

- Branch: `docs/gp-production-hud-layout-spec`
- Base: `origin/main` @ `317ce3f0367111081e3a8987c8ac8beebfbd6310`
- Head: `6998b00d305b2769d705f66ddb535ccc2b8be6de`
- **NOT MERGED**

## Scope confirmation

Documentation only. No runtime code. No `GP/Content`, Config, maps, Blueprints, DataAssets, or Tools.
No Unreal tests or builds.

## Superseded old HUD layout

The previous canonical in-match HUD is **SUPERSEDED** and must not remain as a second source of truth:

- resource/score stack at top-right
- selection panel bottom-left
- command bar bottom-center
- minimap top-right or bottom-right

## Exact new top-bar structure

Two horizontal bars, three major blocks each. Central battlefield stays unobstructed.

- **Top left — Threat + Score:** Ferronite Threat (pressure) + player Ferronite Score
- **Top center — Match Timer:** strongest isolated central readout
- **Top right — Economy + Unit Cap:** Planet Ferronite, Orbital Ferronite, `CurrentUnits / MaxUnits`

## Exact new bottom-bar structure

- **Bottom left — Minimap block:** square placeholder; function not implemented in the next visual slice
- **Bottom center — Selection / Current Info:** widest lower block
- **Bottom right — Context Action Grid:** table/grid; not a permanent Build Menu

## Single-selection mode

Exactly one unit or one building: entity icon, display name, current health, relevant stats only
(Health/Max, Damage, Armor, Move Speed, later cargo/work where applicable). Stats come from actual
entity type/data. Do not force irrelevant stats.

## Group-selection 10×3 mode

Multiple units: 10 icons per row, 3 rows, 30 visible slots. Each icon has a small current-health bar
beneath it. Overflow/paging/aggregation beyond 30 is **TBD / UX DESIGN REQUIRED**. Do not silently
cap gameplay selection to 30.

## Unit Action Grid

When one unit or a unit group is selected:

1. Move
2. Stop
3. Attack-Move ("идти с атакой") — not a rename of direct RMB target Attack
4. Patrol — **PLANNED / DESIGN TARGET**, not runtime-complete

Future unit abilities may occupy extra cells. Ability slots are not fully designed.

## Building Action Grid

Same right-side panel, building selected. Current MVP may have no functional actions.
Future upgrades/ops must not be invented now. Not local building production.
READY orbital procurement remains the separate Order Menu / TEMP HUD flow.

## Patrol

Marked **PLANNED / DESIGN TARGET**. Not implemented. Do not claim Patrol complete.

## Planet Ferronite vs Threat current-source clarification

Current factual threat source: `AGP_GameState` / `UGP_MatchViewModel.FerroniteThreatValue`.

Raw Ferronite stored at planet/MainBase is the **same underlying gameplay quantity** that currently
drives that threat value. The HUD may present it twice:

- Threat block = danger/pressure
- Planet Ferronite block = exact numeric amount

Do not invent a second gameplay currency. If Threat later becomes derived/non-linear, architecture
may separate them. ResourceVM does not yet expose Planet Ferronite (later adapter from MainBase
storage). Opponent score is not part of this approved two-bar prototype (placement TBD).

## Visual prototype contract

Not final art. Medium/dark grey major blocks, lighter grey inner cells, thin borders, modest
rounding, stronger contrast for selected/hover, clear spacing. No decorative sci-fi art, textures,
or final icons required. Placeholder icon fields are acceptable.

## Runtime boundary (do not claim complete)

Already on `main`: `UGP_UserWidgetBase`, `UGP_HUDRootWidget`, `UGP_ResourceViewModel`,
`UGP_MatchViewModel`, `UGP_HUDViewModelSubsystem`, push adapters.

Not implemented:

- `WBP_GP_HUD`
- SelectionVM
- minimap function
- Context Action Grid
- Patrol

## Exact changed files

- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md` (new)
- `Docs/GDD/09_UI_UX.md`
- `Docs/GDD/README.md`
- `Docs/GDD/02_Core_Gameplay_Loop.md`
- `Docs/GDD/05_Buildings.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/README.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/TDD/05_Unit_Architecture.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/07_Resource_Architecture.md`
- `Docs/TDD/13_Architecture_Proposal.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Naming_Conventions.md`
- `Docs/Development/Cursor_Work_Report.md`

## Documentation-only confirmation

No runtime, Content, Config, maps, Blueprints, DataAssets, or Tools files are part of this slice.
No Unreal tests or builds were run.

## NOT MERGED
