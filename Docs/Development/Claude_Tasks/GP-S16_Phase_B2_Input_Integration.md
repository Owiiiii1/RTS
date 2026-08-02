# GP-S16 Phase B2 — Input Integration
(Click select / inspect / marquee — blocked by team assignment)

## Status
**Status: B2B_DONE**

Phase B2 architecture **merged**.
B2a **merged** and operator-validated.
B2b marquee **implemented and operator-validated** — **complete**.
`IA_Select` reused; `IA_Marquee` **absent** (rejected).
GP-S16 overall remains **NOT DONE** (later GP-S16 phases may remain). Do **not** start GP-S17 or full GP-S18.

Parent GP-S16 selection status:
**`PHASE_B2_DONE_NEXT_PHASE_PENDING`**

### B2b operator validation (passed)

| Check | Result |
| --- | --- |
| Marquee rectangle aligned with cursor | **PASS** |
| Drag all directions | **PASS** |
| DPI drift | **NONE** |
| Rectangle hidden after release/cancel | **PASS** |
| Selected green / inspected yellow debug boxes | **PASS** |
| Click + marquee update boxes | **PASS** |
| Enemy/neutral excluded from friendly selection | **PASS** |
| Replace / Shift Add / Ctrl Toggle / Ctrl precedence | **PASS** |
| Empty Replace clears; empty Shift/Ctrl no-op | **PASS** |
| Fast-release fallback | **PASS** |
| B2a / camera regression | **NONE** |
| Tick / log spam | **NONE** |
| Standalone | **PASS** |
| 2-player listen-server + local isolation | **PASS** |
| Replication / RPC warnings | **NONE** |
| Maps saved / assets created | **NO** |

### B2b validation remediation (coordinate + temp boxes) — done

| Item | Detail |
| --- | --- |
| Marquee visual defect | Rectangle offset vs cursor; candidate selection coordinates were correct |
| Cause | Paint treated viewport-local **physical** pixels as Slate-local via `LocalToAbsolute`/`AbsoluteToLocal` |
| Fix | `Local = ScreenPixels / max(GetViewportScale(), KINDA_SMALL_NUMBER)`; full-viewport anchors; no desktop absolute path |
| Contract | PC stores viewport-local physical pixels; widget converts via DPI scale — verified |
| Temp debug boxes | Local-only `DrawDebugBox` — selected **green**, inspected UnitBase **yellow** (validation-only; production highlight deferred) |

### B2b implemented (this pass)

| Item | Detail |
| --- | --- |
| Input | Reuse `IA_Select`; gated marquee Tick while press; idle Tick may draw bounded local debug boxes only |
| Threshold | PressPending → MarqueeActive when distance > **8 px**; fast-release fallback on Completed |
| Widget | Pure C++ `UGP_MarqueeSelectionWidget` (`NativePaint`, HitTestInvisible, local-only) |
| Sources | `GPRuntime/.../UI/GPMarqueeSelectionWidget.h/.cpp`; PC + `GPRuntime.Build.cs` |
| Dependencies | Private `Slate` / `SlateCore`; public `UMG` unchanged |
| DPI | PC stores viewport-local **physical** pixels (`GetMousePosition`); widget paints `pixels / GetViewportScale()` as Slate local — **no** `AbsoluteToLocal` |
| Candidates | `TActorIterator<AGP_UnitBase>`; project actor location; center-in-AABB; sort `GetPathName()` |
| Eligibility | LocalTeam ≥ 1; same TeamId; `IsGameplaySelectable()`; no FoW/LOS/render heuristics |
| Modifiers | Replace / Shift Add / Ctrl Toggle (Ctrl wins); empty Replace clears; empty Shift/Ctrl no-op |
| Cap | `SetSelectionFromUnits` → SelectionComponent dedupe/cap **24** |
| Component | Existing `Begin/Update/End/CancelMarquee` unwired → wired; API unchanged |
| Inspect | Clear before apply (may double-broadcast); empty Shift/Ctrl leave inspect |
| MP | Local-only; no RPC; no replicated marquee/selection |
| Perf | Marquee actor scan once on release; debug boxes iterate ≤24 selected (+ optional inspect) |
| Logging | One-shot `GP Marquee: ... Result=Applied\|NoOpEmpty\|BlockedUnassignedTeam\|Canceled` |
| `IA_Marquee` / HUD / BP widget | **Not** created |

