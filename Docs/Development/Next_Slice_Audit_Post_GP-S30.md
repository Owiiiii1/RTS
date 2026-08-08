# Post-GP-S30 Next Slice Audit

## Baseline

| Fact | Value |
| --- | --- |
| Audit branch | `audit/post-gp-s30-next-slice` |
| Remote `main` HEAD | `0c2bfd2799f85fe281e9920404664d7a16067bf4` |
| GP-S30 finalization parent | `824bf82f203b43387c16beb45b5edf4eb96c7ec1` |
| Last closed stage | **GP-S30 — Container Launch / Orbital Conversion** — **DONE / MERGED** |
| Production code in this audit | **None** (docs/planning only) |

GP-S30 closed the planetary→orbital conversion half of ADR-0009: Ready container launch → Instant `UGP_GE_AddOrbital` / `UGP_GE_AddScore` → Threat decrease; TEMP HUD Base/container breakdown + Launch; full-storage Worker `WaitingForDropOff` resume.

---

## GP-S30 Closed / Merged Statement

**Factual:** GP-S30 is merged on remote `main` at `0c2bfd2…`.  
Prior status text claiming “FINALIZATION_READY / not merged” is obsolete and must not drive NEXT.

Do **not** start the next implementation slice from this audit PR. Spec materialization + explicit implementation assignment required first.

---

## Current Core Loop Coverage

### DONE (playable vertical)

```
Worker mine → cargo → MainBase Storage
→ FerroniteThreatValue ↑
→ Ready container → Launch (2.5s)
→ Planetary ↓ → OrbitalFerronite ↑ + FerroniteScore ↑
→ FerroniteThreatValue ↓
→ TEMP HUD shows Base/containers/Orbital; Launch UI-driven
→ Storage full → WaitingForDropOff (cargo kept) → resume after space
```

### MISSING immediate next (gap)

```
OrbitalFerronite
→ spend (GAS)
→ request orbital asset
→ telegraph / drop
→ payload appears (TeamId / control)
→ player selects / commands new unit
```

Without this, OrbitalFerronite is grant-only currency with no spend sink — the acquisition half of ADR-0009 remains closed.

---

## Remaining Blockers (MVP-relevant)

| Blocker | State | Blocks |
| --- | --- | --- |
| Spend Orbital via GAS | **Absent** (no Spend GE) | Any order |
| `Server_RequestOrbitalDrop` | **Absent** | UI-driven drop |
| `UGP_OrbitalDeliverySubsystem` | **Absent** | Canonical enqueue |
| `AGP_DropPod` | **Absent** | Telegraph pillar |
| `UGP_OrbitalDropDefinition` | **Absent** | Data-driven catalog |
| TEMP Order UI | **Absent** (Launch-only TEMP HUD) | Operator acceptance without console |
| FoW visibility validation | **Absent** | Canonical TDD/14 drop zones |
| `UGP_BuildGridSubsystem` | **Absent** | Grid-aligned placement |
| Unit cap mutation (`CurrentUnits`/`MaxUnits`) | Attrs exist; **no production mutators** | Cap gate (defer or minimal) |
| Full Order Menu / MVVM / reticle | **Absent** | Polish — not first proof |
| Logistics Hub / walls / turrets | **Absent** | Later Slice 8 content |
| SWARM / AI / win wiring / Steam | Partial / absent | Later |

---

## Production Source Inventory (factual)

### Exists and relevant

| Piece | Path / note |
| --- | --- |
| `OrbitalFerronite`, `FerroniteScore`, `MaxUnits`, `CurrentUnits` | `UGP_PlayerAttributeSet` on `AGP_PlayerState` ASC |
| Grant GEs | `UGP_GE_AddOrbital`, `UGP_GE_AddScore` (Instant SetByCaller) |
| Launch | `UGP_StorageComponent::TryLaunchReadyContainer` + PC `Server_RequestLaunchReadyContainer` |
| TEMP HUD | `UGP_TEMP_S28P_PlanetaryFerroniteHUD` (Base/containers/Orbital/Launch) |
| Units | `AGP_Worker`, `AGP_SalvageWalker` (native C++; no UnitDefinition DA) |
| TeamId | Authority `SetTeamId`; GameMode `AssignPlayableTeamId` |
| Commands | PC `Server_RequestCommand` — Move/Attack/Mine only |
| Nav helpers | Resource approach / haul nav queries (not general RTS pathfollowing) |
| Tags (unused consumers) | `Command_OrderDrop`, `Drop_Type_*`, `State_PodInFlight`, `Notify_InsufficientOrbitalFerronite`, `Notify_DropRejected` |

