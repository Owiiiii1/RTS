# GP-S16 Phase C — Control Groups Input
(Assign / append / recall / append-recall via Enhanced Input)

## Status
**Status: PHASE_C_ANALYSIS_READY_IMPLEMENTATION_PENDING**

Phase B (B2a + B2b) **complete** and merged.
This document is the **next minimal GP-S16 checkpoint** after Phase B.
**Implementation not started.**

Parent GP-S16 selection status:
**`PHASE_C_ANALYSIS_READY_IMPLEMENTATION_PENDING`**

GP-S16 overall remains **NOT DONE**.
Do **not** start GP-S17 or full GP-S18.

---

## Why this is next

TDD/13 and `GP-S16_Selection_Component.md` define GP-S16 as:

`SelectedUnits`, `InspectedTarget`, **marquee**, **control groups**.

| Slice | State |
| --- | --- |
| Phase A shell | **Done** (includes control-group **containers**) |
| Phase B1 mutation API | **Done** |
| Prerequisites (UnitBase / TeamId) | **Done** |
| B2a click / inspect | **Done** |
| B2b marquee | **Done** |
| Control-group **input wiring** | **Not started** — APIs exist, PC has **zero** control-group binds |
| Double-tap camera focus | **Deferred** (no `FocusOnLocation` on CameraPawn) |
| Production highlight | **Deferred** (GP-S18b / later UI) |
| Temporary debug boxes | Validation-only; **not** production; keep until highlight |

Container APIs already on `UGP_SelectionComponent`:

- `AssignControlGroup`
- `AppendToControlGroup`
- `RecallControlGroup`
- `AppendControlGroupToSelection`
- `ClearControlGroup`
- `LastGroupRecallTimes` updated on recall (for future double-tap)

`AGP_PlayerController` does **not** call any of these.

Canonical key contract (locked in parent GP-S16 / TDD/04):

| Input | Action |
| --- | --- |
| `Ctrl+1..9` | Assign current selection → group N |
| `Ctrl+Shift+N` | Append current selection → group N |
| `1..9` | Recall group N (replace selection) |
| `Shift+N` | Append group N into current selection |

---

## Proposed subphase split

| Subphase | Scope | This checkpoint? |
| --- | --- | --- |
| **C** (this) | Wire control-group keys to existing SelectionComponent APIs; local-only; operator PIE | **Yes — next** |
| **C2** (later) | Double-tap recall → camera focus request when `FocusOnLocation` exists | No |
| Esc clear binding | Optional tiny follow-up; ground clear already works | Out of C unless OD expands |
| Production highlight + remove debug boxes | Later UI / GP-S18b | No |
| Building mix / OnDeath / FoW / selection cycling | Full GP-S18 / FoW | No |

Do **not** implement C2, highlight, or Esc in Phase C unless a separate reviewed expansion is assigned.

---

## Phase C goal

Minimal production-safe control-group **input integration**:

- Local controller only
- Reuse existing SelectionComponent control-group mutators
- Prefer digit chords on `IMC_GP_Selection` (or minimal IA set) — **avoid** 9× speculative InputAction explosion unless proven necessary (parent OD)
- Do **not** implement double-tap camera focus
- Do **not** change selection click/marquee semantics
- Keep temporary green/yellow debug boxes (they will reflect recall/assign via existing Tick visualization)

---

## Dependencies (satisfied)

- Phase A control-group storage + Phase B1/A APIs
- B2a/B2b selection fill (groups need a selection to assign)
- TeamId / UnitBase (already used by selection)
- `IMC_GP_Selection` + `IA_Select` exist; camera IMC must remain untouched

---

## Likely affected files / assets (implementation later)

| Path | Likely change |
| --- | --- |
| `GPPlayerController.h/.cpp` | Bind/handle control-group input; call SelectionComponent APIs |
| `IMC_GP_Selection.uasset` | Digit / chord mappings (preferred) |
| Optional new IA assets under `/Game/GrimProtocol/Input/Selection/` | Only if chord-in-IMC approach fails; avoid 9× IA unless required |
| Docs (parent + this + AI log) | Status / validation |

**Prefer not to change:** `GPSelectionComponent` (API sufficient), UnitBase, GameMode, CameraPawn, Build.cs, maps, config, `.uproject`.