### B2a implemented (complete)

| Item | Detail |
| --- | --- |
| Assets | `IA_Select` (Boolean), `IMC_GP_Selection` (LMB → IA_Select) |
| Soft paths | `/Game/GrimProtocol/Input/Selection/IA_Select.IA_Select`, `.../IMC_GP_Selection.IMC_GP_Selection` |
| IMC priority | Selection **110**; Camera **100** unchanged |
| Lifecycle | Separate soft-load / bind / BeginPlayingState add / EndPlay remove; local only |
| Press/release | Started stores screen pos; Completed click if distance ≤ **8 px**; else B2b marquee |
| Canceled | Clears pending press only |
| Trace | `DeprojectScreenPositionToWorld` + `LineTraceSingleByChannel(ECC_Visibility)`; ignore controlled pawn; distance `1000000`; complex=false |
| Modifiers | Ctrl / Shift via `IsInputKeyDown` (both left/right); Ctrl wins |
| Friendly selectable | Replace / Shift Add / Ctrl Toggle; clear inspect first (may double-broadcast) |
| Enemy/neutral inspectable | `SetInspectedTarget`; selection unchanged; modifiers ignored |
| Ground / non-unit | `ClearAllSelectionState` |
| Unassigned unit (`TeamId < 0`) | Fail closed; no clear |
| Unassigned local player | Fail closed; no invent TeamId |
| Diagnostics | One-shot `LogTemp` line per processed click |
| No RPC / local-only | Yes |
| `IA_Marquee` | **Rejected** — do not create; B2b reuses `IA_Select` |

### B2a operator validation (passed)

| Check | Result |
| --- | --- |
| Plain friendly click / replace | **PASS** |
| Shift add | **PASS** |
| Ctrl toggle | **PASS** |
| Shift+Ctrl uses Ctrl precedence | **PASS** |
| Enemy inspect | **PASS** |
| Neutral inspect | **PASS** |
| Friendly click clears inspected target | **PASS** |
| Ground / non-unit clear | **PASS** |
| Drag below 8 px behaves as click | **PASS** |
| Drag above 8 px: no selection mutation; deferred to B2b | **PASS** |
| Camera pan / zoom / rotation regression | **NONE** |
| Selection asset load errors | **NONE** |
| 2-player host classification | **PASS** |
| 2-player client classification | **PASS** |
| Local selection isolation | **PASS** |
| Selection / replication warnings | **NONE** |
| Map saved / additional assets | **NO** |

### Team assignment (merged; prior validation)

| Item | Detail |
| --- | --- |
| Allocator | `AGP_GameMode::NextPlayableTeamId` (server-only, starts at `1`) |
| Helper | `AssignPlayableTeamId(APlayerController*)` |
| Assignment point | `PostLogin` **before** `TryStartMatch` |
| Order | `Super::PostLogin` → authority → assign → count log → `TryStartMatch` |
| Playable IDs | Start at `1` |
| Neutral / unassigned | `0` / `-1` receive next playable |
| Preassigned `>= 1` | Preserved; allocator advances past it |
| Monotonic / no reuse | Logout does not decrement; no renumber |
| Transport | Existing PlayerState TeamId replication (no RPC) |

Phase B2 input integration (**B2a + B2b**) is **complete**. GP-S16 overall still **NOT DONE**.

---

## Relationship

| Stage | State |
| --- | --- |
| Phase A selection state shell | **Merged** |
| Phase B1 mutation API | **Merged** |
| Selectable UnitBase prerequisite | **Merged** |
| Phase B2 analysis | **Complete** (this doc) |
| Player TeamId assignment | **DONE** (operator-validated) |
| Phase B2a click / inspect | **DONE** (operator-validated) |
| Phase B2b marquee architecture | **Merged** |
| Phase B2b marquee implementation | **DONE** (operator-validated) |
| GP-S17 / full GP-S18 | Not started |

---

## Approved GameMode team-assignment prerequisite

Separate implementation slice (not this checkpoint).

### `AGP_GameMode`

