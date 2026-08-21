# GP — Production HUD Layout Spec

**Status:** `PRODUCTION_HUD_LAYOUT_DOCUMENTATION_READY_FOR_REVIEW`
**Branch:** `docs/gp-production-hud-layout-spec`
**Base:** current `origin/main` @ `317ce3f0367111081e3a8987c8ac8beebfbd6310`

## Purpose

Replace the superseded coarse HUD layout with the approved production information architecture
and visual-block prototype contract. This is documentation only. The next visual HUD implementation
slice must follow this layout.

## Runtime boundary (factual)

Already implemented (data foundation, not visual HUD):

- `UGP_UserWidgetBase`
- `UGP_HUDRootWidget`
- `UGP_ResourceViewModel`
- `UGP_MatchViewModel`
- `UGP_HUDViewModelSubsystem`
- push-based adapters
- `gp.UI.HUDDump`

Not implemented and must not be claimed complete:

- authored `WBP_GP_HUD`
- visible production resource/timer HUD
- SelectionVM / selection panel
- Context Action Grid
- minimap functionality
- Patrol command
- Order Menu
- notifications
- production end-of-match screen

TEMP HUD remains the live operator surface.

## Superseded layout

The previous canonical in-match HUD is **SUPERSEDED** and must not be implemented:

- resource/score stack at top-right
- selection panel bottom-left
- command bar bottom-center
- minimap top-right or bottom-right

## Canonical structure

The in-match HUD is **two horizontal bars**. Each bar has **three major blocks**.
The central battlefield remains visually unobstructed.

```
TOP:
+-----------------------------------------------------------------------+
| [ THREAT + SCORE ]       [ MATCH TIMER ]       [ PLANET / ORBIT / CAP ]|
+-----------------------------------------------------------------------+

CENTER:
|                                                                       |
|                         GAME WORLD                                    |
|                                                                       |

BOTTOM:
+-----------------------------------------------------------------------+
| [ MINIMAP ]       [ SELECTION / CURRENT INFO ]       [ ACTION GRID ]  |
|   square                  wide rectangle                table/grid     |
+-----------------------------------------------------------------------+
```

Bottom-center is the widest lower block. Minimap stays approximately square.
Action Grid needs enough width for a command table. Exact pixels/percentages are
implementation tuning.

## Top bar

### Top left — Threat + Score

- Ferronite Threat (danger/pressure presentation)
- Player Ferronite Score

Current factual threat source: `AGP_GameState` / `UGP_MatchViewModel.FerroniteThreatValue`.

Raw Ferronite currently stored on the planet/MainBase is the **same underlying gameplay quantity**
that drives `FerroniteThreatValue`. The HUD may present that current source in two ways:

- Threat block = danger/pressure
- Planet Ferronite block = exact numeric amount

Do **not** invent a second gameplay currency. If Threat later becomes a derived/non-linear value,
presentation architecture may separate them.

### Top center — Match Timer

- Match time / countdown
- Strongest isolated central readout
- Source: `UGP_MatchViewModel.MatchTimeRemaining`

### Top right — Economy + Unit Cap

Three compact readouts:

- **Planet Ferronite** — exact numeric raw Ferronite currently stored at MainBase / contributing
  to current threat. Runtime source today: MainBase `UGP_StorageComponent`, not a ResourceVM field yet.
- **Orbital Ferronite** — spendable orbital currency (`UGP_ResourceViewModel.OrbitalFerronite`)
- **Unit Capacity** — `CurrentUnits / MaxUnits`

Opponent score is **not** part of this approved two-bar prototype. Placement of opponent score
inside this layout remains TBD and must not be silently restored into the old top-right stack.

## Bottom bar

### Bottom left — Minimap block

Square reserved region with correct sizing/alignment.

Minimap functionality is **not** implemented by the next visual HUD slice.
Do not claim minimap complete. Placeholder only.

### Bottom center — Current Selection / Information

Two presentation modes. This is the widest lower block.

#### A. Single-entity mode

Used when exactly one unit **or** one building is selected.

Show:

- entity icon
- display name
- current health
- relevant gameplay stats

Possible stat categories include Health / Max Health, Damage, Armor, Move Speed, and other
entity-relevant stats. Do not force irrelevant stats onto every entity. Exact inventory is
driven by entity type/data and may expand later.

Worker/specialized later values (cargo, work state) may appear here. This block also absorbs
single-target inspect presentation; a separate overlapping InspectPanel slot is no longer canonical.

#### B. Group mode

Used when multiple units are selected.

Compact icon grid:

- 10 icons per row
- 3 rows
- 30 visible icon slots

Each icon:

- unit icon
- small current-health bar directly below the icon

Do **not** invent behavior for selections larger than 30.
Overflow/paging/aggregation beyond 30 is **TBD / UX DESIGN REQUIRED**.
Do **not** silently cap gameplay selection itself to 30. This is only the visible HUD grid contract.

### Bottom right — Context Action Grid

Rectangular / table-style action panel. Mode depends on selection type.
This is **not** a permanent Build Menu.

#### A. Unit Action Mode

When one unit or a unit group is selected.

Base command cells:

1. **Move** — choose destination point
2. **Stop** — stop current unit/group command
3. **Attack-Move** — move toward selected point while engaging enemies ("идти с атакой")
4. **Patrol** — **PLANNED / DESIGN TARGET**, not runtime-complete. Patrol from current/assigned
   point to target and back repeatedly.

Existing direct/context target Attack via RMB remains a separate contextual gameplay behavior.
Do not rename direct target Attack as Attack-Move.

Future unit-specific abilities may populate additional cells. Do not fully design ability slots yet.

Existing command tags such as Mine/Repair may still appear as additional cells when the
selected entity actually grants them. They are not a replacement for the four base cells above.

#### B. Building Action Mode

When one building is selected, the same right-side panel switches to Building Actions.

Current MVP may contain **no functional actions** yet.

Future possible actions: upgrades, building-specific operations, other contextual capabilities.
Do not invent actual upgrades now. Do not treat this panel as local building production.
READY orbital building procurement remains the separate orbital Order Menu / TEMP HUD flow.

## Visual prototype contract

Not final art direction. Intentionally simple:

- main HUD blocks = medium/dark grey rectangles
- internal readout/action cells = lighter grey rectangles
- thin borders
- modest corner rounding
- selected/active/hover states may use stronger border/fill contrast
- clear spacing between major blocks
- no decorative sci-fi art, textures, or final icons required
- placeholder icon fields are acceptable

Purpose: validate layout, sizing, information hierarchy, and interaction before visual skinning.

## Canonical docs

- Gameplay IA: [`Docs/GDD/09_UI_UX.md`](../../GDD/09_UI_UX.md)
- Engineering binding/layout: [`Docs/TDD/12_UI_Architecture.md`](../../TDD/12_UI_Architecture.md)
- Selection surface mapping: [`Docs/TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md)