Possible small SelectionComponent addition only if proven necessary (e.g. read-only `GetLastGroupRecallTime` for future C2) — **out of scope for C** unless double-tap is explicitly pulled in (it is not).

---

## Input design (analysis lock)

| Preference | Detail |
| --- | --- |
| Context | Extend `IMC_GP_Selection` (priority **110** unchanged) |
| Digits | `One`..`Nine` → groups 1..9 |
| Modifiers | Ctrl / Shift via mapping chords or PC key-state (match B2a style where safer) |
| Ctrl wins over conflicting chords | Follow Enhanced Input chord resolution; document exact mapping table at implementation |
| Camera | Must not steal digit keys from selection when selection IMC active; do not edit `IMC_GP_Camera` |
| ClearControlGroup | No dedicated key in TDD/04 table — **optional**; not required for C DONE |

Exact asset creation vs pure IMC key mappings is an **implementation OD** within this phase; analysis prefers IMC digit mappings first.

---

## Behavior locks

| Case | Behavior |
| --- | --- |
| Assign (`Ctrl+N`) | `AssignControlGroup(N)` — copy current selection into group |
| Append to group (`Ctrl+Shift+N`) | `AppendToControlGroup(N)` |
| Recall (`N`) | `RecallControlGroup(N)` — replace selection; prune/cap in component |
| Append recall (`Shift+N`) | `AppendControlGroupToSelection(N)` |
| Empty group recall | Selection becomes empty (component prune); legitimate |
| Empty current assign | Group becomes empty copy — allowed |
| Local-only | Non-local controllers: no binds / no mutations |
| Inspect on recall | Prefer clear inspect when recall/append-recall mutates selection (mirror friendly select); document at implementation |
| Double-tap focus | **Not in Phase C** |
| Cap 24 | SelectionComponent only |

---

## Multiplayer policy

- Bindings and handling: **local controller only**
- Control groups: local, non-replicated, non-persistent (already true)
- No RPC
- Host and remote client: independent groups
- One client must not mutate another’s groups/selection

---

## Operator validation plan (after implementation)

- Assign group 1 from multi-select; clear; recall `1` restores same units (green boxes)
- `Ctrl+Shift+N` appends into group; recall shows union (cap 24)
- `Shift+N` appends group into current selection without wiping prior (dedupe)
- Empty group recall → empty selection
- Digits do not break camera pan/zoom/rotate
- Click / marquee / inspect regression: none
- Standalone + 2P listen-server isolation
- No replication / RPC warnings
- No map save; no production highlight assets

---

## Temporary debug boxes decision

| Decision | Value |
| --- | --- |
| Remain for Phase C? | **Yes** — useful for operator validation of recall/assign |
| Part of Phase C deliverable? | **No** — already present; do not expand |
| When remove? | When production selection highlight lands (later UI / GP-S18b) |
| Production? | **No** — temporary developer validation visualization only |

---

## Strict exclusions (Phase C)

- No GP-S17 commands / movement / attack
- No full GP-S18 (highlight MID, OnDeath, building mix policy)
- No FoW
- No double-tap camera focus (C2)
- No production highlight materials
- No removal of temporary debug boxes in C
- No marquee algorithm changes
- No TeamId / GameMode changes
- No map/config/`.uproject` changes unless proven necessary for input assets only

---

## Implementation checklist (future reviewed task)

1. Design digit/chord mappings on `IMC_GP_Selection` (minimal IA surface).
2. PC: local-only handlers → `Assign` / `AppendToGroup` / `Recall` / `AppendControlGroupToSelection`.
3. Preserve B2a/B2b and camera IMC.
4. Inspect-clear policy on recall paths (document + implement consistently with B2a).
5. Builds + operator validation matrix above.
6. Docs → `PHASE_C_DONE` when validated; parent advances to next remaining GP-S16 item or C2.

---

## Stop condition

**PHASE_C_ANALYSIS_READY_IMPLEMENTATION_PENDING.**
Documentation-only determination checkpoint.
Do **not** implement control-group input in this pass.
Do **not** start GP-S17 / full GP-S18.
Do **not** mark GP-S16 DONE.