- Assigns each connecting **playable** player a **unique** TeamId
- Assignment is **server-authoritative**
- Hook: `PostLogin` or existing canonical login initialization path
- Calls `AGP_PlayerState::SetTeamId`
- Host and remote clients receive valid TeamIds
- Playable TeamIds start at **`1`**
- **`0`** remains neutral
- **`-1`** remains unassigned / invalid
- Assignment does **not** depend on client controller index
- **No** client RPC
- PlayerState replication delivers the value to clients

Do **not** invent a complex lobby / team-selection architecture in that slice.

### Contract notes

- This is **deterministic connection-order assignment** for the current foundational slice only
- Future lobby / team selection may replace the **source** of assignment
- Public contract remains `AGP_PlayerState::TeamId`
- Must **not** introduce a second independent team identity source

### Disconnect / reconnect boundary (minimal prerequisite)

| Topic | Decision |
| --- | --- |
| Reuse TeamId within match | **Not required** |
| Monotonic next playable TeamId | **Allowed** |
| Disconnect renumbers existing players | **No** |
| Reconnect without rejoin identity | Receives a **new** TeamId |
| Persistent team identity | **Deferred** |
| Seamless travel policy | **Deferred** |
| Lobby-selected teams | **Deferred** |

Do not implement disconnect/reconnect policy in the B2 analysis checkpoint.

---

## B2 split lock (after team assignment)

B2a and B2b are **separate reviewed implementation checkpoints**.
No temporary throwaway UI.
No marquee implementation inside the team-assignment prerequisite.

### B2a — click selection / inspect

Scope:

- `IA_Select`
- `IMC_GP_Selection`
- Local-controller mapping lifecycle
- Visible cursor
- Visibility trace (`ECC_Visibility`)
- Click vs drag threshold **state** (no marquee resolve yet)
- Plain replace / Shift add / Ctrl toggle / Shift+Ctrl → Ctrl precedence
- Friendly + gameplay selectable → `SelectedUnits`
- Enemy/neutral + inspectable → `InspectedTarget`
- Enemy selectable tag **never** enters `SelectedUnits`
- Ground / non-unit → `ClearAllSelectionState`
- Concise one-shot diagnostic logging
- **No** marquee candidate resolution

### B2b — marquee

Scope (architecture locked in **B2b architecture lock** below; implementation pending):

- Reuse `IA_Select` — **no** `IA_Marquee`
- Screen rectangle widget
- Candidate resolution + eligibility filter
- Modifier integration + cap / order
- Operator validation after implementation

---

## Input contract lock (B2a assets exist; B2b creates none)

```text
/Game/GrimProtocol/Input/Selection/IA_Select          (exists — reused by B2b)
/Game/GrimProtocol/Input/Selection/IMC_GP_Selection  (exists)
```

| Lock | Value |
| --- | --- |
| Selection IMC priority | **110** |
| Camera IMC priority | **100** |
| Contexts | Separate (`IMC_GP_Selection` ≠ `IMC_GP_Camera`) |
| Lifecycle | Mirror camera: soft refs → `LoadSynchronous` → bind `SetupInputComponent` → add local `BeginPlayingState` → remove `EndPlay` |
| Controller | Local only |
| Hit path | PlayerController cursor trace / projection — **no** actor `OnClicked` delegates |
| Trace channel (click) | `ECC_Visibility` |
| RPC | None |
| Selection state | Non-replicated |
| `IA_Marquee` | **Rejected** — do not create |

B2b architecture checkpoint creates **no** assets.

---

## Click rule lock (future B2a)

### Friendly + gameplay selectable

- Plain click → replace
- Shift → add
- Ctrl → toggle
- Shift+Ctrl → **Ctrl wins**

### Enemy or neutral + inspectable

- Set inspected target
- Selected units remain unchanged
- Modifiers ignored

### Enemy + selectable tag

- **Never** add to `SelectedUnits`

### Ground or non-`AGP_UnitBase` actor

- Clear `SelectedUnits` and `InspectedTarget` (`ClearAllSelectionState`)

### Friendly but not selectable

- Inspect only if inspectable; otherwise no selection mutation

### Unassigned local PlayerState

Fail closed:

- Do **not** select friendly units
- Do **not** invent TeamId
- Diagnostic warning allowed
- This state must **not** occur after the team-assignment prerequisite is complete

---

## Multiplayer / local policy

