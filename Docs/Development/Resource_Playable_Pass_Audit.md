# GP-S28P0 — Resource Playable Pass Audit and Implementation Plan

## Status
**GP_S28P0_AUDIT_READY_FOR_REVIEW**

## Baseline
- Branch: `audit/gp-s28p-resource-playable-pass`
- Base / main: `035c486758059032bb2551520834dd73f8667ef5` (Merge GP-S28)
- Audit commit: `d7710e8d7bda59793bc1c8c93363d58640465654`
- Stage type: **audit / specification only** — no production C++, Blueprint, map, or gameplay changes
- Parallel (do not modify): `audit/gp-slice7-combat-reconciliation` — **completed audit, pending review/merge** (not cancelled)

## Goal
Turn the already-implemented Worker → Mine → Cargo → MainBase Storage loop into a **player-facing playable prototype**, then extend it with safe automatic reassignment / failover — without rewriting confirmed S27/S28 systems.

---

## 1. Sources of truth used
- Docs README, DOCUMENTATION_INDEX, AI_Project_Log
- TDD/04, 05, 06, 07, 10, 12, 13
- GDD/02, 04, 05, 06
- ADR-0002, 0006, 0007, 0009
- CONTRIBUTING.md, STYLE.md
- Actual main code under `GP/Source/GPRuntime` (commands, Worker, Mining, Cargo, ResourceNode, Storage, GameState, PC)

Conflict rule applied: GDD = gameplay intent; TDD/ADR = architecture; main + AI log = implemented status.

---

## 2. Player-facing Mine command path

### End-to-end (implemented today)

| Step | Status | Evidence |
| --- | --- | --- |
| LMB select `AGP_Worker` | Works | `Capability_Selectable` + SelectionComponent; Worker sets Selectable/Inspectable/Unit/Worker tags |
| RMB world hit | Works | `AGP_PlayerController::OnCommandInputStarted` → Visibility line trace |
| Hit `AGP_ResourceNode` → smart Mine | Works | `BuildSmartCommand`: ResourceNode + `Resource_Node` tag → `GP.Command.Mine` |
| `Server_RequestCommand` | Works | PC → CommandComponent validate/dispatch |
| Ownership / team | Works | Issuing units filtered to requesting team; Mine further filtered to `AGP_Worker` only |
| Approach → `BeginMining` | Works | UnitCommand Mine executor + MiningComponent |
| Slot Granted / Waiting | Works | `RequestMiningSlot` → Mining / WaitingForSlot |
| CargoFull → haul → drop-off → return → remine | Works | UnitCommand haul chain (GP-S28) |

### Direct answers

| Question | Answer |
| --- | --- |
| Ordinary RMB on ResourceNode already works? | **Yes** — no modifier; smart command becomes Mine when hit actor is ResourceNode with `GP.Resource.Node`. |
| Modifier / separate command needed? | **No** for baseline Mine. Separate hotkey optional later; not required for P1. |
| Mixed selection (Workers + non-Workers)? | Smart command still tags Mine; **server drops non-Workers**; if ≥1 Worker remains → dispatch to Workers only; if zero Workers → `UnsupportedUnit`. |
| Multiple Workers? | **Yes** — each accepted Worker gets Mine; node soft-cap `MaxConcurrentMiners=4`; extras enter **FIFO waiting** on that node. |
| Can Move interrupt full mining/haul chain? | **Yes** — new Held Move replaces Mine/Haul (`ResetMineExecutorForReplacement` / haul cancel); slots released; cargo **kept**. |
| Exact gaps for playable pass | See §Gaps summary below. |

### Gaps (player-facing)
1. No authored BP meshes for Worker/MainBase/Node → console/diagnostic only feel.
2. No cargo mesh toggle wired to `OnCargoAmountChanged`.
3. Depleted node remains with no BP presentation event / no destroy-or-shell policy.
4. No automatic reassignment to another node when target full/depleted/destroyed.
5. Drop-off is single MainBase only; no WaitingForDropOff recovery when base missing (overflow currently LOST when storage full).
6. No player HUD for Planetary stored Ferronite.
7. Operator must hand-author a map (NavMesh, TeamIds, placements) — no content yet beyond PrototypeArena + examples.
8. Shift+command sets `bQueue`, but Held queue execution is **QueueDeferred no-op** (document; fix out of S28P unless explicitly assigned).
9. Cargo-full Worker rejects a **new** Mine unless already in haul chain for that deposit (expected for S28; reassignment must not fight this).

---

## 3. Blueprint readiness

