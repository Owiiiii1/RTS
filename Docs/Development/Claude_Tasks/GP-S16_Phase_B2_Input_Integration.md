# GP-S16 Phase B2 — Input Integration
(Click select / inspect / marquee — blocked by team assignment)

## Status
**Status: B2A_DONE_B2B_PENDING**

TeamId prerequisite **merged**.
B2a click selection / inspect **implemented and operator-validated**.
B2a is **complete**.
B2b marquee **has not started** (owns candidate resolution + rectangle visualization).
Absence of drag-selection in B2a is **expected**.
GP-S16 overall remains **NOT DONE**. Do **not** start GP-S17 or full GP-S18.

Parent GP-S16 selection status:
**`PHASE_B2A_DONE_PHASE_B2B_PENDING`**

### B2a implemented (complete)

| Item | Detail |
| --- | --- |
| Assets | `IA_Select` (Boolean), `IMC_GP_Selection` (LMB → IA_Select) |
| Soft paths | `/Game/GrimProtocol/Input/Selection/IA_Select.IA_Select`, `.../IMC_GP_Selection.IMC_GP_Selection` |
| IMC priority | Selection **110**; Camera **100** unchanged |
| Lifecycle | Separate soft-load / bind / BeginPlayingState add / EndPlay remove; local only |
| Press/release | Started stores screen pos; Completed click if distance ≤ **8 px**; else `DragDeferredToB2b` |
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
| `IA_Marquee` | **Not** created (B2b) |

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

Next input packaging remains **SPLIT_CLICK_THEN_MARQUEE** — B2a **done**, B2b **pending**.

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
| Phase B2b marquee | **Not started** |
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

Scope:

- `IA_Marquee` only if final implementation still requires a separate action
- Screen rectangle
- Candidate resolution
- Friendly / selectable filtering
- Modifier integration
- Cap / order behavior
- Production-safe rectangle visualization
- Operator validation

---

## Input contract lock (assets not created)

```text
/Game/GrimProtocol/Input/Selection/IA_Select
/Game/GrimProtocol/Input/Selection/IA_Marquee
/Game/GrimProtocol/Input/Selection/IMC_GP_Selection
```

| Lock | Value |
| --- | --- |
| Selection IMC priority | **110** |
| Camera IMC priority | **100** |
| Contexts | Separate (`IMC_GP_Selection` ≠ `IMC_GP_Camera`) |
| Lifecycle | Mirror camera: soft refs → `LoadSynchronous` → bind `SetupInputComponent` → add local `BeginPlayingState` → remove `EndPlay` |
| Controller | Local only |
| Hit path | PlayerController cursor trace — **no** actor `OnClicked` delegates |
| Trace channel | `ECC_Visibility` |
| RPC | None |
| Selection state | Non-replicated |

No assets are created in this checkpoint.

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

## Exact next steps

1. Merge this team-assignment prerequisite.
2. Assign **B2a** click/inspect input (separate reviewed task).
3. Then **B2b** marquee.

Do **not** start B2a/B2b/GP-S17/full GP-S18 from this finalize pass.

---

## Stop condition

**B2A_CODE_READY_VALIDATION_PENDING.** Await operator PIE validation of click select/inspect/clear.
Do **not** start B2b / GP-S17 / full GP-S18.
Do **not** mark GP-S16 DONE.