- Input bindings and IMC: local controller only
- Selection mutations: local-only component policy
- No selection RPC
- Cursor trace does not require authority
- TeamId read locally from replicated PlayerState / UnitBase
- One client must not mutate another client's selection

---

## Fog of War boundary

- No FoW implementation in B2
- After team assignment, temporary rule: all relevant actors treated as visible
- Future FoW gate occurs **before** selection mutation
- No permanent visibility bool on UnitBase
- Hidden-selection cleanup deferred

---

## B2b architecture lock (analysis complete — implementation pending)

Base verified: `main` contains `9bf7f4d` Merge GP-S16 B2a click selection.
Analysis branch: `feature/gp-s16-b2b-marquee-selection`.
**No C++ / Build.cs / assets / maps / config changed in this checkpoint.**

### Existing B2a facts (must preserve)

| Fact | Source |
| --- | --- |
| `IA_Select` Boolean + `IMC_GP_Selection` LMB; priority **110** | Assets + PC |
| Started stores press via `GetMousePosition`; Completed compares release; threshold **8.0f** | `AGP_PlayerController` |
| Drag `>8` currently logs `DragDeferredToB2b` and returns (no mutation) | `OnSelectionCompleted` |
| Canceled clears press only | `OnSelectionCanceled` |
| `PrimaryActorTick.bCanEverTick = true`; **no** `Tick` override today | PC ctor |
| SelectionComponent marquee APIs exist but are **unwired** from PC | `Begin/Update/End/CancelMarquee` |
| Marquee APIs do **not** broadcast `OnSelectionChanged` | SelectionComponent |
| No HUD / widget / overlay classes in `GPRuntime` / `GPUIRuntime` | Source scan |
| `GPRuntime.Build.cs` already has public `UMG`; **no** `Slate` / `SlateCore` listed | Build.cs |
| `HUDClass` unset on `AGP_GameMode` | GameMode ctor |
| Camera: `AGP_CameraPawn` spring-arm + camera; PC uses `DeprojectScreenPositionToWorld` for click | CameraPawn / PC |

### 1) Input update mechanism

| Decision | Value |
| --- | --- |
| `IA_Marquee` | **Rejected** — reuse `IA_Select` |
| Press / release | Existing `Started` / `Completed` / `Canceled` |
| Cursor while held | Override `AGP_PlayerController::Tick` |
| Why Tick | Controller tick already enabled for Enhanced Input; no mid-hold Triggered needed; minimal new surface |
| Tick work while LMB held / marquee active | `GetMousePosition` → threshold / `UpdateMarquee` → widget geometry only |
| Idle Tick | No selection work (no trace, no candidate scan) |
| Do **not** disable | `PrimaryActorTick.bCanEverTick` |

### 2) Rectangle visualization

| Decision | Value |
| --- | --- |
| Approach | Pure C++ `UUserWidget` subclass; `CreateWidget` — **no** Widget Blueprint asset |
| AHUD / DebugCanvas / on-screen debug | **Rejected** |
| Visibility | Added only for **local** controller; `Collapsed` / hidden when inactive |
| Input | `ESlateVisibility::HitTestInvisible` — must not capture mouse |
| Draw | `NativePaint`: translucent fill + border in screen/widget space |
| Class | `UGP_MarqueeSelectionWidget` |

**Proposed new source files (implementation pass only):**

```text
GP/Source/GPRuntime/Public/UI/GPMarqueeSelectionWidget.h
GP/Source/GPRuntime/Private/UI/GPMarqueeSelectionWidget.cpp
```

**Also modify (implementation):** `GPPlayerController.h/.cpp`, `GPRuntime.Build.cs`
**Do not modify:** `GPSelectionComponent`, GameMode, PlayerState, UnitBase, Unit, camera, maps, config, `.uproject`.

### 3) Build.cs dependency decision

| Module | Decision |
| --- | --- |
| `UMG` | Already public — keep |
| `Slate`, `SlateCore` | **Add** as `PrivateDependencyModuleNames` for `NativePaint` / Slate geometry includes |
| Move widget to `GPUIRuntime` | **Rejected** for this slice (selection stack stays in `GPRuntime`) |

### 4) DPI / coordinate space

