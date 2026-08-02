# GP-S16 — UGP_SelectionComponent
(SelectedUnits, InspectedTarget, marquee, control groups)

## Slice Group
Slice 4 — Selection + Smart Commands (TDD/13: after GP-S15 Camera Input; before GP-S17 CommandComponent / GP-S18 UnitBase)

## Code Allowed
**Yes** — Phase A pure C++ state shell (this pass).

## Asset Changes Allowed
**No** on this pass. No Input Actions, IMC, maps, or Content changes.

## Depends On
- GP-S15 **DONE** — `AGP_PlayerController` Enhanced Input + camera lifecycle
- UnitBase prerequisite **merged** — compile-safe `AGP_UnitBase` UCLASS
- TDD/04 Detailed Selection Rules (GP-0202), TDD/13, TDD/09, TDD/12, TDD/15 (FoW deferred), ADR-0006

## Goal
Local-only selection state component owned by `AGP_PlayerController`:

- `SelectedUnits`
- `InspectedTarget`
- marquee state
- control groups 1..9
- selection delegates
- **no** replication
- **no** RPC
- **no** gameplay-state mutation

## Status
**PHASE_A_DONE_INTEGRATION_PENDING**

Phase A local state shell **completed** and technically accepted (builds + API audit). Checkpoint ready for merge.

**GP-S16 overall is NOT DONE.** Actual selection integration remains pending. No operator-visible actor selection exists yet.

Still absent / deferred (unchanged):

- no Input Actions / IMC
- no hit-testing
- no team / capability filtering
- no highlight
- no UI rectangle
- no FoW integration
- no camera focus
- no death delegate integration
- no building / unit rules enforcement
- no double-click selection

Do **not** set `Status: DONE` until integration acceptance.

### Phase A implemented

- Exact storage: `TArray<TWeakObjectPtr<AGP_UnitBase>> SelectedUnits`; `TWeakObjectPtr<AActor> InspectedTarget`
- Max selection **24**; control groups **1..9** (non-UPROPERTY `TStaticArray` of unit arrays)
- Local-only mutation (`AGP_PlayerController` + `IsLocalController`)
- No replication / RPC / component tick
- Default subobject on `AGP_PlayerController` + `GetSelectionComponent()`
- Marquee **state only** (`Begin/Update/End/Cancel`); `EndMarquee` keeps last coords, no hit-test
- Duplicate / invalid pruning + clamp
- One native multicast `FGPOnSelectionChanged`
- No input assets; no hit-testing; no filtering / highlight / UI / FoW
- GP-S17 **not** started; full GP-S18 **not** started

### Deferred integration (not Phase A)

- Filling selection from actual cursor hits
- Team ownership filtering
- Capability tags filtering
- Unit/building mixing rules
- Building single-only rule
- Selection highlight
- Death delegate
- Double-click same UnitDefinition
- Marquee world resolution and closest-24
- Control-group camera focus
- FoW visibility
- UI rectangle
- Input Actions / IMC

---

## 1. Goal (expanded)

Provide the local selection / inspect / marquee / control-group **state machine** that later UI (`UGP_SelectionVM`) and commands (`UGP_CommandComponent`) consume.

GP-S16 must **not**:

- issue commands;
- mutate GAS / attributes / tags on units;
- replicate selection;
- own UI widgets or Slate drawing;
- pretend `AGP_UnitBase` / highlights / FoW already exist.

---

## 2. Architecture placement

| Fact | Lock |
| --- | --- |
| Class | `UGP_SelectionComponent` |
| Base | `UActorComponent` |
| Module | `GPRuntime` |
| Owner | `AGP_PlayerController` **only** |
| Creation | `CreateDefaultSubobject` in `AGP_PlayerController` constructor |
| Tick | **Off by default** (`PrimaryComponentTick.bCanEverTick = false`) |
| Authority | Local controller only; **no** server authority requirement |
| Replication | None (`SetIsReplicatedByDefault(false)` / never enable) |
| UI module | **No** `GPUIRuntime` dependency |
| Commands | **No** direct command execution / no RPC |

Accessor on PC (eventual): `UGP_SelectionComponent* GetSelectionComponent() const;`

