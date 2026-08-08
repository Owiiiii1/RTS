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

**Winner: C1.**

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

## Exact In-Scope (proposal)

1. **`UGP_OrbitalDropDefinition`** DataAsset (cost, descent duration, soft payload class, TEMP operator costs).
2. **Two DAs (or equivalent):** Worker drop + Salvage Walker drop — **one generic spawn path**, no duplicate spawn systems.
3. **`UGP_GE_SpendOrbital`** Instant native GE (SetByCaller magnitude; mirror AddOrbital). Authority-only apply. Reject insufficient funds **before** spend. Spend exactly once on accepted order.
4. **Thin `UGP_OrbitalDeliverySubsystem`** (or equivalent single authority owner): validate → spend → spawn DropPod → on land spawn payload.
5. **`AGP_DropPod`** minimal replicated actor: 2–3 s telegraph, visible, then payload spawn + destroy/cleanup. No elaborate VFX.
6. **`Server_RequestOrbitalDrop(DropDef, Location)`** on PlayerController (client intent only).
7. **Interim drop policy (see below).**
8. **Payload init:** `SetTeamId` to owning player team; unit usable (select + Move/Attack as class allows).
9. **TEMP Order UI** on existing TEMP HUD: buttons e.g. Order Worker / Order Salvage Walker — **no console required** for acceptance. Console diagnostic optional.
10. **Contracts** listed below + GPEditor candidate builds.

### Exact Out-of-Scope

- Build Grid / wall placement / Defensive Turret / Logistics Hub
- FoW visibility validation (final policy)
- Full production Order Menu / MVVM / complex reticle
- Minimap / SWARM / AI opponent / Steam
- Pathfinding / AttackMove / TargetingComponent
- Match timer/quota win wiring
- Production VFX/audio, cooldowns, pod interception, multi-pod caps (unless trivial)
- UnitDefinition DA family (use soft class on DropDef)
- Direct attribute mutation of OrbitalFerronite
- Operator-local `.uasset` / map / DefaultEngine.ini commits

---

## Temporary vs Canonical Decisions

### Drop location — **recommend interim A with B-shaped API**

| Option | Verdict |
| --- | --- |
| **A. Auto-drop near own MainBase** | **MVP UX:** TEMP buttons request drop; server picks validated offset near team MainBase. |
| **B. Player click + Nav/overlap** | **API shape now:** RPC always carries `FVector`; validation = MainBase radius + `ProjectPointToNavigation` + blocker overlap. Optional click can be added without redesign. |
| **C. Full FoW + grid** | **Defer** until FoW + BuildGrid exist. |

**Chosen policy:** Server validates drop as:

1. Owner has playable TeamId and team MainBase.
2. Location within configurable radius of own MainBase (TEMP default — DA/settings, not hardcoded magic in call sites).
3. NavMesh projectable.
4. No blocking overlap for payload footprint (simple sphere/capsule).
5. **Not** FoW / **not** grid.

**Later replacement:** same `ValidateDropZone` gains Actively Visible FoW + `UGP_BuildGridSubsystem` footprint checks per TDD/14 / ADR-0009. Interim radius policy is explicitly temporary.

### Payload choice — **C: both Worker + Salvage Walker**

Both classes exist. One DropDefinition → soft class path keeps scope small and proves combat + economy payloads. Prefer data-driven dual catalog over Worker-only if costs are TEMP.

### Pod vs delay — **real minimal `AGP_DropPod`**

ADR-0009 / GDD pillar requires visible telegraph. A replicated DropPod actor with timer is cheap and avoids a throwaway “delayed SpawnActor” that violates “all assets via DropPod.” No fancy VFX required.

### Unit cap — **temporary soft**

`CurrentUnits` / `MaxUnits` attributes exist but have **no production mutators**. For S31R: either (preferred if cheap) Instant GE / authority helper to ++ CurrentUnits on spawn and gate when `MaxUnits > 0`, **or** document temporary no-cap with follow-up slice. Do not invent permanent balance MaxUnits.

### Economy TEMP costs

Operator-test costs only (e.g. low Worker cost, higher Salvage Walker). Not permanent balance. Exact numbers locked in implementation task / DA, not this audit.

---

## Operator Acceptance Plan (UI-driven)

1. Mine → fill → Launch Container (existing).
2. See Orbital balance increase on TEMP HUD.
3. Press TEMP **Order Worker** (and **Order Salvage Walker**).
4. Orbital decreases once.
5. See DropPod telegraph ~2–3 s near MainBase.
6. Payload appears with correct team.
7. Select and Move (Worker also Mine; Salvage Walker Attack).
8. Insufficient Orbital → reject, no spend, no pod.
9. Spam/duplicate request → no double-spend.

Console may exist as diagnostic fallback only.

---

## Automated Contract Plan

Recommend `gp.Resource.RunOrbitalUnitDropContractTest` (or `gp.Orbital.RunUnitDropContractTest`):

| Case | Expect |
| --- | --- |
| Insufficient Orbital | Reject; attribute unchanged; no pod; no payload |
| Valid order | Spend once; Orbital decreases by Cost |
| Telegraph | DropPod exists / in-flight state for duration |
| Landing | Payload spawned; DropPod cleaned up |
| TeamId / owner | Payload TeamId == ordering player team |
| Worker usable | Accepts Mine/Move |
| Salvage Walker usable | Accepts Attack/Move |
| Duplicate RPC / re-entry | No double-spend |
| Authority | Client intent only; server validates |

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
| Interim drop policy mistaken for final | Document replace-with-FoW/Grid in task + ADR note |
| Soft class spawn failures | Contract asserts resolved class; fail closed (refund or never spend — prefer never spend until spawn scheduled) |
| Spend-before-validate double charge | Validate → spend → spawn pod; reject path never spends |
| CurrentUnits/MaxUnits unused | Explicit temporary policy in task |
| Scope creep into FoW/Grid/Order Menu | Hard out-of-scope list; stop condition |
| TDD S31 ID collision | Use **GP-S31R** reconciled name |
| BP payload / soft ref packaging | Prefer native C++ classes already on disk for MVP DAs |

---

## Recommended Artifacts (when implementation is assigned)

| Artifact | Value |
| --- | --- |
| Branch | `feature/gp-s31r-minimal-orbital-unit-drop` |
| Task file | `Docs/Development/Claude_Tasks/GP-S31R_Minimal_Orbital_Unit_Drop.md` |
| Status after audit merge | NEXT = GP-S31R SPEC / implementation assignment — **not started** by this audit |

---

## Stop Condition

This audit is complete when:

1. Status docs record **GP-S30 DONE / MERGED** @ `main` `0c2bfd2…`
2. This file exists and recommends **GP-S31R Minimal Orbital Unit Drop** with in/out scope
3. Audit branch pushed; **not merged** without review
4. **No** gameplay C++ / Content / operator-local files changed
5. Next implementation slice **not** started

Do **not** auto-start GP-S31R from this PR.