| Rule | Value |
| --- | --- |
| Cursor API | Same as B2a: `GetMousePosition` — viewport-local **physical** pixels |
| Projection | `ProjectWorldLocationToScreen` into the **same** physical viewport pixel space (selection unchanged) |
| Widget layout | Full-viewport anchors via `SetAnchorsInViewport(0,0,1,1)` on pure C++ overlay |
| Stored rect | Physical pixels from PC — **not** desktop-absolute |
| Paint mapping | `Local = ScreenPixels / max(GetViewportScale(), KINDA_SMALL_NUMBER)` then draw in widget local space |
| Forbidden | `AbsoluteToLocal(raw GetMousePosition)` / desktop window offset / magic constants |

### 5) Candidate resolution

| Option | Verdict |
| --- | --- |
| A — iterate `AGP_UnitBase` + project point | **Accepted** |
| B — frustum / world overlap | Rejected (complexity / terrain / collision coupling) |
| C — `AHUD::GetActorsInSelectionRectangle` | Rejected (requires HUD/Canvas; project has no HUD; bounds semantics differ) |

**Algorithm (once on release only):**

1. Build axis-aligned `FBox2D` from marquee start/current (min/max; order-independent).
2. `TActorIterator<AGP_UnitBase>` (or equivalent world iteration).
3. Skip invalid / pending-kill.
4. Eligibility filter (below).
5. `ProjectWorldLocationToScreen(Unit->GetActorLocation(), ScreenPos)`.
6. Include if projected point is inside the rectangle (inclusive edges).

**Selection point:** actor location. For `AGP_Unit`, root capsule means this is the capsule center — acceptable RTS center-point rule for this slice.

**Inclusion rule:** **center-point inside rectangle** (not projected bounds overlap). Partial silhouette without center inside = **not** selected. Predictable foundational semantics; bounds-overlap may be revisited later.

### 6) Deterministic ordering

| Rule | Value |
| --- | --- |
| World iterator order | **Not** trusted |
| Sort key | Ascending `GetPathName()` after eligibility + inclusion |
| Cap | `UGP_SelectionComponent` `PruneAndClamp` / `SetSelectionFromUnits` keeps first **24** of submitted array |
| Over-cap | Submit sorted array; component clamps; one-shot log includes `Eligible` and post-apply `SelectedCount` / note truncation when `Eligible > 24` on Replace path |

### 7) Eligibility filters

Include only when **all** true:

- Local `AGP_PlayerState::GetTeamId() >= 1` (else fail-closed; no marquee apply)
- `Unit.TeamId == LocalTeamId`
- `Unit.IsGameplaySelectable() == true`

Exclude: enemy, neutral, `TeamId < 0`, non-selectable, invalid/pending-kill.
**Do not** use `WasRecentlyRendered`, occlusion, or LOS.
FoW absent: all relevant replicated units treated as visible.
Inspected enemy is never added (fails team/selectable filter).

### 8) Modifier semantics (on Completed marquee)

Ctrl wins over Shift (same key reads as B2a).

| Modifier | Behavior |
| --- | --- |
| None | Build sorted eligible array → `SetSelectionFromUnits`. Empty → `ClearSelection`. Always `ClearInspectedTarget`. |
| Shift | Empty eligible → **no-op** (selection + inspect unchanged). Else clear inspect, then append sorted new units onto a copy of current selection → single `SetSelectionFromUnits`. |
| Ctrl | Empty eligible → **no-op**. Else clear inspect; start from current selection; for each sorted eligible unit toggle membership (Option 1 ≡ Option 2 on a unique set); single `SetSelectionFromUnits`. |

**Broadcast minimization:** prefer **one** selection mutation call after building the final array. Separate `ClearInspectedTarget` may produce a second legitimate broadcast (same B2a pattern) — document; do **not** change SelectionComponent solely to merge broadcasts.

### 9) Empty rectangle / empty eligible

| Case | Result |
| --- | --- |
| Replace + empty eligible | Clear selection + clear inspect |
| Shift + empty | No-op |
| Ctrl + empty | No-op |
| Marquee active then Cancel | No selection mutation |

### 10) SelectionComponent API sufficiency

