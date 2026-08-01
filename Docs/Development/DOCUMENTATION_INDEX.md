# Documentation Index

Единый индекс документации Grim Protocol после DOCS-001 (2026-08-01).

Этот файл — **source of truth по статусу документации и NEXT task**. При конфликте между устаревшим cursor в task README и этим индексом — приоритет у `DOCUMENTATION_INDEX.md` + `AI_Project_Log.md`, пока cursor не синхронизирован.

---

## Sources of Truth

| Domain | Canonical path | Notes |
| --- | --- | --- |
| **Architecture** | `Docs/Architecture_Decisions/` (ADR wins) + `Docs/TDD/13_Architecture_Proposal.md` | ADR-0009 (Orbital Delivery) overrides pre-pivot building/production examples. |
| **Gameplay** | `Docs/GDD/` + `Docs/Game_Pitch.md` | Prefer `02`, `06`, `08`, `10`, `11`, `12` over stale fragments in `00` / `09` / `Out_Of_Scope`. |
| **Workflow / process** | `GRIM_PROTOCOL_START_RULES.md` (root + `Docs/Development/`) + `/CONTRIBUTING.md` + `/STYLE.md` + `Docs/Development/Git_Workflow.md` + `Coding_Rules.md` | One stage at a time; operator validates in UE Editor. |
| **Current status / NEXT** | `Docs/Development/AI_Project_Log.md` + **this file** + `Docs/Development/Claude_Tasks/README.md` (cursor) | Cursor must match disk/`AI_Project_Log`. |
| **Engineering rules** | `/CONTRIBUTING.md`, `/STYLE.md`, `Docs/Development/Coding_Rules.md` | Review-blocking rules. |
| **Implementation map** | `Docs/TDD/13_Architecture_Proposal.md` | Slice order S01… after foundation. |

### Supporting indexes

- Docs landing: [`Docs/README.md`](../README.md)
- GDD index: [`Docs/GDD/README.md`](../GDD/README.md)
- TDD index: [`Docs/TDD/README.md`](../TDD/README.md)
- ADR index: [`Docs/Architecture_Decisions/README.md`](../Architecture_Decisions/README.md)
- Development ops: [`Docs/Development/README.md`](README.md)
- Task backlog: [`Claude_Task_Backlog.md`](Claude_Task_Backlog.md)
- Work plan: [`Claude_Work_Plan.md`](Claude_Work_Plan.md)

---

## Current Project Baseline

| Fact | Value |
| --- | --- |
| Engine | **Unreal Engine 5.8.1** |
| Code baseline | SETUP-001 + DOCS-001 + GP-S01 … **GP-S09 DONE** |
| Runtime modules on disk | `GP` (primary) + `GPRuntime` (`AGP_GameState`, `AGP_GameMode`, `AGP_PlayerController`, `AGP_PlayerState`) + `GPGASRuntime` (tags + AttributeSets + ASC + Damage MMC) + `GPUIRuntime` |
| Last closed stage | **GP-S09** |
| Current stage | **none** |

---

## Current NEXT Task

**Current:** none (GP-S09 closed).

**NEXT:** **GP-S10** — `UGP_MatchAssetLoader` (PreloadForMatch + Resolve API), per [`TDD/13_Architecture_Proposal`](../TDD/13_Architecture_Proposal.md) §Implementation Order (Slice 2 — Match Flow + Asset Loader).

Task file **not created**. Do **not** start GP-S10 until explicitly assigned.

### Foundation / Match Flow sequence

1. GP-S01 Module Scaffolds — **DONE**
2. GP-S02 Native Gameplay Tags — **DONE**
3. GP-S03 Attribute Sets — **DONE**
4. GP-S04 AbilitySystemComponent Subclass — **DONE**
5. GP-S05 Damage Calculation MMC — **DONE**
6. GP-S06 AGP_GameState (Match State and Timer) — **DONE**
7. GP-S07 AGP_GameMode (PostLogin, Match Countdown, EndMatch Hook) — **DONE**
8. GP-S08 AGP_PlayerController scaffold — **DONE**
9. GP-S09 AGP_PlayerState (+ASC + AttributeSet) — **DONE**
10. GP-S10 UGP_MatchAssetLoader — not assigned (task file not materialized)

---

## Document Currency

### Active / canonical

| Area | Status |
| --- | --- |
| `Docs/GDD/` (post-pivot core: 02, 04–08, 10–12, Lore, First_Playable, Backlog) | Active |
| `Docs/TDD/` (esp. 03, 05–07 CANONICAL parts, 09–15, 13) | Active |
| `Docs/Architecture_Decisions/ADR_0001`…`0006`, `0008`, `0009` | Active (Accepted) |
| `ADR_0007` Building-As-Pawn | Active but **Draft** — pattern stands; examples superseded by ADR-0009 |
| Root `README.md`, `CONTRIBUTING.md`, `STYLE.md` | Active |
| `GRIM_PROTOCOL_START_RULES.md` | Active process SoT |
| `Docs/Development/*` ops + Claude task specs | Active |
| `Docs/Game_Pitch.md` | Active onboarding |

