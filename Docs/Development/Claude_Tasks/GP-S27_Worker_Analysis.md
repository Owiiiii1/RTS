# GP-S27 — Worker Architecture Reconciliation and Implementation Analysis

## Status
**GP-S27_WORKER_ANALYSIS_CORRECTED_READY_FOR_REVIEW**

## Baseline
`main` @ `d81a9bea45f35069636f13df9229685226282311`  
(includes merged GP-S26B2A Blueprint Authored Visuals)

Branch: `feature/gp-s27-worker-analysis`  
Initial analysis: `b5526d1ebad4d6c76e522005767d0bb162adccd1`  
Type: **analysis / documentation only** — no production C++, no content assets, no map edits.

## Correction reason

Initial analysis correctly inventoried missing Slice-6 systems, but incorrectly proposed:

- treating `UGP_ResourceDefinition` as non-blocking;
- temporary hardcoded mining rates in `UGP_MiningComponent`;
- starting next with **GP-S26C** Mine Target Compatibility before S23/S24;
- softening S24 into “ResourceNode stand-in without full deposit contract.”

That violates documented **implementation order** (`TDD/13` Slice 6) and **ADR-0002 Data-Driven First**.

This correction restores the mandatory dependency chain and DataAsset-first balance policy.

---

## Canonical dependency order (mandatory)

```text
GP-S23  UGP_ResourceDefinition
GP-S24  Ferronite deposit implementation
GP-S25  UGP_CargoComponent
GP-S26  UGP_MiningComponent
GP-S27  AGP_Worker
GP-S28  UGP_StorageComponent + FerroniteThreatValue write
```

**Rules:**

- Do **not** skip GP-S23.
- Do **not** hardcode mining rates / yield balance in C++.
- Do **not** defer ResourceDefinition “until later” past mining implementation.
- Do **not** start Mine-target work before S23/S24 deposit contract.
- Prototype arena `GP-S27A1` / `GP-S27A2` are **not** canonical Slice-6 Worker.

Documentation priority:

1. ADR + `Docs/TDD/13_Architecture_Proposal.md` (ADR-0009 / ADR-0002)
2. `Docs/GDD/` (`02`, `04`, `06`)
3. `Docs/Development/AI_Project_Log.md` (live status)
4. `Docs/TDD/05_Unit_Architecture.md`, `Docs/TDD/07_Resource_Architecture.md`
5. Archive — not source of truth

---

## Current-code inventory

| Entity | C++ status | Notes |
| --- | --- | --- |
| `UGP_ResourceDefinition` | **Missing** | Required next (S23R) |
| `AGP_FerroniteDeposit` | **Missing** | Canonical name; not in code |
| `AGP_ResourceNode` | **Partial deposit** | `AActor`; Ore; Max/Current; `ConsumeResource`; visuals |
| `EGP_ResourceType` | **Partial** | `None`, `Ore` only |
| `UGP_CargoComponent` | **Missing** | |
| `CarriedFerronite` | **Partial / unused economy** | Replicated attr; no cargo component |
| `UGP_MiningComponent` | **Missing** | |
| `AGP_Worker` | **Missing** | Tag `GP.Unit.Type.Worker` only |
| `UGP_StorageComponent` | **Missing** | |
| `FerroniteThreatValue` | **Partial** | Field on `AGP_GameState`; no drop-off write |
| `GP.Command.Mine` | **Partial** | Validate path exists; cannot target ResourceNode; no executor |
| `GP.Command.Repair` | **Partial** | Tag only; full GA later (S46) |
| Move / Attack | **Implemented** | Reusable routing |
| `VisualSourceMode` | **Implemented** | S26B2A authored / native fallback |
| `bAutoAttacks` | **Missing** | Must be explicit Worker policy later |

---

## Revised S23–S28 reconciliation matrix

| Stage | Canonical requirement | Current implementation | Status | Follow-up stage |
| --- | --- | --- | --- | --- |
| **S23** | `UGP_ResourceDefinition` Ferronite metadata + DA-driven rates | Absent | **Missing** | **GP-S23R** |
| **S24** | Ferronite deposit: capacity, tags, soft-cap/queue, mine-targetable | `AGP_ResourceNode` amounts + consume; Ore naming; no definition ref; Mine cannot target it; no miner queue | **Partially implemented** on ResourceNode; BuildingBase subclass **not** required if ResourceNode retained with owner-aligned ADR note | **GP-S24R** |
| **S25** | `UGP_CargoComponent` | Missing; attr scaffold only | **Missing** | **GP-S25** |
| **S26** | `UGP_MiningComponent` auto-cycle SM using definition tunables | Missing | **Missing** | **GP-S26** |
| **S27** | `AGP_Worker` assembles cargo+mining; no Build; no auto-attack | Missing | **Missing** | **GP-S27** |
| **S28** | Storage drop-off + ThreatValue write | ThreatValue field only | **Partially** / **Missing** | **GP-S28** |

