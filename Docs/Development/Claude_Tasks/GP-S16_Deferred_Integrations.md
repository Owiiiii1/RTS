# GP-S16 — Deferred Integrations
(Post-completion ownership map; no implementation in this pass)

## Status
**Reference document** for items deferred when GP-S16 closed as
`DONE_WITH_DEFERRED_INTEGRATIONS`.

Common rules for every item below:

- **not implemented** in closed GP-S16 core;
- **not silently included** in GP-S16 DONE;
- has an explicit **future owner**;
- **does not block GP-S17** unless a later dependency review says otherwise.

---

## C2 camera focus

| Field | Detail |
| --- | --- |
| Scope | Double-tap group recall within **0.4 s** requests camera focus on selection centroid |
| Dependency | Requires confirmed CameraPawn focus API (`FocusOnLocation` or equivalent) |
| Status | **Not implemented**; not part of GP-S16 DONE |
| Owner | Camera follow-up / **GP-S16-C2** |
| Blocks GP-S17? | **No** |

---

## Production highlight

| Field | Detail |
| --- | --- |
| Scope | Production material / MID (or equivalent visual) selection + inspect presentation |
| Replaces | Temporary green (selected) / yellow (inspected) `DrawDebugBox` visualization |
| Status | **Not implemented**; not part of GP-S16 DONE |
| Owner | **GP-S18b** or dedicated visual-selection slice |
| Debug boxes | **Remain** until this replacement is implemented |
| Blocks GP-S17? | **No** |

---

## UnitBase / definitions integration

| Field | Detail |
| --- | --- |
| Scope | `OnDeath` prune/subscription; building single-selection; unit/building mix rules; same-definition double-click; closest-24 overflow refinement |
| Status | **Not implemented** as full production contracts; not silently included in GP-S16 DONE |
| Owner | Full UnitBase / definitions integration |
| Notes | Cap 24 already enforced; closest-to-cursor is a refinement. Interim team/capability filtering already exists for current concrete units. |
| Blocks GP-S17? | **No** (unless future dependency review says otherwise) |

---

## FoW

| Field | Detail |
| --- | --- |
| Scope | Selection / inspect / marquee filtering by FoW visibility |
| Status | **Not implemented**; not part of GP-S16 DONE |
| Owner | FoW slice (TDD/15 area) |
| Blocks GP-S17? | **No** |

---

## Optional UX

| Field | Detail |
| --- | --- |
| Scope | Esc → clear selection + inspect (ground / non-unit clear already works) |
| Status | **Not implemented**; optional; not part of GP-S16 DONE |
| Owner | Optional UX follow-up |
| Blocks GP-S17? | **No** |

---

## Out of MVP

| Field | Detail |
| --- | --- |
| Scope | Control-group persistence across matches |
| Status | **Not implemented**; out of MVP; not part of GP-S16 DONE |
| Owner | Post-MVP persistence (if ever) |
| Blocks GP-S17? | **No** |

---

## Ownership summary

| Deferred item | Future owner | Blocks GP-S17? |
| --- | --- | --- |
| C2 double-tap camera focus | Camera follow-up / GP-S16-C2 | No |
| Production highlight + debug-box removal | GP-S18b or visual-selection slice | No |
| OnDeath / building mix / double-click / closest-24 | Full UnitBase / definitions integration | No* |
| FoW select/inspect filter | FoW slice | No |
| Esc clear | Optional UX follow-up | No |
| Control-group persistence | Out of MVP | No |

\* Unless a later dependency review says otherwise.

---

## Temporary debug boxes

Local `DrawDebugBox` (selected green / inspected yellow) **remains** as developer visualization until production highlight replaces it.
Do **not** remove as part of GP-S16 closure.

---

## Stop condition

Documentation-only ownership map.
Do **not** implement any deferred item from this file without a separate assignment.
Do **not** start GP-S17 or full GP-S18 from this document.
