# GP-0501 Steam Matchmaking MVP

## Goal

Описати Steam 2-player host/client MVP flow.

## Inputs

- [`../../TDD/08_Steam_Matchmaking.md`](../../TDD/08_Steam_Matchmaking.md)
- [`../../TDD/03_Multiplayer_Architecture.md`](../../TDD/03_Multiplayer_Architecture.md)
- [`../../GDD/07_Match_Flow.md`](../../GDD/07_Match_Flow.md)

## Code Allowed

No.

## Scope

Session flow, lobby/start states, failure paths, authority notes. No OnlineSubsystem code.

## Required Skill Pass

- `gp-mechanics-validator`

## Deliverables

- Host flow.
- Client join flow.
- Ready/start condition.
- Failure handling.
- Same-map rule.

## Validation

- 2 players only.
- Host/client listen server.
- Server authoritative gameplay remains unchanged.
- Host leave/client fail paths are documented.

## Stop Condition

Зупинитися після Steam MVP flow spec.

## Output

- Design spec: section **"Detailed Steam Matchmaking Rules (GP-0501)"** у [`../../TDD/08_Steam_Matchmaking.md`](../../TDD/08_Steam_Matchmaking.md).
- Session state machine: MainMenu → Connecting → Hosting/Client Lobby → LoadingMatch → Playing → Finished.
- Host flow + Client flow (3 entry points: search, friend overlay, command-line `+connect_lobby`).
- Ready/Start conditions: 2 players + both ready + host clicks Start; client cannot start.
- Same-map invariant enforced (hardcoded `MAP_GP_MatchDefault`, server-side assert).
- Failure matrix: 15 fault vectors (create fail, join fail, mid-lobby disconnect, mid-match disconnect, travel failure, Steam offline, AppID mismatch, etc.) з UX responses.
- Decision: client mid-match leave → auto-win host (`EndMatch Reason=OpponentLeft`).
- `AGP_LobbyState` replication contract + `UGP_LobbyVM` per Common UI + MVVM rule.
- 15 playtest scenarios, anti-pattern checklist.
- Code implementation deferred to follow-up task **GP-0501A Steam Matchmaking Implementation** (Code Allowed: Yes).