| Class | UCLASS | Root | Notes |
| --- | --- | --- | --- |
| `AGP_Worker` | `Blueprintable` concrete | `UCapsuleComponent` | Safe `BP_GP_Worker : AGP_Worker`. No native mesh — add `UStaticMeshComponent`(s) attached to Capsule. Collision: QueryOnly Pawn, Visibility Block, **does not AffectNavigation**. |
| `AGP_MainBase` | `Blueprintable` concrete | Capsule | Safe `BP_GP_MainBase : AGP_MainBase`. Inherits BuildingBase Selectable/Inspectable. Storage + registry on TeamId≥1. |
| `AGP_ResourceNode` | `Blueprintable` concrete | `UBoxComponent` CollisionBox | Safe `BP_GP_ResourceNode_Ferronite : AGP_ResourceNode`. Visual via `UGP_ResourceNodeVisualComponent` (NoCollision parts). Box AffectsNavigation=true. Soft DA defaults to Ferronite. |

### Selection highlight
- Not a mesh component on the actor. PC draws selection for selected **units** with Capability tags.
- BP Worker/MainBase children remain selectable if they keep C++ CapabilityTags (do not clear tags in BP).
- ResourceNode is **not** a UnitBase — Mine via RMB hit, not selection membership.

### Cargo visual attach
- **Missing** dedicated `CargoVisualAnchor`. Worker Capsule is the only native attach root.
- Proposed: add SceneComponent anchor in P1 C++ (or BP child socket) — prefer C++ contract for stability.

### Existing Blueprint events / delegates
| Signal | Type | Notes |
| --- | --- | --- |
| `UGP_CargoComponent::OnCargoAmountChanged` | `BlueprintAssignable` Dynamic | Fires on authority mutate + `OnRep_CurrentCargoAmount` |
| `UGP_MiningComponent::OnMiningStateChanged` / `OnMiningCycleCompleted` | BlueprintAssignable (per MiningComponent) | Available for presentation polish |
| `UGP_StorageComponent::OnStorageChanged` | `BlueprintAssignable` Dynamic | Totals after add/remove / OnRep containers |
| `AGP_ResourceNode::OnMinerSlotStateChanged` | Native multicast only | **Not** BlueprintAssignable |
| Resource depleted BP event | **Absent** | Only `OnRep_CurrentAmount` log |
| Worker activity | `GetWorkerActivityState` BlueprintPure | No multicast on change |

### Proposed minimal C++ presentation contract (P1 — do not implement in P0)

| Symbol | Owner | Purpose |
| --- | --- | --- |
| `PresentationRoot` (`USceneComponent`) | Worker / MainBase / Node | Stable attach parent for BP meshes (Node may alias CollisionBox or dedicated scene) |
| `CargoVisualAnchor` (`USceneComponent`) | Worker | Attach cargo prop |
| `DropOffVisualAnchor` (`USceneComponent`) or DropOffZone ref | MainBase | Optional marker for approach/VFX; gameplay range remains `DropOffRangeCm` |
| `OnCargoVisualStateChanged(bool bVisible, float FillNormalized)` | Worker or thin presentation helper | Drive show/hide + optional scale |
| `CargoFillNormalized` | BlueprintPure → `Cargo->GetFillRatio()` | 0..1 |
| `OnResourceDepleted` | ResourceNode Dynamic multicast | Fire once when CurrentAmount crosses to ≤0 |
| Accessors | BlueprintPure | GetCargoComponent, GetStorageComponent, IsDepleted, GetTotalStored |

**Rule:** do **not** spawn a replicated cargo actor unless a later playtest proves a SceneComponent prop insufficient.

---

## 4. Cargo visual audit

| Topic | Finding |
| --- | --- |
| Replicated | `CargoCapacity`, `CurrentCargoAmount` (`OnRep_CurrentCargoAmount`) |
| OnRep | Broadcasts `OnCargoAmountChanged(Prev, New, Cap, Delta)` |
| Native/BP delegates | Dynamic `OnCargoAmountChanged` — BP-bindable |
| Mutations | Authority `AddCargo` / `RemoveCargo` / `ClearCargo` → `ApplyCargoAmount` → broadcast |
| Drop-off | Haul removes Accepted then may `ClearCargo` for overflow LOST |
| Late join | OnRep delivers current amount; bind in BeginPlay + handle initial amount |

### Recommended visual rule
```
Visible  := CurrentCargoAmount > KINDA_SMALL_NUMBER
Hidden   := !Visible
Fill     := GetFillRatio()   // optional scale/material
```
Bind `OnCargoAmountChanged` (and initial BeginPlay sync). No extra replicated flag required.