Naming/package paths follow existing `Public/Player/` + `Private/Player/` + `UGP_*Component` convention (`Naming_Conventions.md`).

---

## 3. Dependency gap analysis

`AGP_UnitBase` is scheduled at **GP-S18**. CapabilityTags on Unit/Building definitions, selection highlight MID flow, death delegates, and FoW actor visibility are not available as implemented gameplay contracts today.

| Gap | Safe to prepare in GP-S16 (if unblocked) | Explicitly deferred | Connects at |
| --- | --- | --- | --- |
| Typed `AGP_UnitBase` selection storage / API | **Cannot** — UHT/canonical type missing | Full typed containers + accessors | **GP-S18** (class exists) then finish S16 |
| Team ownership filtering via UnitBase | API hooks / comments only | Runtime `GetTeamId()` filter | GP-S18 + ownership contract |
| CapabilityTags Selectable/Inspectable checks | Tags **already registered** in `GPGASRuntime` (`GP.Capability.*`, `GP.Selection.Type.*`) — no new tag work in S16 | Reading tags from UnitDefinition/BuildingDefinition | Definition assets + GP-S18 UnitBase soft ref |
| `SetSelectionHighlight` calls | None | Call on add/remove/inspect | **GP-S18** |
| `OnDeath` subscriptions | None (no UnitBase delegate yet) | Per-unit bind/unbind | **GP-S18** |
| Double-click same `UnitDefinition` | None | Screen-frustum same-definition select | GP-S18 + definition assets |
| Building vs unit type routing | Rule tables in docs only | Mixed-selection / building single-only enforcement | GP-S18 (+ building tags later) |
| Concrete marquee candidate filtering | Screen-space drag **state** only | World/query filter owned+Selectable, exclude buildings/enemies, closest-24 | GP-S18 (+ optional later query helpers) |
| Fog of War visibility filtering | None | FoW-gated hit/marquee/inspect | FoW slice (TDD/15 / GP-S48 area) — **not** S16 |

**Hard ban for this stage:** no temporary fake classes, duplicate selection interfaces, or placeholder `AGP_UnitBase` invented inside GP-S16 to “unlock” typing.

---

## 4. Compile-safe GP-S16 scope proposal

### Preferred standalone shell (if storage were available)

- Component class + PC default subobject ownership/accessor
- Local state: selection list, inspect target, marquee drag rect, control groups 1..9, recall timestamps
- Hard cap `24`
- Delegates for selection/inspect change
- Clear / prune / EndPlay invalidation for weak refs
- Control-group assign / append / recall / append-to-selection / clear (container logic only)
- Duplicate removal + invalid weak pruning
- Public read-only accessors
- **No** hit-testing, world actor enumeration, highlight, ownership/capability filtering
- **No** input binding unless a later approved asset pass exists (see §11)

### Storage type decision (A vs B)

Canonical TDD/04 storage:

```cpp
UPROPERTY()
TArray<TWeakObjectPtr<AGP_UnitBase>> SelectedUnits;

UPROPERTY()
TWeakObjectPtr<AActor> InspectedTarget;

TStaticArray<TArray<TWeakObjectPtr<AGP_UnitBase>>, 9> ControlGroups;
```

| Option | Type | UHT / compile today | Architecture contract |
| --- | --- | --- | --- |
| **A** | `TWeakObjectPtr<AGP_UnitBase>` (+ arrays) | **Not compile-safe.** Forward declaration alone is insufficient for `UPROPERTY` / UHT: UHT requires a real `UCLASS` definition in a compiled module. No `AGP_UnitBase` header exists under `GP/Source`. | Canonical — **required** |
| **B** | `TWeakObjectPtr<AActor>` for selection list | Compile-safe (`AActor` exists) | **Non-canonical.** Silently changing the TDD type would force S17/S18/UI churn and hide the real dependency. **Rejected** for this stage. |

`InspectedTarget` as `TWeakObjectPtr<AActor>` **is** already canonical and would be compile-safe in isolation — insufficient to ship S16 goal (SelectedUnits + control groups are `AGP_UnitBase`-typed).

