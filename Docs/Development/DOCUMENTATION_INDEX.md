# Documentation Index

Единый индекс документации Grim Protocol после DOCS-001 (2026-08-01).

Этот файл — **source of truth по статусу документации и NEXT task**. При конфликте между устаревшим cursor в task README и этим индексом — приоритет у `DOCUMENTATION_INDEX.md` + `AI_Project_Log.md`, пока cursor не синхронизирован.

---

## Sources of Truth

| Domain | Canonical path | Notes |
| --- | --- | --- |
| **Architecture** | `Docs/Architecture_Decisions/` (ADR wins) + `Docs/TDD/13_Architecture_Proposal.md` | ADR-0009 overrides pre-pivot examples; current implementation order/status is reconciled by [`MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`](MVP_Roadmap_Reconciliation_Post_Building_Vitals.md). |
| **Gameplay** | `Docs/GDD/` + `Docs/Game_Pitch.md` | Prefer `02`, `06`, `08`, `10`, `11`, `12` over stale fragments in `00` / `09` / `Out_Of_Scope`. |
| **Workflow / process** | `GRIM_PROTOCOL_START_RULES.md` (root + `Docs/Development/`) + [`Risk_Based_Development_Workflow.md`](Risk_Based_Development_Workflow.md) + `/CONTRIBUTING.md` + `/STYLE.md` + `Docs/Development/Git_Workflow.md` + `Coding_Rules.md` | One stage at a time; operator validates in UE Editor. Test/build selection is risk-based. |
| **Current status / NEXT** | `Docs/Development/AI_Project_Log.md` + **this file** + `Docs/Development/Claude_Tasks/README.md` (cursor) | Cursor must match disk/`AI_Project_Log`. |
| **Engineering rules** | `/CONTRIBUTING.md`, `/STYLE.md`, `Docs/Development/Coding_Rules.md` | Review-blocking rules. |
| **Implementation map** | [`MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`](MVP_Roadmap_Reconciliation_Post_Building_Vitals.md) + `Docs/TDD/13_Architecture_Proposal.md` | Reconciliation owns current capability status/order; TDD/13 owns intended architecture and historical mapping. |

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
| Code baseline | Current `origin/main` baseline: `de718725115ddd636b56092bd6197cf0f7a65950` |
| Runtime modules on disk | `GP` + `GPRuntime` + `GPGASRuntime` + `GPUIRuntime` (+ `GPEditor`) |
| Last closed / merged stage | **Building Vitals / Definition Ownership** on `main` @ `b7e391a636749173c445f7994a41daf3c18ba902` |
| Slice 6 | **Completed** |
| Slice 7 combat reconciliation (S29R) | **DONE / MERGED** |
| Current stage | [`Claude_Tasks/GP-FoW-Runtime-Foundation.md`](Claude_Tasks/GP-FoW-Runtime-Foundation.md) — **FOW_RUNTIME_FOUNDATION_FINALIZED_READY_FOR_MERGE** |

---

## Current NEXT Task

**Closed:** GP-S39E Economy / Logistics Data Ownership — **MERGED / VERIFIED / CLOSED** on `main` @ `75a0e5bd1ce9ca473f216ced36bd8b8970a3005d`.

**Prior closed:** GP-S38D / GP-S37T / GP-S36G / GP-S35B / GP-S34W / TEMP HUD layout / GP-S33C / GP-S33M / GP-S32A / GP-S32R / GP-S31R / GP-S30 / GP-S29R / GP-S30R — **DONE / MERGED**.

**Closed on `main`:** Delivery timing ownership cleanup @ `d2c1abcfcf4fe2f61ae00793294c0cc31919cd65`; Unit payload compatibility cleanup @ `75b13fc193531170eb3d4c1eaf9ee3f736d1d160`; Unit numeric compatibility cleanup @ `47a220b480e455f1cf5dfb6ca0613c13cf760a53`; Dead overlap setting removal @ `967e6ea3a5b81ddc1a2c19c4bfe292f5ef989507`; Settings Visibility Truth @ `f38e803771261c60d865949c693a52a73fbcedb2`; Unit Drop Nested Readiness @ `283297012c1cefe162028a7ba4166c02a81230cc`; Configuration / Data Ownership Audit @ `9c4ef72e44fad28d9922d82e8cded1f5d00a473f`; GP-S42A Wall Package Data + MainBase Wall Inventory; GP-S41M Movement Shortest Yaw; GP-0305R Wall Package canon.

**Current docs reconciliation:** [`MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`](MVP_Roadmap_Reconciliation_Post_Building_Vitals.md) — **MVP_ROADMAP_RECONCILIATION_POST_VITALS_FINALIZED_READY_FOR_MERGE**. Historical S-number order is no longer the execution cursor.

**Exactly one NEXT production capability:** production UI foundation and trusted **FoW presentation**.
Do not jump to SWARM, cleanup, or building redesign.

**Owner decisions:** cleanup phase closed; footprint/geometry cleanup deferred pending building-system redesign; existing orbital building ghost/placement capability is DONE; SWARM is separate from RTS AI Opponent and is the final gameplay implementation stage after a mandatory design review.

