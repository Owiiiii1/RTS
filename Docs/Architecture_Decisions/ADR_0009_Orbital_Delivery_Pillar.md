# ADR-0009 — Orbital Delivery as Architecture Pillar

## Status
Accepted (2026-05-16)

## Context

GrimProtocol MVP initially used а **local production model**:

- MainBase has `UGP_ProductionComponent` із FIFO queue, produces Workers locally.
- Assembly Yard built on planet through `UGP_ConstructionComponent` (Worker channels build, multi-worker speedup).
- Defensive Turret built locally same way.
- Resource pool spendable immediately upon mining drop-off (single state `Ferronite`).

Owner directive 2026-05-16 — **pivot to orbital delivery model** (Helldivers-style):

- Player не будує локально. Усі units / vehicles / modules / defensive structures / спеціальні конструкції arrive **from orbit via drop pods** after order.
- Player spends **Orbital Ferronite** (currency) для orbital drop.
- Worker drops resource into **Containers** at MainBase. Container ships to orbit when full → converts to Orbital Ferronite.
- Two states of Ferronite: Planetary (in containers, not spendable, vulnerable) vs Orbital (spendable currency).

This pivot affects multiple subsystems: building lifecycle, resource model, UI flow, win condition, multiplayer relevance.

## Decision

**Orbital Delivery is а project-wide architecture pillar.** Все, що з'являється на планеті після initial match start — arrives через `UGP_OrbitalDeliverySubsystem`. No local production. No local construction.

This is not just а gameplay mechanic — це **architectural constraint** that ripples through code structure:

- Specific systems are **removed**: `UGP_ProductionComponent`, `UGP_ConstructionComponent`, `AGP_ConstructionSite`, `AGP_GhostBuilding`, `Worker.UGP_GA_Build`.
- Specific systems are **added**: `UGP_OrbitalDeliverySubsystem` (UWorldSubsystem), `AGP_DropPod` actor, `UGP_OrbitalDropDefinition` DataAsset family, `UGP_OrderMenuVM` ViewModel.
- Resource model **split**: `OrbitalFerronite` (player attribute, spendable) vs Container state (on MainBase storage, planetary).
- Building lifecycle **simplified**: no construction phase — drop pod arrives, asset is operational immediately at landing point.

## Rationale

### Gameplay

- **Helldivers reference fantasy** — proven, satisfying, fun in v1 (passes Pillar 8 5-question gate).
- **Greed-vs-safety risk loop:** accumulated Planetary Ferronite is vulnerable. Must regularly ship. Forces strategic tempo.
- **Telegraph + window of vulnerability:** drop pod descent is 2-3 s visible to opponent + SWARM. Drops are decisions, not free production.
- **Drop targeting requires FoW visibility:** ties scout-investment до production capability.
- **Removes "production queue" complexity** — replaced by orbital order menu. Pillar 8 ("Simple Core"): 1-sentence explanation, fun у v1.

### Architectural

- **Simplifies building lifecycle** — no transient construction state. Drop pod is the only animated transition.
- **Removes Worker.Build ability** — Worker becomes pure mining + repair unit. Reduces ability surface.
- **Removes ghost preview actor** — replaced by drop-targeting reticle.
- **Cohesive subsystem** (`UGP_OrbitalDeliverySubsystem`) owns drop pipeline end-to-end. Single-responsibility.
- **DataAsset-driven catalog** — adding new drop types = new `DA_GP_OrbitalDrop_*`, no C++ change.
- **Server-authoritative** — drop spend + pod spawn + payload spawn all server-side.

### Strategic Side-Effects

- **MainBase destruction = annihilation** (changed from "consequence not auto-lose"). Без MainBase containers cannot ship → no path до victory. Cleaner win condition.
- **Symmetric for AI opponent** — AI uses same orbital model. No special AI path.
- **Drop pod telegraph creates new combat layer** (post-MVP: anti-drop intercept abilities).

## Hard Rules (Project-Wide)

1. **No local production code paths.** `UGP_ProductionComponent`, `UGP_ConstructionComponent` — removed. Не add у new code.
2. **All non-initial assets arrive via `AGP_DropPod`.** Direct `SpawnActor` для gameplay assets outside `UGP_OrbitalDeliverySubsystem` — review-blocking (exception: initial faction StartingBuildings / StartingUnits at match init via GameMode).
3. **Containers are the ONLY ship-to-orbit pipeline.** Direct write to `OrbitalFerronite` attribute outside `GE_GP_AddOrbital` (from container launch) — review-blocking.
4. **Drop validation requires Actively Visible FoW.** Blind drops banned by default; only `DropDef.bRequiresActiveVisibility = false` overrides (no MVP drop type uses this — reserved post-MVP).
5. **Drop spend через `GE_GP_SpendOrbital`** — direct attribute write banned per [ADR-0003 GAS First].
6. **`OrbitalFerronite` is `COND_OwnerOnly`** — private currency, opponent shouldn't see your funds.

## Consequences

### Positive

- **Simpler code surface:** ~5 component classes removed, replaced by ~3 new (subsystem, pod, drop reticle).
- **Cleaner gameplay loop:** мining → containers → ship → orbital pool → order → drop. Single, readable.
- **Easier to add content:** new drop types via DataAsset, no class explosion.
- **Win condition cleaner:** delivery quota (FerroniteScore) — clearly the metric.
- **Better aligned with project pillars:** Pillar 1 (Extraction-first), Pillar 8 (Simple Core), Pillar 3 (One Resource — но з 2 states).
- **Helldivers reference fantasy** — strong genre marker.