**Conclusion:** canonical typed GP-S16 **cannot** be implemented compile-safely before `AGP_UnitBase` exists. Do **not** substitute `AActor` storage. Mark dependency-blocked; adjust order before code.

### Proposed implementation-order correction

Keep Slice 4 intent, split UnitBase arrival:

1. **GP-S18a (new thin scaffold, before S16 code)** — minimal `AGP_UnitBase : APawn` abstract: TeamId stub, soft UnitDefinition ref placeholder, `OnDeath` multicast, empty `SetSelectionHighlight(bool)` stub, no MID implementation yet.
2. **GP-S16** — typed `UGP_SelectionComponent` state shell (this spec).
3. **GP-S17** — `UGP_CommandComponent` (still cannot fully resolve smart targets until more unit API exists — separate analysis).
4. **GP-S18b** — MID `EmissiveBoost` highlight implementation + SelectionComponent highlight call wiring + death subscribe integration.
5. **GP-S19** — `FGP_CommandRequest` + native tag mapping.

Alternative (simpler reorder): swap TDD lines so **GP-S18 UnitBase abstract** precedes **GP-S16 SelectionComponent**, then finish highlight integration as a follow-up note inside S18. Either way, **typed S16 code must not start until UnitBase UCLASS exists.**

---

## 5. Public API proposal (signatures only — not implemented)

```cpp
// Read
const TArray<TWeakObjectPtr<AGP_UnitBase>>& GetSelectedUnits() const;
AActor* GetInspectedTarget() const;
int32 GetSelectionCount() const;
bool HasSelection() const;
bool IsMarqueeActive() const;

// Clear
void ClearSelection();
void ClearInspectedTarget();
void ClearAllSelectionState(); // selection + inspect + cancel marquee

// Marquee (screen-space; no world resolve in S16 shell)
void BeginMarquee(const FVector2D& ScreenStart);
void UpdateMarquee(const FVector2D& ScreenCurrent);
void EndMarquee();      // may resolve later when deps exist; S16 stores/ends state
void CancelMarquee();

// Control groups (1..9 inclusive; invalid index = no-op)
void AssignControlGroup(int32 GroupIndex);           // overwrite from SelectedUnits
void AppendToControlGroup(int32 GroupIndex);         // Ctrl+Shift+N
void RecallControlGroup(int32 GroupIndex);           // replace selection
void AppendControlGroupToSelection(int32 GroupIndex);// Shift+N
void ClearControlGroup(int32 GroupIndex);

// Maintenance
void PruneInvalidEntries(); // selection + groups + inspect
```

**Not in API:** command builders, UI widget hooks, RPC, FoW queries, camera `FocusOnLocation` calls.

Optional later (post-S18, not S16 shell): `TrySelectActor(AActor*)`, `TryInspectActor(AActor*)`, additive/toggle helpers — only when filtering contracts exist.

---

## 6. Selection rules locked from TDD (no reinterpretation)

From TDD/04 §Detailed Selection Rules (GP-0202) + FoW note in GP-0202 task:

- Selection is **local-only**
- Cap **= 24**
- Owned selectable → `SelectedUnits`
- Enemy/neutral inspectable → `InspectedTarget`
- Inspect **does not** clear `SelectedUnits`
- Units and buildings **cannot mix** in `SelectedUnits`
- Buildings are **single-selection**
- Marquee **excludes buildings**
- Marquee **ignores enemies**
- Marquee overflow → **24 closest to cursor** (screen space)
- Additive selection **skips duplicates**
- Dead / invalid entries are **pruned**
- Ground / Esc clears selected **and** inspected
- Control groups: `Ctrl+1..9` assign; `Ctrl+Shift+N` append; `N` recall; `Shift+N` append recall
- Double-tap `N` within **0.4 s** requests camera focus (see §8)
- Control groups are local, **non-persistent**, **non-replicated**
- FoW restrictions **deferred** until FoW actor visibility contract exists (TDD/15)

---

## 7. Marquee boundary

| Layer | Responsibility |
| --- | --- |
| `UGP_SelectionComponent` | Stores drag active + screen start/current; optional marquee-changed notify; resolves final selection **only when** UnitBase/filter deps exist |
| `AGP_PlayerController` / input | Owns mouse Enhanced Input; starts/updates/ends drag; supplies cursor screen coords |
| UI (`GPUIRuntime`) | Draws rectangle later; **must not** live in GPRuntime component |