**Do not** implement old pod-per-segment Wall.

**Prior audits:** [`Roadmap_Reconciliation_Post_GP-S32R.md`](Roadmap_Reconciliation_Post_GP-S32R.md), [`Next_Slice_Audit_Post_GP-S30.md`](Next_Slice_Audit_Post_GP-S30.md).

### Foundation / Match Flow sequence

1. GP-S01 … GP-S28 — **DONE / merged**
2. **GP-S29R** — LOS + health bar + team colors + Salvage Walker — **DONE / MERGED**
3. **GP-S30** — Container Launch — **DONE / MERGED**
4. **GP-S31R** — Orbital Unit Drop — **DONE / MERGED**
5. **GP-S32R** — Orbital Building Drop — **DONE / MERGED**
6. Roadmap reconciliation — **DONE / MERGED** @ `ba98383…`
7. **GP-S30R** — Combat Auto-Acquire — **DONE / MERGED**
8. **GP-S32A** — Attack-Move Reconciliation — **DONE / MERGED** @ `0df44684…`
9. **GP-S33M** — RTS Movement Reconciliation — **DONE / MERGED** @ `e40d545b…`
10. **GP-S33C** — Unit Cap + Logistics Hub Capacity — **DONE / MERGED** on `main`
11. TEMP HUD layout reconciliation — **DONE / MERGED** @ `7873d282…`
12. **GP-S34W** — Match Win/Lose MVP — **DONE / MERGED** @ `3b5cdb8…`
13. **GP-S35B** — Multi-Building Data Architecture — **DONE / MERGED** @ `6f258a1…`
14. **GP-S36G** — BuildGrid MVP — **DONE / MERGED** @ `9ace159…`
15. **GP-S37T** — Defensive Turret MVP — **DONE / MERGED** @ `c79b017…`
16. **GP-S38D** — Unit/Building Combat Data — **DONE / MERGED** @ `f841cdee…`
17. **GP-S39E** — Economy / Logistics Data Ownership — **MERGED / VERIFIED / CLOSED** @ `75a0e5bd…`
18. **GP-S40R** — Timed Retaliation Pursuit — **DONE / MERGED**
19. **GP-S41M** — Movement Shortest Yaw — **DONE / MERGED** @ `5dd56cac…`
20. **GP-0305R** — Wall Package Delivery Reconciliation — **on `main`** @ `c00e95ed…`
21. **GP-S42A** — Wall Package Data + MainBase Wall Inventory — **DONE / MERGED** @ `d9e89605…`
22. **Unit Drop Nested Readiness** — **DONE / MERGED** @ `28329701…`

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
| `GRIM_PROTOCOL_START_RULES.md` | Active process SoT (roles / one-stage / factual review) |
| [`Risk_Based_Development_Workflow.md`](Risk_Based_Development_Workflow.md) | Active process SoT for test/build selection |
| `Docs/Development/*` ops + Claude task specs | Active |
| `Docs/Game_Pitch.md` | Active onboarding |
| `UGP_ResourceGameplaySettings` + `GP/Config/DefaultGame.ini` (`[/Script/GPRuntime.GP_ResourceGameplaySettings]`) | Active — Project Settings → Game → GP Resource Gameplay (P2 + P3 `DropOffRetrySeconds`) |
| `UGP_GameplayPresentationSettings` + `DefaultGame.ini` (`[/Script/GPRuntime.GP_GameplayPresentationSettings]`) | Active — Project Settings → Game → GP Gameplay Presentation (S29R team colors + health bar tunables) |
| [`Configuration_Data_Ownership_Audit.md`](Configuration_Data_Ownership_Audit.md) | Current factual ownership/precedence audit; recommendations are not implemented architecture |
| [`MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`](MVP_Roadmap_Reconciliation_Post_Building_Vitals.md) | Current factual MVP capability matrix, remaining stages, historical mapping, and NEXT authority |
| [`Claude_Tasks/GP-FoW-Runtime-Foundation.md`](Claude_Tasks/GP-FoW-Runtime-Foundation.md) | Current implementation candidate and operator validation contract |

### Stale / needs cleanup (still in active tree)

| Document | Issue |
| --- | --- |
| `Docs/GDD/09_UI_UX.md` | Build command / construction UX leftovers |
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
3. **Economy / SWARM (resolved 2026-08-20):** `FerroniteThreatValue` from raw planetary stock at base is canonical; `GDD/00` was aligned. Final SWARM behavior remains DESIGN REQUIRED.
4. **FoW MVP (resolved 2026-08-20):** three-state FoW is in MVP per GDD/11 + TDD/15; `GDD/Out_Of_Scope.md` now excludes only advanced embellishments.
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

Current baseline: `origin/main` @ `de718725115ddd636b56092bd6197cf0f7a65950`. Current stage:
**FOW_RUNTIME_FOUNDATION_FINALIZED_READY_FOR_MERGE** on `feature/gp-fow-runtime-foundation`.
**NOT MERGED.** NEXT: production UI foundation and trusted FoW presentation.