### Absent

- Spend Orbital GE (`GE_GP_SpendOrbital` / `UGP_GE_SpendOrbital`)
- `UGP_OrbitalDeliverySubsystem`, `AGP_DropPod`, `UGP_OrbitalDropDefinition`
- Order Menu / DropReticle / BuildMenu classes (`GPUIRuntime` shell only)
- FoW / BuildGrid subsystems
- Production unit spawn RPC / orbital order path
- UnitDefinition / BuildingDefinition DAs

---

## Dependency Map

```
GP-S30 OrbitalFerronite grant (DONE)
        │
        ▼
Spend GE + authority order RPC + drop validation (interim)
        │
        ▼
Minimal DropPod telegraph → payload Worker / SalvageWalker
        │
        ▼
TEMP Order buttons (extend existing TEMP HUD)
        │
        ├─► later: FoW + BuildGrid validation (replace interim policy)
        ├─► later: full Order Menu / reticle / Logistics Hub
        └─► later: buildings/walls/turrets via same DropDefinition path
```

NavMesh project + MainBase proximity can validate **now**. FoW/Grid are **not** prerequisites for first spend→spawn proof if interim policy is explicit and replaceable.

---

## Candidate Comparison

### C1 — Minimal Orbital Unit Drop (primary)

Spend Orbital → DropPod telegraph → spawn Worker and/or Salvage Walker near own base; TEMP UI buttons; thin delivery subsystem.

- **Pros:** Closes ADR-0009 acquisition gap; reuses S30 currency + existing unit classes; UI-driven acceptance; data-driven catalog scales later.
- **Cons:** Interim drop validation (no FoW/grid yet).

### C2 — Full TDD/14 OrbitalDelivery + Order Menu + FoW/Grid

- **Pros:** Canonical end state.
- **Cons:** Pulls FoW + BuildGrid + CommonUI Order Menu — too wide; violates “smallest useful vertical.”

### C3 — Pathfinding / Targeting / AttackMove

- **Pros:** Combat QoL.
- **Cons:** Does not spend Orbital; economy loop remains incomplete.

### C4 — Buildings / walls / Logistics Hub first

- **Pros:** Slice 8 content fantasy.
- **Cons:** Needs drop pipeline first (ADR: all non-initial assets via DropPod).

**Winner: C1** — still correct after 2026-08-08 owner refinements (see § Owner-approved orbital procurement refinement). Building inventory/ghost becomes the **follow-on** vertical on the same DropPod pipeline.

---

## Owner-approved orbital procurement refinement (2026-08-08)

Owner accepted design that **updates canonical GDD/TDD/ADR-0009 refinement** before audit merge. Philosophy reaffirmed: **no local production** after start (MainBase + 2 Workers initial only).

### Unit Drop Zone

- Normal units do **not** free-place in world.
- Land at authored MainBase **Unit Drop Zone** (scene anchor/component; server-resolved; not `BaseLocation + hardcoded offset`).
- Buildings never use this pad.

### Transport Slots

- Pod packing capacity ≠ MaxUnits.
- MVP tuning examples (DA-driven, not final balance): capacity **4**; Worker **1**; Salvage Walker **2**.
- Manifest: `sum(count × slotCost) <= capacity`.
- Future-proof: larger pods / upgrades / heavier units — schema only now.

### Unit manifest semantics

UI builds manifest → shows slots, counts, per-unit Orbital + slot costs, total → Confirm → validate → spend once → one DropPod → Drop Zone → multi-unit deterministic offsets → control.