**GP-S16:** no Slate / UMG / CommonUI code.

---

## 8. Control-group camera focus boundary

SelectionComponent **must not** manipulate camera every frame.

**Chosen contract for later wiring:**

- SelectionComponent may emit a **focus request** (native multicast) carrying `FVector` centroid **or** return focus request data to PC.
- `AGP_PlayerController` later calls `AGP_CameraPawn::FocusOnLocation` when that API exists.

**GP-S16:** do **not** implement camera focus. `FocusOnLocation` is **not** confirmed on current `AGP_CameraPawn` (TDD/04 open question / camera API gap). Double-tap timing may be stored (`LastGroupRecallTimes[9]`) without performing focus.

---

## 9. Delegates

Minimal contract:

```cpp
DECLARE_MULTICAST_DELEGATE(FGP_OnSelectionChanged);
// Optional second delegate only if UI needs inspect-only without selection churn:
DECLARE_MULTICAST_DELEGATE(FGP_OnInspectedTargetChanged);
```

| Choice | Recommendation |
| --- | --- |
| Native multicast | **Yes** — C++ PC / future VM adapter |
| `BlueprintAssignable` / dynamic | **Defer** until UI adapter pass proves BP bind need (ADR-0006: no speculative surface) |
| Event count | Prefer **one** `OnSelectionChanged` covering selection **and** inspect (matches TDD/04: “Single delegate `OnSelectionChanged`”); add inspect-only only if profiling/UI requires it |

All mutating helpers that change visible selection/inspect state must go through a single internal `NotifySelectionChanged()` path.

---

## 10. Lifecycle and safety

- Valid / active only on **locally controlled** `AGP_PlayerController`
- Initialize without world scans
- **No** component tick
- Weak references only for actors
- `PruneInvalidEntries` before reads / recall / UI notify
- Clear on `EndPlay` / map travel
- Actor destruction: **do not** bind `AGP_UnitBase::OnDeath` until GP-S18; until then prune-on-access only (UI may lag — accepted pre-S18 gap)
- No cross-match persistence; no save game
- No replication of component or properties

---

## 11. Input assets boundary

Canonical docs name future Enhanced Input actions (TDD/04 pipeline, TDD/12 IMC list):

| Doc name | Role |
| --- | --- |
| `IA_Select` | LMB select / replace / inspect |
| `IA_Marquee` | LMB drag marquee |
| (control group keys) | `Ctrl+N` / `N` / Shift variants — may be key binds in `IMC_GP_Selection` |
| `IMC_GP_Selection` | Selection mapping context (TDD/12) |

**This pass / GP-S16 code (when unblocked):**

- **Do not** create IA/IMC assets
- **Do not** invent final soft paths without a locked OD pass (camera S15 style)
- **Do not** modify `IMC_GP_Camera`
- Selection **input integration** is a **separate operator/asset pass** (and PC bind work) after the component shell exists — **not** required for a pure state-component compile

Capability tags already exist in `GPGASRuntime`; no tag registration required for S16 shell.

---

## 12. Expected code files (eventual implementation)

| Path | Change |
| --- | --- |
| `GP/Source/GPRuntime/Public/Player/GPSelectionComponent.h` | New |
| `GP/Source/GPRuntime/Private/Player/GPSelectionComponent.cpp` | New |
| `GP/Source/GPRuntime/Public/Player/GPPlayerController.h` | Default subobject + getter only |
| `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp` | Construct subobject |

**Prerequisite file (order correction, not S16):**  
`GP/Source/GPRuntime/Public/Units/GPUnitBase.h` + `.cpp` (exact Units folder TBD at S18 — follow TDD Units placement when implemented).

No `GPRuntime.Build.cs` change expected for a local `UActorComponent`.

---

## 13. Build and validation plan (code pass, after unblock)

- `GPEditor Win64 Development`
- `GP Win64 Development`
- `GP Win64 Shipping`
- Pure component shell: **no** new assets required
- PIE: component present on local `AGP_PlayerController`; absent/unused on non-local
- 2P listen-server: selection state independent per window
- Confirm **no** selection replication traffic
- **No** map changes