---

## 5. Resource depletion lifecycle (actual)

| Behavior when `CurrentAmount == 0` | Actual |
| --- | --- |
| Actor remains? | **Yes** — `ConsumeResource` does not Destroy |
| Accept new miners / Mine command? | **No** — `CanAcceptMineCommand` → `Depleted`; `IsDepositStateValidForMining` false |
| Notify active/waiting? | Active miners stop via mining cycle → `DepositDepleted`; occupancy released; waiting get EndPlay-style only if destroyed — **deplete does not auto-clear waiting queue by itself** until slots release / invalid cleanup |
| Occupancy cleanup | On EndPlay: snapshot → clear → notify (safe). On deplete: miners release via StopMining paths |
| Timers | Mining timer cleared on stop |
| Destroy actor? | **No** automatic destroy |
| Selection/inspection | Node not unit-selectable; still hittable until destroyed |
| Worker walking to depleted node | Approach may complete; `BeginMining` / Mine command validation fails → terminal Invalid / no remine |

### Recommended lifecycle (prototype-compatible)
```
Depleted (CurrentAmount <= 0)
  → reject new Mine / RequestMiningSlot
  → notify active + waiting (Release / Occupancy events + new OnResourceDepleted)
  → clear occupancy safely (same EndPlay discipline if needed)
  → Blueprint presentation (empty shell / hide ore accents)
  → policy choice:
       A) persistent depleted shell (recommended for prototype + GDD “finite deposits”)
       B) deferred Destroy after presentation delay (optional; must reassign Workers first)
```
**Recommendation for P2:** persistent depleted shell + `OnResourceDepleted` + force release/notify waiting; **no** silent Destroy in gameplay until content pass decides.

---

## 6. Mining slots and reassignment

### Current model (multi-slot, not binary)
| Item | Value |
| --- | --- |
| `MaxConcurrentMiners` | Default **4** |
| Active / Waiting | Authority `TArray` weak ptrs; counts replicated |
| FIFO | Waiting array order; `PromoteWaitingMiners` pops head |
| `RequestMiningSlot` | Granted / Waiting / AlreadyActive / AlreadyWaiting / Rejected* |
| Arrive while full | Worker must be **in interaction range** to `BeginMining` → WaitingForSlot **at node** (moves first, then queues) |
| Queue vs destroy | EndPlay clears queues, notifies, **no promote** during clearing |

### Reassignment algorithm A–E vs code

| Step | Desired | Exists? |
| --- | --- | --- |
| A Manual target valid + free slot | Use target | **Yes** (explicit Mine) |
| B Manual target full → nearest free node in radius | Search + retarget | **Missing** |
| C No free node → deterministic waiting target + FIFO | Join FIFO | **Partial** — FIFO only on **manual** node after arrive; no cross-node choose |
| D Slot released → head validates path → move | Promote + MiningComponent event | **Partial** — promote on **same** node; already in range when WaitingForSlot |
| E Node destroyed/depleted → leave queue → alt search / WaitingForResource | | **Partial** — destroy/deplete stops mining/haul cargo; **no** alt search / WaitingForResource state |

---

## 7. Resource search scope

| Option | Status | Notes |
| --- | --- | --- |
| GameState authority registry | **Missing** for nodes | MainBase registry pattern exists — **mirror this** |
| UWorldSubsystem | Missing | Heavier; avoid for MVP (ADR-0006) |
| `GetAllActorsOfClass` in production loop | Used in **diagnostics only** | Ban for gameplay tick/command hot path |
| Node self-register BeginPlay/EndPlay | Missing | Preferred with GameState list |

### Recommended minimal architecture
- `AGP_GameState` authority `TArray` of `AGP_ResourceNode*` (or weak) with Register/Unregister from Node BeginPlay/EndPlay.
- Query API: `FindReachableResourceNodes(TeamContext, Origin, Radius, OutSorted)`.
- Parameters (DataAsset or temporary EditDefaults on Worker/Mining):
  - `ResourceSearchRadius`
  - `MaxResourcePathCost` (or max path length / rejected if no Nav path)
  - `bAllowManualTargetOutsideAutoSearchRadius` (default **true** — manual Mine always allowed if valid)
  - Deterministic tie-break: path cost → distance → actor name/net GUID

**Selection must require navigable path**, not Euclidean-only.

---

## 8. Drop-off architecture

