# GP-S27 — Worker Architecture Reconciliation and Implementation Analysis

## Status
**GP-S27_WORKER_ANALYSIS_READY_FOR_REVIEW**

## Baseline
`main` @ `d81a9bea45f35069636f13df9229685226282311`  
(includes merged GP-S26B2A Blueprint Authored Visuals)

Branch: `feature/gp-s27-worker-analysis`  
Analysis commit: `b5526d1ebad4d6c76e522005767d0bb162adccd1`  
Type: **analysis / documentation only** — no production C++, no content assets, no map edits.

## Canonical place in roadmap

From `Docs/TDD/13_Architecture_Proposal.md` Slice 6:

| Stage | Canonical deliverable |
| --- | --- |
| GP-S23 | `UGP_ResourceDefinition` (Ferronite metadata) |
| GP-S24 | `AGP_FerroniteDeposit` (capacity, soft-cap + miner queue) |
| GP-S25 | `UGP_CargoComponent` |
| GP-S26 | `UGP_MiningComponent` (auto-cycle SM) |
| **GP-S27** | **`AGP_Worker`** (`bAutoAttacks=false`; mine + carry + repair; no Build) |
| GP-S28 | `UGP_StorageComponent` drop-off + `FerroniteThreatValue` write |

**Naming collision note:** Prototype arena slices `GP-S27A1` / `GP-S27A2` are **not** canonical Slice-6 Worker. They delivered `AGP_ResourceNode` + `L_PrototypeArena` scaffolding only.

Documentation priority used (INDEX + ADR over Archive):

1. `Docs/Architecture_Decisions/` + `Docs/TDD/13_Architecture_Proposal.md` (ADR-0009 wins on Build/Produce)
2. `Docs/GDD/` (`02`, `04`, `06`)
3. `Docs/Development/AI_Project_Log.md` for live status
4. `Docs/TDD/05_Unit_Architecture.md`, `Docs/TDD/07_Resource_Architecture.md`
5. Archive — **not** source of truth

---

## Current-code inventory

| Entity | C++ status | Location / notes |
| --- | --- | --- |
| `UGP_ResourceDefinition` | **Missing** | Docs/TDD only |
| `AGP_FerroniteDeposit` | **Missing** | Canonical name unused in code |
| `AGP_ResourceNode` | **Implemented** | `GPRuntime/Resources`; `AActor`; replicated type/max/current; `ConsumeResource` |
| `EGP_ResourceType` | **Implemented** | `None`, `Ore` only (not Ferronite enum) |
| `UGP_CargoComponent` | **Missing** | Docs only |
| `CarriedFerronite` attr | **Partial** | `UGP_UnitAttributeSet` replicated; no cargo UX / component |
| `UGP_MiningComponent` | **Missing** | Docs only |
| `AGP_Worker` | **Missing** | Tags only (`GP.Unit.Type.Worker`) |
| `UGP_StorageComponent` | **Missing** | Docs only |
| `FerroniteThreatValue` | **Partial** | `AGP_GameState` replicated getter/setter; **no** drop-off write path |
| `GP.Command.Mine` | **Partial** | Tag + smart-build + server validate; **no executor**; target must be `AGP_UnitBase` + `GP.Resource.Node` |
| `GP.Command.Repair` | **Partial** | Tag registered; **not** in validate allow-list; GA is later (S46) |
| `GP.Resource.Node` / `GP.Resource.Type.Ferronite` | **Tags only** | Not applied on `AGP_ResourceNode` |
| Unit hierarchy | **Implemented** | `AGP_UnitBase` → `AGP_MobileUnit` → `AGP_Unit` |
| `bAutoAttacks` | **Missing** | Docs/`UGP_UnitDefinition` only; no definition class in C++ |
| Move / Attack routing | **Implemented** | Held-command + `UGP_MovementComponent::RequestMove` |
| Movement-to-mine-target | **Missing** | Mine enters Held but does not approach deposit |
| VisualSourceMode | **Implemented** | Unit + ResourceNode; NativeFallback / AuthoredComponents (S26B2A) |
| Blueprint examples | **Implemented** | `BP_Unit_AuthoredExample`, `BP_ResourceNode_AuthoredExample` |