Full gameplay select/marquee/highlight acceptance waits GP-S18 (+ input asset pass).

---

## 14. Acceptance criteria

### GP-S16 standalone (after UnitBase exists + code pass)

- [ ] Compiles three targets
- [ ] Component default-subobject on `AGP_PlayerController`
- [ ] Typed `SelectedUnits` / control groups / `InspectedTarget` storage
- [ ] Cap 24 + prune + clear helpers
- [ ] Marquee screen-space state begin/update/end/cancel
- [ ] Control-group container ops
- [ ] `OnSelectionChanged` fires on state mutation
- [ ] No tick / no replication / no RPC / no gameplay mutation
- [ ] No UI / no input assets in this slice unless separately assigned

### Deferred — GP-S18 integration

- [ ] Highlight via `SetSelectionHighlight`
- [ ] `OnDeath` subscribe/unsubscribe
- [ ] Team + CapabilityTags filtering
- [ ] Mixed unit/building rules enforced on real actors
- [ ] Marquee world resolve + closest-24
- [ ] Double-click same UnitDefinition

### Deferred — input / operator

- [ ] `IMC_GP_Selection` + IA assets at locked paths
- [ ] PC Enhanced Input bind (without breaking camera IMC)

### Deferred — UI

- [ ] Marquee rectangle draw
- [ ] `UGP_SelectionVM` / panels bind to delegate

### Deferred — FoW

- [ ] Visible-only select/inspect/marquee per TDD/15

---

## 15. Explicit non-goals

- no `AGP_UnitBase` implementation inside GP-S16
- no selection highlights
- no commands / RPC
- no UnitDefinition / BuildingDefinition schema changes
- no gameplay-tag registration (already present)
- no Input Action / IMC assets
- no UI rectangle
- no camera focus implementation
- no Fog of War implementation
- no map changes
- no GP-S17 / GP-S18 work disguised as S16
- no fake interfaces / placeholder UnitBase

---

## 16. Decision

### Final verdict: **BLOCKED_BY_GP-S18**

**Proof:**

1. Canonical `SelectedUnits` / ControlGroups type is `TArray<TWeakObjectPtr<AGP_UnitBase>>` (TDD/04, TDD/13).
2. No `AGP_UnitBase` UCLASS exists under `GP/Source` today.
3. `UPROPERTY` + UHT cannot reflect an unknown UCLASS from a forward declaration alone.
4. Substituting `AActor` would violate the architectural contract and is **rejected**.
5. Fake interface / placeholder duplicate class is **forbidden**.
6. Therefore canonical, compile-safe GP-S16 code **does not start** until a compile-safe `AGP_UnitBase` exists.

### Approved resolution (checkpoint)

- Suspend GP-S16 code.
- Prerequisite: minimal compile-safe `AGP_UnitBase` scaffold (separate tech-lead task; **not** full GP-S18).
- After scaffold: resume GP-S16 state shell (see Availability split above).
- Full highlight / death / filter / classification integration remains full GP-S18.
- GP-S17 not started.

**Code:** do not write on this branch until unblocked by that prerequisite task.

---

## Risks / open questions

1. Exact packaging of the UnitBase scaffold prerequisite — separate tech-lead task (do not invent ticket id here).
2. `FocusOnLocation` still absent on CameraPawn — keep focus as request/defer.
3. GDD/09 still mentions Q/E rotate vs MMB (historical); camera locked in S15 — out of S16 scope.
4. Selection soft asset paths not locked (unlike S15 camera) — needs OD pass before operator assets.

## Linked canonical docs

TDD/13, TDD/04, TDD/09, TDD/12, TDD/15, GDD/09, GP-0202, ADR-0006, GP-S15, Naming_Conventions, STYLE, CONTRIBUTING.

## Stop Condition

Status **PHASE_A_DONE_INTEGRATION_PENDING**. Phase A checkpoint complete. Do **not** create input assets / hit-testing / highlight / UI. Do **not** mark GP-S16 overall DONE. Do **not** start GP-S17 or full GP-S18. Await merge to `main`, then a separate integration assignment.