| Question | Answer |
| --- | --- |
| Only one MainBase per team? | **Yes** — GameState registry uniqueness (playable TeamId) |
| Other storage/drop-off actors? | **No** in code today |
| LogisticsHub drop-off? | Canonical TDD/06: `bProvidesDropOff=false` — **storage-cap / unit-cap provider only**, not drop-off |
| Multiple Ferronite drop-off points? | Not in MVP canon; MainBase is drop-off. Future only if design changes |
| GP-S36–S40 impact | S36 launch on MainBase Storage; S39 content MainBase; S40 LogisticsHub cap — **do not** invent Hub drop-off in S28P |

### Current path
`Worker haul → GameState::FindMainBaseForTeam → Storage.AddPlanetaryFerronite → Threat += Accepted × ThreatPerUnit` (overflow LOST).

### Interface recommendation
**Do not introduce `IGP_FerroniteDropOff` in S28P.** Premature vs LogisticsHub canon.  
**P3 scope slim:** MainBase-only resilience:
- missing / dead / unreachable MainBase → `WaitingForDropOff`, **keep Cargo**
- storage full → keep current LOST policy **or** optional wait (product decision; default keep LOST for S28 compat)
- wake on MainBase register / TeamId refresh events (GameState registry hooks)

---

## 9. Waiting semantics (event-driven)

### Exists
- Per-node FIFO waiting + `OnMinerSlotStateChanged` (native)
- MiningComponent binds occupancy; promotion → start timer
- Command replace / EndPlay clear slots
- Haul `WaitingForStorage` enum exists but S28 overflow policy is LOST (enum largely reserved)

### Missing
- `WaitingForResource` (cross-node)
- Drop-off registered/unregistered wake
- Low-frequency safety retry timer (optional)

### FIFO invariants to keep
- Queue per ResourceNode
- Prune invalid weak entries
- One Worker in at most one node queue
- Command replacement removes from queue
- EndPlay clears without promote
- Deterministic head order

**No permanent Tick** for waiting — occupancy delegates + optional ≤1 Hz recovery timer only if events can be missed.

---

## 10. Minimal resource HUD

| Source | Use? |
| --- | --- |
| `UGP_StorageComponent::GetTotalStored()` on local team MainBase | **Yes** — Planetary stored |
| `FerroniteScore` / `OrbitalFerronite` | **No** |
| Per-frame actor iteration | **No** |
| Replicated total | Containers replicated on MainBase Storage; `GetTotalStored()` derived; `OnStorageChanged` + OnRep |
| Local TeamId | `AGP_PlayerState::GetTeamId()` |
| `FindMainBaseForTeam` on clients | **Blocked today** — MainBase registry is **authority-only / not replicated**. P4 must add a client-safe resolve (replicate weak MainBase per team, or one-time team-tagged MainBase cache + `OnRep_TeamId` refresh). |

### Choice (amended): **A′ — TEMP HUD with client-safe MainBase resolve**
- Prefer smallest debt that still respects TDD/12 (HUD must not poll random actors each frame).
- **P4 must ship** a replicated or otherwise client-visible MainBase handle for local team before any widget.
- UI surface: tiny TEMP text widget (PC-owned) bound to `OnStorageChanged` / container OnRep — mark `TEMP_S28P_HUD`.
- Optional upgrade path (not required in P4): thin `UGP_PlanetaryStorageVM` (option B-lite) if operator wants Slice-9 shape early.
- Reject: Score/Orbital display; GetAllActorsOfClass every tick; inventing LogisticsHub as storage source.

---

## 11. Operator level workflow (after implementation — do not do in P0)

1. Create Blueprints:
   - `BP_GP_Worker` : `AGP_Worker`
   - `BP_GP_MainBase` : `AGP_MainBase`
   - `BP_GP_ResourceNode_Ferronite` : `AGP_ResourceNode` (confirm Ferronite soft DA)
2. Add primitive StaticMesh children under PresentationRoot / Capsule / CollisionBox; **visuals NoCollision**; do not replace Capsule/Box roots.
3. Worker Capsule: keep QueryOnly + Visibility Block; nav off.
4. ResourceNode CollisionBox: keep Block/nav as C++ defaults unless playtest requires tweak.
5. Place **one** MainBase per team; set `TeamId` 1 and 2 (EditInstanceOnly). Do **not** duplicate MainBase same team (registry rejects).
6. Place Workers with matching TeamId; place several ResourceNodes with amount > 0.
7. Add `NavMeshBoundsVolume` covering playable area; build paths.
8. `PlayerStart` + GameMode override to project GP GameMode if needed.
9. Save map under `/Game/GrimProtocol/Maps/…`; ensure `.umap` tracked via Git LFS per `.gitattributes`.
10. PIE: select Worker → RMB node → verify haul loop; stop PIE clean.