### Critical compatibility gap
`UGP_CommandComponent` Mine validation requires `AGP_UnitBase` with `GP.Resource.Node`. Shipped `AGP_ResourceNode` is a plain `AActor` without that path → **Mine cannot currently target the only deposit actor in the project.**

---

## Renames / supersessions vs canonical

| Canonical / older assumption | Current reality | Reconciliation |
| --- | --- | --- |
| `AGP_FerroniteDeposit : AGP_BuildingBase` | `AGP_ResourceNode : AActor` (Ore) | **Superseded for near-term MVP** by ResourceNode. Full building-as-pawn deposit deferred. |
| Ferronite-only enum/DA | `EGP_ResourceType::{None,Ore}` + Ferronite **tag** unused on node | Treat Ore node as Ferronite deposit stand-in until rename/alias stage; do not block Worker on DA. |
| `UGP_ResourceDefinition` required before mining | No DataAsset; amounts on node | Definition remains canonical metadata; thin rates may live on MiningComponent until S23-lite. |
| Worker under abstract mobile hierarchy | Concrete placeable is `AGP_Unit` (combat/infantry visual defaults) | Worker must **not** inherit `AGP_Unit`; sibling under `AGP_MobileUnit`. |
| DataAsset unit definitions drive visuals | Blueprint `VisualSourceMode=AuthoredComponents` | Author Worker presentation via BP subclass (S26B2A workflow). |
| Mine/Repair full command hooks | Mine validate-only; Repair tag-only | Need command/target + executor work before/with Worker. |
| Auto-cycle mining SM | Not in code | Belongs in MiningComponent stage, not invented inside Worker-only glue. |

---

## S23–S28 reconciliation matrix

| Stage | Canonical requirement | Current implementation | Status | Required follow-up | Proposed ownership |
| --- | --- | --- | --- | --- | --- |
| **S23** | `UGP_ResourceDefinition` Ferronite metadata | Absent | **Missing** | Optional thin DA or config later; not hard-block if rates temporary on MiningComponent | **GP-S26C** (metadata lite) or deferred post-S27 if rates hardcoded with TODO |
| **S24** | `AGP_FerroniteDeposit` capacity + soft-cap queue | `AGP_ResourceNode` amounts + `ConsumeResource`; no miner queue | **Superseded by current architecture** (ResourceNode) + **Partially** (capacity only) | Keep ResourceNode as deposit MVP; add miner soft-cap/queue when mining SM needs it; rename/alias Ferronite later | Deposit targeting + consume hooks: **GP-S26C**; queue soft-cap: with **GP-S26D** Mining |
| **S25** | `UGP_CargoComponent` | Missing; `CarriedFerronite` attr exists unused by economy | **Missing** (attr **Partial**) | Implement CargoComponent; decide attr vs component SoT (prefer component per TDD/07 CANONICAL) | **GP-S26D** |
| **S26** | `UGP_MiningComponent` auto-cycle SM | Missing; `ConsumeResource` exists | **Missing** | Implement SM; call ConsumeResource; approach deposit via movement | **GP-S26D** |
| **S27** | `AGP_Worker` mine+carry+repair, no Build | Missing actor; tags only | **Missing** | Assemble Worker on MobileUnit; wire commands; BP visual; no Storage | **GP-S27** (this analysis’s next implementation after prereqs) |
| **S28** | Storage drop-off + ThreatValue write | ThreatValue field only; no Storage | **Partially** (ThreatValue) / **Missing** (Storage) | MainBase Storage + drop-off mutation | **GP-S28** (out of S27) |

---

## Worker inheritance options

### Option A — `AGP_Worker : AGP_Unit`
- Pros: fastest placeable reuse (capsule + visual component already on Unit)
- Cons: inherits InfantryMelee-oriented concrete combat unit; muddies unit catalog; auto-attack / combat defaults live on sibling systems tied to “the” unit class; future Salvage Walker also wants a clean combat branch