### Negative

- **Sunk cost of pre-pivot spec work** — GP-0301 / GP-0304 specs partially superseded (Output sections amended with Pivot Notes per cascade update).
- **Reduces "building" depth** — no construction site to defend / sabotage. Mitigated by drop telegraph window як new tactical layer.
- **MainBase destruction now = auto-loss** (changed from "consequence only"). Players must defend MainBase priority-1. Need balance pass.
- **All drops look similar** initially — same pod animation, varied payload. Visual variety deferred to art pass.

## Cascade Updates Required

When pivot is committed:

- ✅ `Docs/TDD/06_Building_Architecture.md` — Post-Pivot Override section (done).
- ✅ `Docs/TDD/07_Resource_Architecture.md` — Container System Update section (done).
- ✅ `Docs/TDD/14_Orbital_Delivery.md` — new file (done).
- ✅ `Docs/TDD/15_Fog_of_War.md` — new file (done).
- ✅ `Docs/GDD/10_Orbital_Delivery.md` — new file (done).
- ✅ `Docs/GDD/11_Fog_of_War.md` — new file (done).
- ✅ Task Outputs (GP-0301, GP-0303, GP-0304, GP-0202, GP-0204) — Pivot Notes appended (done).
- ✅ `Docs/TDD/13_Architecture_Proposal.md` — `UGP_ProductionComponent` / `UGP_ConstructionComponent` removed from class list; orbital subsystem added; AI opponent confirmed MVP (done).
- ✅ `Docs/GDD/05_Buildings.md` — inline rename AssemblyYard → Logistics Hub (done).
- ✅ `Docs/GDD/06_Resources.md` — Container System gameplay section (done).
- ⏳ Slice files `GP-S##` — Slices 6 / 8 / 11 reorganized у TDD/13 (pending — materialize incrementally; code/process work).
- ⏳ Backlog GP-0802 scope updated від "Assembly Yard rename" до "Logistics Hub rename + content rewrite" (pending).
- ✅ Win condition spec у GDD/08 — переробити з score-race до delivery quota (done).

## Alternatives Considered

### Hybrid (some buildings local, some orbital)

Reject — splits architecture, creates "which path do I use" decision per building type. Confusing rules для player + designer.

### Container System without orbital delivery

Keeps local production but adds container buffer. Reject — half-measure, doesn't deliver risk loop або Helldivers fantasy.

### Orbital delivery only for vehicles / Workers; buildings stay local

Reject — same hybrid issue. Cleaner to commit fully.

### Keep production but add "Order Module" як alternate path

Adds parallel system. Pillar 8 (Simple Core) violation — two ways to do same thing.

## Refinement — 2026-08-08 (Owner-approved; does not replace ADR)

This note **refines placement/procurement UX** without overturning the pillar (no local production; all non-initial assets via DropPod; GAS spend; FoW remains relevant for **building** deploy when FoW exists).

| Topic | Refinement |
| --- | --- |
| Dual flows | **Unit Delivery** vs **Building Purchase→READY→Deploy** share one DropPod/rocket family. |
| Unit landing | No free world placement for normal units. Land at authored MainBase **Unit Drop Zone** (not hardcoded BaseLocation+offset). |
| Transport slots | Unit pods use data-driven **PodTransportSlotCapacity** + per-unit **TransportSlotCost** (MVP examples: capacity 4; Worker 1; Salvage Walker 2). Distinct from MaxUnits. |
| Unit spend | Spend once on manifest Confirm. Reject whole manifest if over MaxUnits (no silent partial fill). |
| Building spend | Spend on **Purchase** → READY inventory. Deploy consumes READY; **no second Orbital charge**. Esc cancel keeps READY. |
| FoW rule #4 | Continues to apply to **building placement / deploy validation** when FoW ships. Unit Drop Zone path does not use FoW click targeting. |
| Presentation | Native DropPod lifecycle + authored BP mesh/Niagara soft refs; owner-replaceable visuals without gameplay rewrite. |

Canonical player docs: `Docs/GDD/10_Orbital_Delivery.md`. Engineering: `Docs/TDD/14_Orbital_Delivery.md`.

## Refinement — 2026-08-18 (GP-0305R; does not replace ADR)

Wall is a **third orbital flow**. Buy a **Wall Package of 5** (one rocket to MainBase). Placement is **Build Wall** from MainBase inventory (max 5, no stacking). Not READY. Not per-segment pods. Not Worker construction. Details: `Docs/Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md`.

## References

- Memory rule: `project_orbital_delivery_model`.
- Memory rule: `project_container_system`.
- Memory rule: `project_fog_of_war`.
- `Docs/GDD/10_Orbital_Delivery.md`.
- `Docs/TDD/14_Orbital_Delivery.md`.
- `Docs/Architecture_Decisions/ADR_0002_Data_Driven_First.md`.
- `Docs/Architecture_Decisions/ADR_0003_GAS_First.md`.
- `Docs/Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md`.
- Pillar 1 (Industrial Extraction First), Pillar 3 (One Resource), Pillar 8 (Simple Core, Combinatorial Depth).