**Do not duplicate:** two MainBases same TeamId; overlapping exclusive nav blockers without rebuild; committing huge non-LFS binaries.

---

## 12. Proposed implementation sequence

### GP-S28P1 — Blueprint-ready actors + cargo visual hooks + RMB Mine validation
**Scope:** PresentationRoot / CargoVisualAnchor / DropOffVisualAnchor; `OnCargoVisualStateChanged` helper or BP-bindable wrapper; `OnResourceDepleted` stub OK if tiny; document BP creation; confirm RMB Mine with BP classes in PIE.  
**Not:** reassignment, multi-drop-off, HUD, launch.  
**Files:** Worker, MainBase, ResourceNode (+ maybe thin presentation component).  
**Tests:** existing S28 suite still Failures=0; new smoke: cargo OnRep shows/hides.  
**Operator:** create three BPs + temp map placements.  
**Depends:** main @ S28.  
**Accept:** RMB Mine on BP node works; cargo mesh toggles with amount.

### GP-S28P2 — Depletion + resource reassignment + FIFO waiting polish
**Scope:** Depleted lifecycle + `OnResourceDepleted`; GameState ResourceNode registry; path-aware search A–E; WaitingForResource; keep per-node FIFO.  
**Not:** drop-off multi-provider; HUD; combat.  
**Tests:** full-slot reassignment; deplete → alt node; destroy node → reassignment; FIFO promote unchanged.  
**Depends:** P1.  

### GP-S28P3 — Drop-off resilience (MainBase-only)
**Scope:** WaitingForDropOff when MainBase missing/unreachable; wake on registry; keep Cargo; **no** `IGP_FerroniteDropOff` unless design explicitly adds second drop-off later.  
**Not:** LogisticsHub drop-off; launch/Orbital/Score.  
**Depends:** P2 (or P1 if only base-missing — prefer after P2).  
**Rationale for slim P3:** TDD LogisticsHub `bProvidesDropOff=false`; multi-provider interface would be speculative debt.

### GP-S28P4 — Minimal Planetary HUD + full playable validation
**Scope:** Client-safe MainBase resolve for local team; TEMP Planetary HUD (A′); operator map checklist; full loop validation both teams; regression suite.  
**Not:** Full Slice 9 MVVM stack (optional B-lite only if explicitly wanted).  
**Depends:** P1–P3.  
**Accept:** playable prototype demo without console spawns; client+listen see correct Planetary totals.

### Split justification
User’s P1–P4 order retained; **P3 narrowed** (no drop-off interface) to match canonical MainBase/LogisticsHub split. If audit review demands Hub-as-drop-off later, that becomes a separate design ADR — out of S28P.

---

## 13. Blocking questions / conflicts

1. **Storage full:** keep S28 LOST vs introduce WaitingForStorage for prototype? Default: **keep LOST** unless operator requests wait.
2. **Depleted shell vs Destroy:** recommend shell; confirm before P2.
3. **Combat audit branch** remains separate pending review — S28P must not block or rewrite combat.
4. DOCUMENTATION_INDEX on main was stale (claimed NEXT=S16); this audit re-syncs cursor to S28P0 + notes combat audit.
5. **HUD client resolve:** MainBase registry not replicated — P4 must fix resolve before TEMP HUD (follow-up from playable-path inventory).
6. **Shift-queue:** document as known no-op; do not expand into S28P unless assigned.

---

## 14. Implementation status

**P0 audit:** documentation only (this file).

### Addendum — post-P0 implementation (do not rewrite audit body)

| Stage | Status |
| --- | --- |
| GP-S28P1 | **DONE / MERGED** to `main` @ `86bcc9740fde0f19ac40c70f2f49298680f5f7d6` |
| GP-S28P2 | **DONE / MERGED** to `main` @ `e90b7bd48fb9080a881e6dda7be889eaa99a3161` — depletion / registry / approach / reassignment / FIFO / partial-cargo. Details: `Claude_Tasks/GP-S28P2_Depletion_Resource_Reassignment.md` |
| GP-S28P3 | **SPEC_READY_FOR_REVIEW** on `docs/gp-s28p3-dropoff-resilience-spec` — WaitingForDropOff / MainBase missing-unreachable recovery (no Hub drop-off). Details: `Claude_Tasks/GP-S28P3_DropOff_Resilience.md` |
