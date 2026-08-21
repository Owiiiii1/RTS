# GP — Production HUD Layout Spec

**Status:** `PRODUCTION_HUD_LAYOUT_AND_MAINBASE_PROCUREMENT_DOCUMENTATION_READY_FOR_REVIEW`
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
- MainBase PURCHASE / procurement panel (Units / Buildings / Defense)
- fullscreen / global Order Menu (superseded as the production HUD path)
- notifications / global toast stack
- production end-of-match screen

TEMP HUD remains the live operator surface and may still expose debug procurement controls.
Backend orbital procurement already exists; the production HUD interaction below is **DESIGN /
NEXT IMPLEMENTATION**.

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

Do not turn every building into a procurement source. Only **MainBase** owns orbital PURCHASE
access in this design.

**MainBase selected:** the first explicitly defined Building Action is **PURCHASE**. Future
unrelated MainBase actions may occupy other cells later.

**Other buildings:** building-specific contextual actions only. Current MVP may still have no
functional actions. Do not invent upgrades. Do not treat this panel as local building production.

Orbital procurement is **not** a permanent global HUD panel and is **not** a separate fullscreen
Order Menu. Canonical visible entry:

Select MainBase → PURCHASE → UNITS / BUILDINGS / DEFENSE

inside the same bottom-right panel.

## Message Strip

A small horizontal **MESSAGE STRIP** sits directly **above** the bottom-right Context Action /
Procurement block. It belongs to that right-side HUD area.

Purpose: short contextual procurement/action feedback. Not a full global notification/toast system.
Default: empty when no useful contextual message exists.

Examples:

- Shuttle capacity: X slots
- Shuttle: 3 / 4 slots
- Not enough Orbital Ferronite
- Shuttle capacity reached
- Unit cap reached
- Building unavailable
- Placement unavailable
- Wall stock full
- Delivery already pending / Wall delivery already pending

## MainBase procurement (design / not implemented)

Uses existing server-authoritative orbital flows. Do **not** redesign gameplay authority or
spending semantics. Do **not** invent a new building-spawn RPC.

Bottom-center Selection/Info **does not** switch to procurement while MainBase is selected.
It continues to show MainBase icon, name, health, and relevant stats/state.

Click **PURCHASE** replaces the action-grid content with three large category buttons:

- UNITS
- BUILDINGS
- DEFENSE

Navigation stays inside the same bottom-right panel.

A later keyboard shortcut may convenience-activate MainBase procurement. It is **not** the
canonical visible entry. Global `O` Order Menu as the production HUD path is **SUPERSEDED**.

### Right-panel state machine

```
NO / GENERIC SELECTION
  → relevant unit/building commands

MAINBASE SELECTED
  → Building Actions
  → PURCHASE available

PURCHASE
  → category chooser: UNITS | BUILDINGS | DEFENSE

UNITS
  → unit manifest grid/list
  → LMB add / RMB remove / quantity badges
  → LAUNCH SHUTTLE (existing unit-manifest Confirm)

BUILDINGS
  → building list (no Wall Package, no MainBase)
  → select building → selected-item launch state
  → BACK (no spend, no READY consume) or LAUNCH
  → LAUNCH = Purchase → READY → immediately enter deploy ghost

DEFENSE
  → defense list (Defensive Turret, Wall Package; Wall-mounted Turret when production-ready)
  → turret: selected-item launch state → Purchase → READY → deploy ghost
  → wall: selected-item launch state → Buy Wall Package → delivery to MainBase stock
```

The Message Strip stays attached above this block and reflects the current context.

### UNITS

Show the current factual orbital unit catalog.

Backend remains:

unit manifest → validate Orbital Ferronite → validate transport slots → validate player unit cap
→ Confirm → one DropPod to MainBase Unit Drop Zone

**Icon interaction**

- **LMB:** add one unit of that type to the pending shuttle manifest; show a small quantity
  number on the icon. Repeated LMB increments while valid.