### Option B — Worker under mobile base (existing `AGP_MobileUnit`), sibling of `AGP_Unit`
- Pros: **matches TDD/13** (`AGP_Worker : AGP_MobileUnit`); shares Move/command/ASC; independent visual BP; combat stays on `AGP_Unit` / future combat classes; minimal hierarchy invention (no new abstract base)
- Cons: must duplicate/own capsule (or shared helper) + visual component setup on Worker (small, explicit)

### Option C — role/config on `AGP_Unit`
- Pros: zero new actor class
- Cons: Infantry and Worker share one class forever; Blueprint authoring and AllowedCommands become tangled; worst for Salvage Walker / catalog growth

### Selected architecture
**Option B — `AGP_Worker : AGP_MobileUnit` (sibling of `AGP_Unit`).**

Justification:
- Canonical TDD/13 hierarchy
- Preserves S26B2A Blueprint authored visuals (`BP_Worker` with `VisualSourceMode=AuthoredComponents`)
- Keeps combat infantry (`AGP_Unit`) separate from economic Worker
- Reuses existing command/movement/ASC stack from `AGP_UnitBase` / `AGP_MobileUnit`
- Minimizes rework vs inventing a new mobile base (Option B “new base”) or collapsing roles (Option C)

`bAutoAttacks=false` must be introduced as an explicit Worker (or UnitBase) policy flag/command filter — not assumed from a missing `UGP_UnitDefinition`.

---

## Prerequisites (do not fold into GP-S27)

Large missing systems must **not** be silently absorbed into Worker.

### Required before GP-S27 implementation

#### GP-S26C — Resource Mine Target Compatibility
**In scope (proposed):**
- Allow `GP.Command.Mine` to target `AGP_ResourceNode` (not only `AGP_UnitBase` + capability tag)
- Document Ore node as Ferronite deposit stand-in for MVP (or add Ferronite alias without full BuildingBase rewrite)
- Ensure smart-command + validate + HeldCommand path can accept ResourceNode
- No full mining SM yet; no Worker actor; no Storage

**Out of scope:** auto-cycle, cargo, Worker BP, map population

#### GP-S26D — Cargo + Mining Foundation
**In scope (proposed):**
- `UGP_CargoComponent` (replicated cargo state; capacity)
- `UGP_MiningComponent` (server SM: move-to-deposit, mine via `ConsumeResource`, cargo fill; auto-cycle stubs toward base **without** Storage drop-off completion — or stop at “cargo full / idle notify”)
- Tick policy: no permanent component tick when idle; event/timer driven
- Authority: server mining + cargo mutation

**Out of scope:** `AGP_Worker` class assembly (S27); Storage / ThreatValue write (S28); Repair GA (S46)

### Deferred / not blocking Worker class creation
- Full `UGP_ResourceDefinition` DataAsset catalog (S23) — recommend thin rates on MiningComponent with explicit follow-up DA stage if needed
- `AGP_FerroniteDeposit` BuildingBase rewrite (S24 rename) — ResourceNode remains deposit MVP
- Storage / orbital delivery (S28 / S36 / ADR-0009)

### Exact proposed implementation split

```text
GP-S26C  Resource Mine Target Compatibility   (command ↔ AGP_ResourceNode)
GP-S26D  UGP_CargoComponent + UGP_MiningComponent
GP-S27   AGP_Worker actor + BP authored visual + command allow-list
GP-S28   UGP_StorageComponent drop-off + FerroniteThreatValue write
```

**Recommended next implementation task after this analysis is accepted:** **GP-S26C**.

---

## Exact GP-S27 Worker scope (when reached)

### In-scope
- Gameplay actor `AGP_Worker : AGP_MobileUnit`
- Mobile unit (movement component inherited)
- Explicit no-auto-attack / Attack command rejected or filtered (`bAutoAttacks=false` policy)
- Owns / requires Mining + Cargo capabilities (components from S26D)
- Repair capability **MVP**: command acceptance / stub wiring acceptable; full `UGP_GA_Repair` may remain S46 if called out
- Server-authoritative mining/cargo behavior
- Replicated gameplay state via existing patterns (cargo/mining as designed in S26D)
- Compatible with current Move/Stop command routing
- Compatible with `AGP_ResourceNode`
- Compatible with Blueprint-authored visuals (`VisualSourceMode`)
- Example `BP_Worker` (or equivalent) for operator validation — **implementation stage**, not this analysis

