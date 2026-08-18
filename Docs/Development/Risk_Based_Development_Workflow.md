# Risk-Based Development Workflow

Canonical process for choosing tests, builds, and Cursor prompt scope on future slices.

This document does **not** replace `GRIM_PROTOCOL_START_RULES.md` (roles, one-stage rule, factual review) or `Git_Workflow.md` (branching / commits). It is the source of truth for **validation selection**.

Risk-based workflow means **adaptive validation**, not weaker validation.

---

## 1. Quality principle

Optimize for, in this order of intent:

- correctness
- regression safety
- operator validation
- time
- token usage

Reducing tests is **not** the goal by itself.

Run the **smallest** test/build set that still gives sufficient confidence for the **actual change risk**.

---

## 2. Implementation candidate

For a normal bounded slice:

**Required**

- slice-specific contract test
- directly affected regression tests only
- `GPEditor Win64 Development` + UHT when C++ changed
- operator validation when observable gameplay / editor behavior changed

Do **not** automatically run the entire project regression suite.

Typical affected regressions: **3–5 tests**, selected by touched systems and invariants.

---

## 3. Finalization

After operator **PASS**:

**Required**

- rerun the slice-specific contract
- rerun only high-risk affected regressions
- verify the factual git diff
- update report / docs

### Build rules

If C++ changed:

- `GPEditor Win64 Development` + UHT
- `GP Win64 Development`
- `GP Win64 Shipping`

Do **not** rebuild the same target multiple times in the same stage without a code change.

If only docs changed:

- no Unreal tests
- no Unreal builds

If only content / DataAsset / Blueprint operator work changed and **no committed C++** changed:

- use only the relevant validation needed for that change

---

## 4. Full regression suite

Run a broad / full regression suite **only when justified**, including:

- cross-cutting architecture changes
- changes to shared authority / state infrastructure
- replication / GAS / framework-level changes
- major refactors
- milestone stabilization
- release candidate
- a targeted regression that exposes unexpected cross-system breakage

Do **not** run the full suite by default for every bounded slice.

---

## 5. Risk-based test selection

Before giving Cursor a task, identify:

- directly modified systems
- invariants that could regress
- downstream systems **actually coupled** to those changes

Choose tests from that dependency surface.

### Examples

**Combat-only change**

- slice combat contract
- attack / LOS / auto-acquire regressions as relevant
- no resource / orbital suite unless shared code was touched

**Economy-only narrow change**

- economy contract
- affected resource / orbital / unit-cap tests
- no combat / movement suite unless shared infrastructure changed

**Docs-only**

- no tests
- no builds

---

## 6. Cursor token / time economy

Cursor prompts should contain:

- scope
- required invariants
- relevant factual baseline
- required tests / builds
- safety constraints

Do **not** repeat the full project architecture when it is not needed.

Cursor should not reread unrelated large document sets / files unless necessary.

Reports (`Docs/Development/Cursor_Work_Report.md`) must be concise but factual:

- status
- branch / base / head
- implementation summary
- tests actually run
- builds actually run
- changed files
- operator status
- merge state

Do **not** duplicate long design documentation inside `Cursor_Work_Report.md`.

---

## 7. Quality escalation

Escalate validation scope when:

- a contract fails unexpectedly
- shared code changed
- the factual diff reveals a larger blast radius
- operator behavior differs from expected
- there is uncertainty about the regression surface

Then run additional relevant regressions, or the full suite if justified.

---

## 8. Factual review remains mandatory

This rule does **not** change.

| Gate | Required inspection |
| --- | --- |
| Before operator test | report + factual GitHub diff |
| Before merge authorization | final report + factual GitHub diff / head |
| After merge | verify remote `main` |

---

## 9. Stage summary

| Stage | Tests | Builds |
| --- | --- | --- |
| Implementation candidate (bounded slice, C++ changed) | slice contract + 3–5 affected regressions | GPEditor + UHT |
| Finalization after operator PASS (C++ changed) | slice contract + high-risk affected regressions | GPEditor + UHT, GP Development, GP Shipping — once per unchanged code |
| Docs-only | none | none |
| Operator content only, no committed C++ | only relevant validation | none unless that change requires it |
| Escalation / cross-cutting / RC | additional or full suite | as justified by risk |
