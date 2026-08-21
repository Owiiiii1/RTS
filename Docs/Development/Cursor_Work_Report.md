# Cursor Work Report — Production HUD Layout + MainBase Procurement

## Status

**PRODUCTION_HUD_LAYOUT_AND_MAINBASE_PROCUREMENT_DOCUMENTATION_READY_FOR_REVIEW**

## Branch / base / head

- Branch: `docs/gp-production-hud-layout-spec`
- Base: `origin/main` @ `317ce3f0367111081e3a8987c8ac8beebfbd6310`
- Previous layout commit on this branch: `6998b00d305b2769d705f66ddb535ccc2b8be6de`
- Head: `2af66aac2ff0e54642fc8c288cfe71ae17540575`
- **NOT MERGED**

## Previous HUD layout status

The approved two-bar × three-block HUD IA remains canonical (Threat+Score / Match Timer /
Planet-Orbit-Cap; square minimap placeholder / Selection-Info / Context Action Grid).
The coarse pre-2026-08-21 placement remains **SUPERSEDED**.

This amendment adds owner-approved MainBase procurement into the bottom-right panel.
A global `O` / standalone Order Menu is **SUPERSEDED** as the production HUD path.
TEMP HUD debug procurement remains temporary scaffolding. Backend ownership is unchanged.

## MainBase PURCHASE entry

Orbital procurement is not a permanent global HUD panel.

When MainBase is the current single selected building, the bottom-right Building Action Grid
shows **PURCHASE**. Clicking PURCHASE replaces that panel with three large category buttons:
UNITS, BUILDINGS, DEFENSE. Navigation stays inside the same bottom-right panel.
Only MainBase owns PURCHASE. Other buildings are not procurement sources.

Keyboard shortcut may later convenience-open the same panel; select MainBase + PURCHASE is the
canonical visible entry.

## Message Strip

Small horizontal strip directly **above** the bottom-right Context Action / Procurement block.
Short contextual procurement/action feedback. Not a global toast system. Default may be empty.

Examples: shuttle capacity, not enough Orbital Ferronite, shuttle capacity reached, unit cap
reached, building/placement unavailable, wall stock full, delivery already pending.

## Units category behavior

Uses the current factual orbital unit catalog and existing backend:

manifest → validate Orbital Ferronite → validate transport slots → validate player unit cap
→ Confirm → one DropPod to MainBase Unit Drop Zone

No free world placement for normal unit orders.

## LMB add / RMB remove quantity behavior

- LMB on a unit icon: add one of that type to the pending shuttle manifest; show quantity on
  the icon; repeated LMB increments while valid.
- RMB: remove one; decrement quantity; hide the marker at zero.
- UI must not create invalid authoritative state. Local prevent of obvious invalid adds is
  allowed; final validation remains server-authoritative.

## Shuttle slot messaging

Before the first unit is added, Message Strip should show shuttle capacity
(`PodTransportSlotCapacity`), e.g. `Shuttle. Capacity — X slots`.
Do not confuse shuttle slots with `CurrentUnits / MaxUnits`.
As the manifest changes: `Shuttle: 3 / 4 slots`, capacity reached, not enough Orbital Ferronite,
unit cap reached. Per-unit transport-slot cost remains canonical.

LAUNCH SHUTTLE (name may be finalized in WBP authoring) uses existing unit-manifest Confirm.

## Buildings category and Launch/Back flow

BUILDINGS lists orbital buildings. No Wall Package. No MainBase.

LMB on an icon → selected-item launch state: mostly empty panel, selected identity/icon may
remain, centered **LAUNCH**, corner **BACK**.

BACK: return to list. No spend. No READY consume.

## Exact preservation of Purchase → READY → Deploy backend

LAUNCH is a UX shortcut, not a collapsed authority model:

Purchase → spend Orbital Ferronite exactly once → READY → immediately enter current building
deployment ghost using that READY item

No second spend on placement. RMB / Esc cancel: READY remains owned and can be deployed later
via existing READY inventory. No new building-spawn RPC.

## Defense category

DEFENSE shows Defensive Turret and Wall Package. Wall-mounted Turret may appear when its
placement/support workflow is production-ready. Do not invent other defenses.
Foundation Slab package HUD category remains **TBD** (orbital flow still documented).

## Defensive Turret behavior

Same selected-item launch pattern as BUILDINGS. LAUNCH preserves Purchase → READY →
immediately enter current building placement ghost. Placement remains server-authoritative.
Future foundation requirements still apply when Terrain/Foundation implements them.

## Exact Wall Package behavior

Same selected-item launch presentation, different LAUNCH:

Buy Wall Package → spend full package cost once → one DropPod to owning MainBase Unit Drop Zone
→ add wall segment stock to MainBase → NO placement mode → NO READY inventory → NO wall actor
from the pod

Canonical rules: package = 5; MainBase stock cap = 5; buy at 0..4 only when none in flight;
full price never prorated; arrival `min(5, free capacity)`; excess wasted; no refund;
stock 5 rejects; pending package rejects another purchase.

## Foundation category remains TBD

Do not force Foundation Slab package into Units / Buildings / Defense in this pass.
Do not omit it from orbital architecture docs. Do not claim it is implemented.

## Right-panel state machine

NO/GENERIC SELECTION → unit/building commands
MAINBASE SELECTED → Building Actions → PURCHASE
PURCHASE → UNITS | BUILDINGS | DEFENSE
UNITS → manifest LMB/RMB/quantity → launch current manifest
BUILDINGS → list → selected-item launch → BACK or Purchase+READY+ghost
DEFENSE → turret (READY+ghost) or wall (Buy Wall Package → MainBase stock)

Message Strip stays attached above this block. Bottom-center stays on MainBase info.

## Exact changed files

- `Docs/Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/GDD/10_Orbital_Delivery.md`
- `Docs/GDD/05_Buildings.md`
- `Docs/GDD/README.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/TDD/14_Orbital_Delivery.md`
- `Docs/TDD/06_Building_Architecture.md`
- `Docs/TDD/04_RTS_Selection_And_Commands.md`
- `Docs/TDD/README.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/Cursor_Work_Report.md`

## Documentation-only confirmation

No runtime, Content, Config, maps, Blueprints, DataAssets, or Tools files are part of this slice.
No Unreal tests or builds were run.

## NOT MERGED