---

## GP-S23R — Resource Definition Reconciliation

**Exact next implementation stage after this analysis is accepted.**

### In scope
- Runtime type `UGP_ResourceDefinition : UPrimaryDataAsset` (or project-equivalent DataAsset)
- Ferronite metadata / resource identity / display fields per TDD/07 + TDD/10
- Mining rate / yield-related tunables required by later MiningComponent
- Cargo/resource unit metadata where canonical docs require it on the resource DA
- Soft object references for content
- Asset Manager / cook registration **only if** required by existing project policy (ADR-0002 soft refs)
- Content asset e.g. `DA_GP_Resource_Ferronite` (implementation stage — not this analysis)

### Out of scope
- Mining state machine
- Worker
- Storage / ThreatValue write
- Map changes
- Visual primitive DataAsset profiles (abandoned S26B2A experiment — unrelated)

### Conclusion
ResourceDefinition is **mandatory** before mining balance logic. No hardcoded C++ mining rates.

---

## GP-S24R — Ferronite Deposit Contract on `AGP_ResourceNode`

### Architecture stance
Retain **`AGP_ResourceNode` as the deposit actor (`AActor`)** for the near-term Slice-6 path, provided S24R brings it up to the **canonical deposit contract** without inventing a premature `AGP_BuildingBase` rewrite.

This is a **documented adaptation** of canonical class name `AGP_FerroniteDeposit` → existing `AGP_ResourceNode`, not a license to skip S24 contents.

### In scope
- Soft reference from node → `UGP_ResourceDefinition`
- Apply canonical resource tags (`GP.Resource.Node`, Ferronite type tag / naming policy)
- Ferronite vs Ore naming policy (identity aligned to Ferronite; Ore enum migration or alias documented in S24R implementation task)
- Keep replicated `MaxAmount` / `CurrentAmount` / authority `ConsumeResource`
- **Mine target compatibility** as part of deposit contract (command validate/smart-build can target ResourceNode) — **not** a pre-S23 stage
- Canonical S24 miner occupancy / soft-cap / queue hooks (`ActiveMiners` / `WaitingMiners` / max concurrent) as required by TDD/13 + deposit DA fields

### Soft-cap / queue
Canonical S24 includes soft-cap + queue. **Include these hooks in GP-S24R.**

Deferring queue would be an **explicit documented deviation requiring owner approval** — not a silent exclusion. This analysis does **not** recommend that deviation.

### Out of scope
- `UGP_MiningComponent`
- `AGP_Worker`
- `UGP_StorageComponent`
- Full BuildingBase / footprint rewrite unless forced by ADR conflict during S24R design review

---

## GP-S25 — `UGP_CargoComponent`

Separate stage after S23R/S24R.

### Cargo single source of truth (recommendation)

| Option | Verdict |
| --- | --- |
| Dual-write Component + Attribute independently | **Rejected** — undefined authority |
| AttributeSet-only (`CarriedFerronite`) | **Rejected** — contradicts TDD cargo-component design and Worker composition |
| **`UGP_CargoComponent` authoritative SoT** | **Selected** |

**Migration impact:**

- Economy / mining / UI cargo bar read **CargoComponent**.
- Existing `CarriedFerronite` on `UGP_UnitAttributeSet` remains for now as a **non-authoritative legacy GAS field**:
  - either left unused (0) with a follow-up cleanup to remove/repurpose,
  - or later given an optional **server one-way mirror** from CargoComponent for GAS/UI convenience.
- Mining/cargo code must **never** treat the attribute as an independent write target.
- No unexplained dual-write.

---

## GP-S26 — `UGP_MiningComponent`

After S23R + S24R + S25.

### Requirements
- Server-authoritative mining state machine
- Reads **ResourceDefinition** tunables (rate/yield); **no temporary hardcoded balance**
- Uses deposit `ConsumeResource` + S24R occupancy hooks
- No permanent idle tick (timers/events/state-driven)
- Before S28 Storage exists: on full cargo enter explicit **`CargoFull` / `WaitingForDropOff`** (or equivalent) — **do not** fake Storage drop-off
- Auto-cycle toward base only when a valid drop-off target exists (S28+); until then remain in CargoFull/Waiting

---

## GP-S27 — `AGP_Worker`

### Selected architecture (unchanged)
**`AGP_Worker : AGP_MobileUnit`** (sibling of `AGP_Unit`).

