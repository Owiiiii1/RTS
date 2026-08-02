# GP-S16 Phase B2 — Input Integration
(Click select / inspect / marquee — blocked by team assignment)

## Status
**Status: TEAM_ASSIGNMENT_DONE_B2A_PENDING**

GameMode playable TeamId allocator **implemented and operator-validated**.
TeamId blocker **resolved**.
B2a click selection is now **technically unblocked**.
B2a code **has not started**. B2b marquee **has not started**.
GP-S16 overall remains **NOT DONE**. Do **not** start GP-S17 or full GP-S18.

Parent GP-S16 selection status:
**`TEAM_ASSIGNMENT_DONE_PHASE_B2A_PENDING`**

### Team assignment (implemented)

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

### Operator validation (passed)

| Check | Result |
| --- | --- |
| Standalone player TeamId `1` | **PASS** |
| Listen-server host TeamId `1` | **PASS** |
| Remote client TeamId `2` | **PASS** |
| TeamIds unique | **PASS** |
| Repeated PIE resets allocator to `1`/`2` | **PASS** |
| PlayerState / TeamId replication warnings | **NONE** |
| Camera regression | **NONE** |
| Match-flow regression | **NONE** |
| Map saved / assets created | **NO** |

Next input packaging remains **SPLIT_CLICK_THEN_MARQUEE** (B2a → B2b).

---

## Relationship

| Stage | State |
| --- | --- |
| Phase A selection state shell | **Merged** |
| Phase B1 mutation API | **Merged** |
| Selectable UnitBase prerequisite | **Merged** |
| Phase B2 analysis | **Complete** (this doc) |
| Player TeamId assignment | **DONE** (operator-validated) |
| Phase B2 code / assets | **Not started** |
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

**TEAM_ASSIGNMENT_DONE_B2A_PENDING.** Team-assignment prerequisite ready for merge.
Do **not** start B2a / B2b / GP-S17 / full GP-S18 from this finalize pass.
Do **not** mark GP-S16 DONE.
