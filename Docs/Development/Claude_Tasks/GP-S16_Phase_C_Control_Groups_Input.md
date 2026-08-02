# GP-S16 Phase C — Control Groups Input
(Assign / append / recall / append-recall via Enhanced Input)

## Status
**Status: PHASE_C_DONE**

Phase B (B2a + B2b) **complete** and merged.
Phase C control-groups input **implemented and operator-validated**.
C2 double-tap camera focus **deferred** (`FocusOnLocation` absent).
Production highlight / debug-box cleanup **deferred**.

Parent GP-S16 selection status:
**`PHASE_C_DONE_NEXT_CHECKPOINT_PENDING`**

GP-S16 overall remains **NOT DONE** (next checkpoint is a separate reviewed decision).
Do **not** start GP-S17 or full GP-S18.

---

## Implemented

| Item | Detail |
| --- | --- |
| Asset | `/Game/GrimProtocol/Input/Selection/IA_ControlGroup` — **Axis1D** |
| IMC | `IMC_GP_Selection` **`DefaultKeyMappings`**: `IA_Select` LMB + One..Nine → `IA_ControlGroup` with **Scalar** 1..9 |
| Modifiers | Ctrl / Shift via PC `IsControlModifierDown` / `IsShiftModifierDown` |
| Binding | `ETriggerEvent::Started` only; independent bind guards |
| Soft path | `/Game/GrimProtocol/Input/Selection/IA_ControlGroup.IA_ControlGroup` |
| Priority | Selection IMC **110**; Camera **100** unchanged |
| SelectionComponent | **Unchanged** |

### Modifier precedence

1. Ctrl+Shift → `AppendToControlGroup`
2. Ctrl → `AssignControlGroup`
3. Shift → `AppendControlGroupToSelection`
4. None → `RecallControlGroup`

### Inspect policy

| Op | Inspect |
| --- | --- |
| Assign / AppendToGroup | Unchanged |
| Recall | Clear inspected before recall |
| AppendRecall | Clear inspected only if selection identity changed |

### Logging

One-shot per Started operation:

```text
GP ControlGroup: Group=N Operation=Assign|AppendToGroup|Recall|AppendRecall Before=X After=Y
```

Missing-asset Error allowed once at load. No Tick / raw per-press spam after finalize.

---

## Remediation note (operator-validated)

| Item | Detail |
| --- | --- |
| Initial failure | No `GP ControlGroup` events; click/marquee OK |
| Root cause | Automation populated deprecated IMC `mappings`; UE 5.8 runtime reads **`DefaultKeyMappings`** |
| Fix | Rewrite saved `DefaultKeyMappings` (LMB + One..Nine / Scalar); verified after reload |
| Outcome | Operator validation **passed** after remediation |

---

## Operator validation (passed)

| Check | Result |
| --- | --- |
| Ctrl+N Assign | **PASS** |
| N Recall | **PASS** |
| Ctrl+Shift+N AppendToGroup | **PASS** |
| Shift+N AppendRecall | **PASS** |
| Raw Axis1D group values | **PASS** |
| Hold digit no spam | **PASS** |
| Click / marquee regression | **NONE** |
| IA_ControlGroup load | **PASS** |
| DefaultKeyMappings One..Nine | **PASS** |
| Local-only / no RPC | **PASS** |

---

## Explicitly not in Phase C

- C2 double-tap camera focus
- Esc clear
- Production highlight / remove temporary debug boxes
- GP-S17 / full GP-S18

---

## Stop condition

**PHASE_C_DONE.**
Phase C complete and operator-validated.
Do **not** mark entire GP-S16 DONE automatically.
Do **not** start C2 / GP-S17 / full GP-S18 from this finalize.
Await separate reviewed decision for the next GP-S16 checkpoint.