### Clarifications
- Worker **assembles** already-shipped `UGP_CargoComponent` + `UGP_MiningComponent` (S25/S26)
- Worker **must not** re-implement S23–S26 insides
- `bAutoAttacks=false`; Attack / AttackMove **rejected**
- Allowed economic commands: Mine / Move / Stop (+ Repair only at documented MVP depth)
- Repair: **not** “fully ready” in S27 if full `UGP_GA_Repair` / OrbitalFerronite cost remains S46 — S27 may accept command or stub only; must not claim complete repair gameplay
- Blueprint-authored visuals via S26B2A `VisualSourceMode`
- No Storage / orbital delivery / Build / Produce

---

## Exact recommended implementation sequence

```text
GP-S23R  Resource Definition Reconciliation
   ↓
GP-S24R  ResourceNode Ferronite Deposit Contract
         (includes Mine target compatibility + soft-cap/queue hooks)
   ↓
GP-S25   UGP_CargoComponent
   ↓
GP-S26   UGP_MiningComponent
   ↓
GP-S27   AGP_Worker
   ↓
GP-S28   UGP_StorageComponent + FerroniteThreatValue write
```

**Do not** recommend GP-S26C (or any Mine-target-first stage) before S23R/S24R.

**Exact next stage:** **GP-S23R — Resource Definition Reconciliation**.

---

## Worker inheritance options (summary)

| Option | Decision |
| --- | --- |
| A `AGP_Worker : AGP_Unit` | Rejected — combat/infantry concrete class |
| **B `AGP_Worker : AGP_MobileUnit`** | **Selected** — matches TDD/13 |
| C role on `AGP_Unit` | Rejected — conflates roles |

---

## Networking / tick (future stages)

- Server authority: Mine execution, cargo mutation, ConsumeResource, occupancy
- VisualSourceMode: class/default cosmetic, not gameplay RPC
- No permanent idle tick on Worker / Mining / Cargo when idle
- Controller tick remains enabled; visual component tick remains disabled

---

## Acceptance criteria (future GP-S27 only)

1. Worker placeable; no auto-attack; Attack rejected.
2. Move/Stop work.
3. Mine targets ResourceNode under S24R contract; mining uses S26 + S23 definition rates.
4. Cargo fills via S25; full cargo → WaitingForDropOff until S28.
5. Listen+client replication coherent.
6. Authored BP visual compatible.
7. No Storage/Build/Produce/map persistence unless requested.

---

## Operator validation plan (future GP-S27)

1. Place `BP_Worker` — authored visual; do not save map unless asked.
2. Select + Move.
3. Attack rejected / no auto-attack.
4. Mine ResourceNode — approach, consume, cargo increase (definition-driven rates).
5. Full cargo → WaitingForDropOff (no fake Storage).
6. Inspect server + client.
7. Native fallback Worker optional.
8. Tick / collision / nav checks; discard temp placements.

---

## Risks

| Risk | Mitigation |
| --- | --- |
| Skipping S23 for speed | Forbidden by this corrected analysis |
| Hardcoded rates in MiningComponent | Forbidden (ADR-0002) |
| Mine-target before deposit contract | Belongs in S24R only |
| Silent skip of miner queue | Not allowed; S24R includes hooks or owner-approved deviation |
| Dual-write cargo | CargoComponent SoT only |
| Claiming Repair complete in S27 | Document MVP depth; S46 for full GA |
| Storage creep in S26/S27 | CargoFull/WaitingForDropOff until S28 |

---

## Open questions

### Resolved from canonical docs (not open)

| Topic | Resolution |
| --- | --- |
| Implementation order S23→S28 | Mandatory (`TDD/13`) |
| Data-Driven First for rates | Mandatory (`ADR-0002`); no C++ hardcode |
| ResourceDefinition before mining | Mandatory |
| Worker inheritance | `AGP_Worker : AGP_MobileUnit` (`TDD/13`) |
| Worker no Build / no local production | ADR-0009 |
| Mine-target before S23 | Invalid sequence |

### Real owner decisions

1. Exact Ferronite/Ore enum+display migration strategy inside S24R (alias vs rename vs dual identity period).
2. Whether `CarriedFerronite` gets a later one-way mirror from CargoComponent or is removed in a cleanup stage.
3. S27 Repair depth: command reject vs accept-stub vs minimal heal before S46.
4. Whether retaining `AGP_ResourceNode : AActor` (vs eventual BuildingBase `AGP_FerroniteDeposit`) needs a short ADR addendum — analysis recommends ResourceNode + S24R contract; confirm with owner.
5. Soft-cap numbers / replication detail for ActiveMiners count (values from DA; confirm any COND_* nuance).

---

## Recommended next task

**GP-S23R — Resource Definition Reconciliation** (implementation).

Then GP-S24R → GP-S25 → GP-S26 → GP-S27 → GP-S28.

Do **not** start Worker/Cargo/Mining C++, DataAsset content, or map work on this analysis branch.