### Out-of-scope for GP-S27
- `UGP_StorageComponent` / drop-off completion economy
- Orbital delivery / container launch (ADR-0009 / S36)
- Build / Produce / Construction / local production
- Full unit catalog / Salvage Walker
- Projectile changes
- Map population / GameDefaultMap change
- Rewriting ResourceNode into BuildingBase FerroniteDeposit
- DataAsset visual profiles (abandoned S26B2A experiment)

### Networking rules
- Server authority for Mine execution, cargo mutation, ConsumeResource
- No client-authoritative cargo/mining
- VisualSourceMode remains class/default cosmetic (not gameplay RPC)
- Listen server + client must see same cargo/mine outcomes via replication

### Tick rules
- No permanent Worker actor tick for idle presentation
- Mining/cargo driven by timers/events/state changes (same discipline as movement/combat)
- Controller tick remains enabled; visual component tick remains disabled

### Blueprint visual workflow
- Prefer `BP_Worker` : `AGP_Worker` with `VisualSourceMode=AuthoredComponents`
- NativeFallback allowed for C++ debug placement
- Follow S26B2A authored component contract (NoCollision presentation meshes)

---

## Acceptance criteria (future GP-S27 implementation)

1. `AGP_Worker` placeable (native and/or BP) without combat auto-attack.
2. Select + Move works via existing command stack.
3. Attack / AttackMove rejected or no-op per Worker policy.
4. Mine accepted against `AGP_ResourceNode`; approaches and consumes under authority.
5. Cargo fills; deplete/idle behavior defined (even if drop-off waits for S28).
6. Listen + client replication coherent for cargo/mine state.
7. Authored BP visual works without native mesh overlay.
8. No Storage / ThreatValue write / Build / Produce in this stage.
9. No `.umap` persistence unless explicitly requested.

---

## Operator validation plan (future GP-S27)

1. Place `BP_Worker` (authored visual) in PIE / listen — do not save map unless asked.
2. Select Worker; issue Move — arrives.
3. Issue Attack against enemy unit — rejected / no auto-attack.
4. Issue Mine on `AGP_ResourceNode` — accepted; Worker approaches; `CurrentAmount` decreases; cargo increases.
5. Inspect cargo / mining state on server and client.
6. Place native `AGP_Worker` (if exposed) — NativeFallback visual OK.
7. Confirm visual component tick off; no authored collision/nav warnings.
8. Confirm ResourceNode box collision / nav unchanged by mining visuals.
9. Discard temporary placements (map unchanged).

---

## Risks

| Risk | Mitigation |
| --- | --- |
| Mine still UnitBase-only | Must ship GP-S26C before Worker validation |
| Scope creep: Storage inside Worker | Hard out-of-scope; stop at cargo-full / idle notify until S28 |
| Ore vs Ferronite naming confusion | Document stand-in; optional alias stage |
| Repair promises vs S46 GA | Limit S27 to command acceptance / stub; full ability later |
| Worker inherits AGP_Unit combat visuals | Selected Option B avoids this |
| Auto-cycle without drop-off target | Mining SM must define idle/notify when no Storage yet |
| `DOCUMENTATION_INDEX` stale vs log | Prefer AI_Project_Log + this analysis for NEXT; INDEX for doc priority rules only |

---

## Open questions

1. Should S26C introduce `GP.Resource.Node` tagging on ResourceNode, or a dedicated `IsResourceMineTarget` interface?
2. Cargo SoT: component-only vs dual-write `CarriedFerronite` attribute?
3. Is Repair in S27 accept-only, or minimal heal without OrbitalFerronite cost until S46?
4. When to rename Ore → Ferronite / ResourceNode → FerroniteDeposit for player-facing terms?
5. Soft-cap concurrent miners: required in S26D or only when multi-worker stress appears?

---

## Recommended next task

**GP-S26C — Resource Mine Target Compatibility** (implementation), then **GP-S26D — Cargo + Mining Foundation**, then **GP-S27 — AGP_Worker**.

Do **not** start GP-S28, orbital delivery, or Worker C++ in this analysis branch.
