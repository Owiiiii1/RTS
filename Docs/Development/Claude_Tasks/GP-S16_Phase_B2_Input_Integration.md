# GP-S16 Phase B2 — Input Integration
(Click select / inspect / marquee — blocked by team assignment)

## Status
**Status: BLOCKED_BY_TEAM_ASSIGNMENT**

Documentation checkpoint **approved**. Implementation **not started**.
No C++ / assets / maps / config changed in this pass.
GP-S16 overall remains **NOT DONE**. Do **not** start GP-S17 or full GP-S18.

Parent GP-S16 selection status:
**`PHASE_B2_BLOCKED_TEAM_ASSIGNMENT`**

### Verdict

**`BLOCKED_BY_TEAM_ASSIGNMENT`**

| Finding | Lock |
| --- | --- |
| `AGP_PlayerState::SetTeamId` | Exists (authority-only) |
| Actual call sites | **None** |
| `AGP_GameMode::PostLogin` | Does **not** assign teams |
| Listen-server host TeamId | Remains `-1` |
| Remote client TeamId | Remains `-1` |
| `-1 == -1` as friendly | **Forbidden** |
| Hidden fallback `-1 → 1` | **Forbidden** |
| Phase B2 with unassigned local team | Must **fail closed** — no selection |

**Required next prerequisite:** server-authoritative playable TeamId assignment.

After that unlock, input packaging is locked as **SPLIT_CLICK_THEN_MARQUEE** (B2a → B2b).

---

## Relationship

| Stage | State |
| --- | --- |
| Phase A selection state shell | **Merged** |
| Phase B1 mutation API | **Merged** |
| Selectable UnitBase prerequisite | **Merged** |
| Phase B2 analysis | **Complete** (this doc) |
| Player TeamId assignment | **Missing** ← blocks B2 |
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

## Exact next coding assignment

**GameMode TeamId assignment slice only** (see approved prerequisite above).

Do **not** in that slice:

- Create selection IA/IMC assets
- Implement cursor trace / click / marquee
- Start GP-S17 or full GP-S18

---

## Stop condition

**BLOCKED_BY_TEAM_ASSIGNMENT.** Documentation checkpoint complete.
Await tech-lead assignment for server-authoritative playable TeamId allocation.
Do **not** implement Phase B2 input until that prerequisite merges.
Do **not** use `-1 → 1` fallback.
Do **not** mark GP-S16 DONE.