### UnitCap vs DropSlots

Distinct systems. MVP: **reject entire manifest** if it would exceed free MaxUnits (no silent partial fill).

### Building Orbital Inventory / READY

Purchase spends Orbital immediately → READY count++. Deploy later. Conceptual SoT: `(BuildingType/DropDef, ReadyCount)` — server-authoritative, owner UI, decrement once on accepted deploy. Spec for **building slice**; not S31R.

### Building deployment ghost

READY click → ghost → LMB consume READY + DropPod (no second spend); Esc/RMB cancel keeps READY.

### Shared rocket presentation

One `AGP_DropPod` + authored BP (mesh/Niagara). Unit and building share MVP animation family. Hooks: descent start / progress / impact / payload deploy (exact names = impl).

### Authored Niagara/mesh seam

Gameplay C++ must not hardcode Niagara/mesh. Soft-ref BP visual; owner replaces without rewrite.

### Multi-unit spawn offsets

Server deterministic offsets around Drop Zone; no identical transforms; basic spacing; no formation AI.

### Revised GP-S31R scope

**Still the best next cut.** Expanded vs prior audit draft:

**IN for GP-S31R:**
- GAS SpendOrbital
- Unit drop defs + TransportSlotCost + PodTransportSlotCapacity
- Unit manifest TEMP UI (slots/costs/total/Confirm)
- MainBase Unit Drop Zone authored seam
- Real minimal AGP_DropPod + presentation hooks / soft visual class
- Worker + Salvage Walker multi-payload spawn + offsets
- Select/control after landing
- Contracts for funds/slots/cap/manifest/TeamId/no double-spend

**OUT of GP-S31R (next building vertical):**
- Building READY inventory
- Building ghost placement
- Build Grid / FoW
- Logistics Hub / Turret / Walls implementation

### Future building deployment slice dependency

Requires shared DropPod + Spend GE from S31R; adds Purchase/READY/Deploy RPCs + ghost + building payloads.

---

## Recommended Next Slice

### Slice ID

**GP-S31R — Minimal Orbital Unit Drop**

**Why not plain GP-S31:** Historical TDD/13 labels `GP-S31` as `GE_GP_Damage_Basic` (already shipped under combat slices). Operational next after GP-S30 / GP-S29R reconciliation pattern favors an **`R` reconciled ID** to avoid agent/doc collision with obsolete TDD numbering. Canonical TDD “S37 OrbitalDelivery” is the design ancestor; chronological playable ID is **GP-S31R**.

### Recommended branch (implementation, later)

`feature/gp-s31r-minimal-orbital-unit-drop`

### Recommended task filename (later, not created here)

`Docs/Development/Claude_Tasks/GP-S31R_Minimal_Orbital_Unit_Drop.md`

---

## Exact In-Scope (GP-S31R proposal — revised)

1. `UGP_GE_SpendOrbital` Instant GE (mirror AddOrbital).
2. Unit catalog / DropDefs with OrbitalCost + TransportSlotCost; PodTransportSlotCapacity (DA/settings).
3. Unit **manifest** order path + `Server_RequestUnitDrop(Manifest)` (name TBD).
4. Thin `UGP_OrbitalDeliverySubsystem` (or equivalent): validate → spend once → DropPod.
5. MainBase **Unit Drop Zone** authored anchor (server resolve).
6. `AGP_DropPod` + soft-ref authored visual BP hooks (no hardcoded Niagara/mesh).
7. Worker + Salvage Walker payloads; deterministic multi-unit offsets; SetTeamId; selectable.
8. TEMP Unit Order UI (manifest fill UI) — no console required.
9. Unit-cap gate: reject whole manifest if over MaxUnits (when MaxUnits policy active).
10. Contracts + GPEditor candidate builds.

## Exact Out-of-Scope (GP-S31R)