- **RMB:** remove one of that type; decrement quantity; hide the marker at zero.

UI must not create invalid authoritative state. It may locally prevent obvious additions when
capacity/funds/cap are already known. Final validation remains server-authoritative.

**Shuttle capacity** (not `CurrentUnits / MaxUnits`)

Before the first unit is added, Message Strip should show shuttle capacity, e.g.
`Shuttle. Capacity — X slots` where X is factual `PodTransportSlotCapacity`.
Exact player-facing wording may be refined later.

As the manifest changes, examples:

- Shuttle: 3 / 4 slots
- Shuttle capacity reached
- Not enough Orbital Ferronite
- Unit cap reached

Existing per-unit `TransportSlotCost` rules remain canonical.

**LAUNCH SHUTTLE** (naming may be finalized in WBP authoring) activates the existing unit
manifest Confirm. No free world placement for normal unit orders.

### BUILDINGS

Show orbital buildings available for purchase.

- Do **not** include Wall Package (that is DEFENSE).
- Do **not** include MainBase (initial-only).

**LMB on a building icon** replaces the panel with a selected-item launch state:

- mostly empty/clear panel
- selected building identity/icon may remain visible
- prominent **LAUNCH** centered
- small **BACK** in a corner

**BACK:** return to the Building list. No spend. No READY consume.

**LAUNCH** is a UX shortcut across the existing two backend stages. It does **not** collapse
or remove READY inventory authority:

Purchase → spend Orbital Ferronite exactly once → building becomes READY → immediately enter
the current building deployment ghost using that READY item

No second spend on placement. If placement is canceled with RMB / Esc: no second spend; READY
item remains owned and can be deployed later via existing READY inventory behavior.

Do not invent a new building-spawn RPC. Use existing Purchase + READY + Deploy contracts.

### DEFENSE

Show defensive orbital purchases. Current items at minimum:

- Defensive Turret
- Wall Package

Wall-mounted Turret may appear here when its placement/support workflow is production-ready.
Do not invent other defenses.

**Defensive Turret** uses the same selected-item launch pattern as BUILDINGS (BACK / LAUNCH).
LAUNCH preserves Purchase → READY → immediately enter current building placement ghost.
Placement remains server-authoritative. Future foundation requirements still apply when the
Terrain/Foundation stage implements them.

**Wall Package** uses the same selected-item launch presentation, but LAUNCH behavior is
**different** from a READY building.

Existing Wall Package authority remains canonical:

Buy Wall Package → spend full package cost once → one DropPod to owning MainBase Unit Drop Zone
→ package adds wall segment stock to MainBase → **NO** placement mode → **NO** READY building
inventory → **NO** wall actor spawns from the pod

Exact current rules:

- package = 5 wall segments
- MainBase wall stock capacity = 5
- can buy at stock 0..4 only when no package is already in flight
- full package price; never prorated
- arrival accepts `min(5, free capacity)`; excess wasted; no refund
- stock 5 rejects purchase
- pending package rejects another purchase

Message Strip examples: Wall stock full; Wall delivery already pending; Not enough Orbital Ferronite.

Do **not** change Wall gameplay semantics.

### Foundation Slab package

Foundation Slab package already exists as future orbital-flow canon.

Do **not** force it into Units / Buildings / Defense in this pass.
Its production HUD category/placement remains **TBD**.
Do not omit its existence from orbital architecture docs.
Do not pretend it is already implemented.

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
- Orbital flows (unchanged authority): [`Docs/GDD/10_Orbital_Delivery.md`](../../GDD/10_Orbital_Delivery.md)
- Engineering binding/layout: [`Docs/TDD/12_UI_Architecture.md`](../../TDD/12_UI_Architecture.md)
- Orbital engineering: [`Docs/TDD/14_Orbital_Delivery.md`](../../TDD/14_Orbital_Delivery.md)
- Selection surface mapping: [`Docs/TDD/04_RTS_Selection_And_Commands.md`](../../TDD/04_RTS_Selection_And_Commands.md)