### Stale / needs cleanup (still in active tree)

| Document | Issue |
| --- | --- |
| `Docs/GDD/00_Project_Overview.md` | Pre-pivot SWARM/success-criteria wording leftovers |
| `Docs/GDD/09_UI_UX.md` | Build command / construction UX leftovers |
| `Docs/GDD/Out_Of_Scope.md` | Still lists FoW as MVP exclusion; FoW is MVP (GDD/11, TDD/15) |
| `Docs/GDD/01_Game_Pillars.md` | Residual “Build” feedback wording |
| `Docs/TDD/00_Technical_Overview.md` | Common UI marked deferred; TDD/12 requires it |
| `Docs/TDD/01_Module_Architecture.md` | Still mentions Production/Construction ownership |
| `Docs/TDD/02_GAS_Architecture.md` | Header still shows old Resource pool attrs; ASC subclass timing inconsistent |
| `Docs/TDD/07_Resource_Architecture.md` | Dual CANONICAL + SUPERSEDED layers (intentional but noisy) |
| `CONTRIBUTING.md` Component list | Still lists Production/Construction components |
| `GP-0304_Barracks.md` | Filename legacy; SUPERSEDED → Logistics Hub rewrite = GP-0802 |
| `grim_protocol_docs_audit_v2.md` | Historical audit; many Priority items already addressed — do not treat as live contradiction list |

### Deprecated / archive

| Path | Status |
| --- | --- |
| `Docs/Archive/*` | Reference-only. Not source of truth. Do not update for UE version. |
| `Docs/Archive/LEGACY_RN_*` | Legacy RN/RNC project rules |
| `Docs/Archive/PrePivot_Building_Architecture.md` | Pre-orbital architecture archive |
| `Docs/grim-protocol-docs/` (if reimported) | **Deprecated duplicate import** — nested mirror with old Source tree. Never SoT. Delete or keep only under Archive; do not edit in parallel with `Docs/`. |

---

## Known Conflicts (DOCS-001)

1. **Engine version (resolved in active rules):** process/start rules + SETUP-001 → **5.8.1**. Older gameplay/tech overviews said 5.7 — updated in DOCS-001 for active docs.
2. **Task cursor vs disk:** Claude_Tasks claimed S01–S03 DONE / NEXT S04; disk has blank `GP` only → corrected to **NEXT = GP-S01**.
3. **Economy / SWARM:** Canon = `FerroniteThreatValue` from planetary stock at base (`GDD/06`, ADR-0009). Conflict: `GDD/00` still implies SWARM scales with shipped Ferronite / time-escalation produce loop.
4. **FoW MVP:** In MVP per GDD/11 + TDD/15 + Docs/README pivots. Conflict: `GDD/Out_Of_Scope.md` still excludes full FoW from MVP.
5. **Common UI enablement timing:** Mandatory per TDD/12 + CONTRIBUTING/README. Residual: TDD/00 still says deferred in places; `GP.uproject` lacks CommonUI until GP-S01 (expected).
6. **CommonGame / Lyra UI stack (RESOLVED 2026-08-01):** Active docs previously listed `CommonGame` (+ often a separate `CommonInput` plugin entry) as mandatory. **Resolution:** blank UE 5.8.1 uses stock plugins `CommonUI` + `ModelViewViewModel` only; `CommonInput` is a module dependency provided with Common UI; `EnhancedInput` remains for gameplay IMC. `CommonGame` / `CommonUser` / Lyra foundation are **not** required and must not be imported (ADR-0005). Updated: GP-S01, TDD/12, TDD/13 (S47), GP-0401.
7. **Local production vs orbital:** ADR-0009 / GDD orbital docs win. Residual Production/Construction references remain in CONTRIBUTING component list, TDD/01, ADR-0003 examples, GDD/09.
8. **Git LFS:** `.gitattributes` already tracks `*.uasset` / `*.umap` via LFS; `Git_Workflow.md` still says LFS is deferred — process docs should be reconciled later.
9. **Duplicate start rules:** Root `GRIM_PROTOCOL_START_RULES.md` and `Docs/Development/GRIM_PROTOCOL_START_RULES.md` must stay in sync.

---

## UE Version Policy

Official baseline: **Unreal Engine 5.8.1**.

Active docs must not prescribe 5.7. Archive/legacy RN docs may keep historical 5.7 references.

---

## Recommended Reading Order (post-import)

1. `Docs/Game_Pitch.md`
2. `Docs/Development/DOCUMENTATION_INDEX.md` (this file)
3. `Docs/Development/AI_Project_Log.md`
4. `GRIM_PROTOCOL_START_RULES.md`
5. `/CONTRIBUTING.md` + `/STYLE.md`
6. ADR-0009 → GDD/02 → TDD/13
7. Current NEXT task file only

---

## Stop

SETUP-001, DOCS-001, GP-S01…GP-S09 are closed. Current stage: **none**. NEXT per TDD/13 = **GP-S10** (task file not materialized; not started).