- Building READY inventory / Purchase / Deploy ghost
- Build Grid / FoW / Logistics Hub / Turret / Walls
- Full production Order Menu / MVVM polish
- Minimap / SWARM / AI / Steam / pathfinding / AttackMove / Targeting
- Win wiring / cooldowns / intercept / multi-pod caps
- Direct Orbital attribute mutate
- Operator-local assets

---

## Temporary vs Canonical Decisions

### Unit landing — **canonical Unit Drop Zone (owner-approved)**

Replaces prior audit interim “MainBase radius click/auto” as the **player-facing** unit policy. Implementation still resolves an authored MainBase-relative transform (not hardcoded offset). FoW/grid remain for **buildings** later.

### Building placement — **Purchase→READY→Deploy** (owner-approved)

Canonical; not in S31R scope.

### Pod vs delay — **real DropPod** (unchanged)

### Payload both Worker + SW — **yes** via manifest (unchanged intent; now slot-packed)

### Unit cap — **gate when MaxUnits active**

Attrs exist without mutators today. S31R: increment CurrentUnits on spawn if feasible; reject manifest over cap; if MaxUnits==0 temporary soft-open with explicit note.

### Economy TEMP costs

Operator-test costs only (Worker cheaper than Salvage Walker). Not permanent balance.

---

## Operator Acceptance Plan (UI-driven)

1. Mine → fill → Launch Container (existing).
2. See Orbital balance increase on TEMP HUD.
3. Open TEMP Unit Order; fill manifest within slots (e.g. mix Workers + Salvage Walker).
4. Confirm → Orbital decreases **once** by total cost.
5. DropPod telegraph ~2–3 s at MainBase Unit Drop Zone.
6. Multiple units appear with spacing; correct TeamId.
7. Select/control (Mine / Attack as class allows).
8. Insufficient Orbital / over slots / over MaxUnits → reject, no spend.
9. Duplicate Confirm → no double-spend.

Console diagnostic optional only.

---

## Automated Contract Plan

Recommend `gp.Resource.RunOrbitalUnitDropContractTest` (or `gp.Orbital.RunUnitDropContractTest`):

| Case | Expect |
| --- | --- |
| Insufficient Orbital | Reject; no spend; no pod |
| Slots overflow | Reject; no spend |
| Unit cap overflow | Reject full manifest; no spend |
| Valid manifest | Spend once; Orbital -= total |
| Telegraph | DropPod in-flight ~duration |
| Landing | Units spawned near Drop Zone; non-overlapping |
| TeamId | Matches ordering player |
| Worker / SW usable | Mine / Attack as applicable |
| Duplicate RPC | No double-spend |
| Authority | Client intent only |

Also keep S28 / ContainerLaunch / combat regressions green on finalization.

### Build policy (candidate)

| Phase | Builds |
| --- | --- |
| Candidate implementation | **GPEditor Win64 Development + UHT** only |
| After operator PASS | GP Win64 Development + GP Win64 Shipping |

---

## Risks

| Risk | Mitigation |
| --- | --- |
| Hardcoded BaseLocation+offset | Require authored Unit Drop Zone seam |
| Scope creep into buildings | Hard OUT list; building slice after DropPod lands |
| Spend-before-validate | Validate → spend → pod |
| Soft visual missing | Soft-ref + fail-closed diagnostics |
| MaxUnits unset (0) | Explicit temporary policy in task |
| TDD S31 ID collision | Keep **GP-S31R** |

---

## Recommended Artifacts (when implementation is assigned)

| Artifact | Value |
| --- | --- |
| Branch | `feature/gp-s31r-minimal-orbital-unit-drop` |
| Task file | `Docs/Development/Claude_Tasks/GP-S31R_Minimal_Orbital_Unit_Drop.md` |
| Status after audit merge | NEXT = GP-S31R SPEC / assignment — **not started** by this audit |

---

## Stop Condition

This audit refinement is complete when:

1. Canonical GDD/TDD/ADR refinement docs match owner decisions
2. This file records revised GP-S31R in/out scope
3. Branch pushed; **not merged** without review
4. **No** gameplay C++ / Content / operator-local files
5. GP-S31R implementation **not** started

Do **not** auto-start GP-S31R from this PR.