| API | Role in B2b |
| --- | --- |
| `BeginMarquee` / `UpdateMarquee` / `EndMarquee` / `CancelMarquee` | Rectangle state only (already no notify) |
| `SetSelectionFromUnits` | Primary apply path (dedupe + cap + single notify if changed) |
| `ClearSelection` / `ClearInspectedTarget` | Empty replace + inspect cleanup |
| `AddUnitToSelection` / `ToggleUnitSelection` | Available but **avoid** in loops (multi-broadcast) |

**Decision:** SelectionComponent API is **sufficient** — **do not change** it in B2b unless a proven gap appears.
Marquee active flag is **not** required to fire `OnSelectionChanged`; widget is driven by PC.

### 11) State machine

```text
Idle
  → Started: PressPending (store press; no widget)
  → Tick while PressPending: if distance > 8px → MarqueeActive
       (BeginMarquee(press); show widget)
  → Tick while MarqueeActive: UpdateMarquee(current); update widget
  → Completed while PressPending (≤8px): existing B2a click path unchanged
  → Completed while MarqueeActive: resolve → apply modifiers → EndMarquee → hide widget
  → Canceled / focus-loss policy / EndPlay: CancelMarquee → hide widget; no selection mutation
  → Idle
```

| Edge case | Policy |
| --- | --- |
| Cursor leaves viewport | Clamp or keep last valid `GetMousePosition`; do not cancel solely for leaving |
| Viewport / app focus loss | Prefer CancelMarquee (no apply) if press/marquee active |
| PIE stop / EndPlay | CancelMarquee; destroy/hide widget; clear press flags |
| Unpossess / non-local | No widget; no scan; existing local guards |
| Widget create failure | Concise Error log; CancelMarquee; do not crash; click path remains |
| Release after marquee | **Must apply** marquee (replace deferred-only log) |

### 12) Multiplayer

- Local controller only for Tick marquee work, widget, and world scan
- No RPC; no replicated marquee; no replicated selection
- Host and remote client: independent widgets/state
- Server remote PlayerControllers: no widget, no scan
- Candidates may be replicated actors; selection remains local-only

### 13) Logging (one-shot on complete/cancel)

```text
GP Marquee: LocalTeam=1 Rect=(x1,y1)-(x2,y2) Candidates=5 Eligible=3 SelectedCount=3 Modifier=Replace Result=Applied
```

Cancel: `Result=Canceled`.
No per-Tick / per-candidate spam. `LogTemp` Log level for applied; Verbose for harmless no-ops if needed.

### 14) Performance boundary

| Rule | Value |
| --- | --- |
| Actor scan | **Once** on marquee Completed |
| Tick | Cursor + rectangle UI only |
| Cost | O(N units) on release — acceptable for foundational unit counts |
| Future | Spatial index deferred |
| Revisit threshold | When release hitch becomes measurable (order-of-magnitude: hundreds+ units / profiling) — not implemented now |

### 15) Implementation checklist (next reviewed task)

1. Add `Slate` / `SlateCore` private deps to `GPRuntime.Build.cs`.
2. Add `UGP_MarqueeSelectionWidget` (NativePaint; HitTestInvisible; no BP asset).
3. Extend `AGP_PlayerController`: Tick gate, marquee state, widget lifecycle, release resolve.
4. Wire `Begin/Update/End/CancelMarquee`; replace `DragDeferredToB2b` early-return with marquee complete path.
5. Preserve ≤8 px B2a click path exactly.
6. Eligibility + center-point inclusion + `GetPathName` sort + modifier apply via `SetSelectionFromUnits`.
7. One-shot `GP Marquee:` log.
8. Local-only / EndPlay / cancel policies.
9. Builds: GPEditor Dev, GP Dev, GP Shipping; no map save; no `IA_Marquee`.
10. Operator validation; then finalize.

---

## Exact next steps

1. Merge this B2b feature branch when ready.
2. Continue with the next phase defined by the existing GP-S16 plan (not GP-S17 / full GP-S18 unless that plan says so).
3. Keep temporary debug boxes until a later production highlight slice replaces them.

---

## Stop condition

**B2B_DONE.**
Phase B2 (click + marquee) complete and operator-validated.
Do **not** mark entire GP-S16 DONE solely because B2 is done.
Do **not** start GP-S17 / full GP-S18 from this finalize.
Do **not** create `IA_Marquee` / HUD / BP widget / production highlight here.
