# GP-S11 — AGP_LobbyState
(Replicated Lobby Player List and Ready Summary)

## Slice Group
Slice 2 — Match Flow + Asset Loader

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S10 DONE (prior Slice 2 stage; not a hard code dependency).
- ADR-0006 (no overengineering; LobbyState is an actor, not a new subsystem).
- ADR-0008 (AI opponent exists in architecture; GP-S11 has no AI-specific lobby fields).
- Module `GPRuntime`.
- Canonical: TDD/13 (`AGP_LobbyState : AInfo`, list + `bAllReady`); TDD/08 (session/travel separated from lobby snapshot).

## Goal
Implement `AGP_LobbyState : AInfo` that stores an authoritative server lobby participant snapshot (`PlayerId`, `DisplayName`, `bIsReady`), replicates the list and aggregated `bAllReady`, exposes read-only getters + native change delegates, and provides authority-only mutation methods — without sessions, ServerTravel, ready RPC, Lobby GameMode, UI, TeamId, AI/host fields, or PlayerState replacement.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S12 until explicitly assigned (do not auto-materialize GP-S12 task file).

### Closed with
- `AGP_LobbyState : AInfo`
- replicated and always relevant
- movement replication disabled
- Tick disabled
- exact three-field `FGP_LobbyPlayer`
- `PlayerId` identity
- `DisplayName` snapshot
- `bIsReady`
- authority-only mutation API
- `INDEX_NONE` validation
- duplicate ID updates existing entry
- ready preserved on name update
- deterministic sorting by PlayerId
- `bAllReady = Num() > 0 && all ready`
- empty list → false
- replicated `TArray`
- replicated `bAllReady`
- `COND_None`
- `REPNOTIFY_Always`
- native delegates
- server broadcasts after real mutation
- client broadcasts from RepNotify
- `ForceNetUpdate` only after real mutation
- no TeamId
- no AI/host/Steam fields
- no PlayerState pointer
- no RPC
- no Blueprint API
- no LobbyGameMode/session/travel/discovery wiring
- no config/maps/assets/tests
- operator Editor/PIE validation **PASSED**
- real replication/listen-server proof **deferred**
- GP-S12 not started

### Files
- `GP/Source/GPRuntime/Public/Lobby/GPLobbyState.h`
- `GP/Source/GPRuntime/Private/Lobby/GPLobbyState.cpp`

---

## Tech-lead locks (OD-1…OD-26) — RESOLVED + IMPLEMENTED

All OD-1…OD-26 locks from SPEC_READY are implemented as closed above.

---

## Acceptance Criteria
- [x] Compiles (GPEditor Dev, GP Dev, GP Shipping) — PASSED
- [x] Exact three-field struct + PlayerId identity + authority mutations
- [x] Sort-by-PlayerId on add; ready preserved on name update
- [x] `bAllReady` formula; empty → false; no min/expected count
- [x] `COND_None` + `REPNOTIFY_Always`; C++-only; no RPC/Blueprint
- [x] No GameMode/GameState/PC/PS/config/session/UI/travel/TeamId/AI/host
- [x] Editor / module load (operator) — PASSED
- [x] `GP_LobbyState` found in Class Viewer (operator) — PASSED
- [x] PIE (operator) — PASSED
- [x] GP-S11 related errors — ABSENT
- [x] Tech lead accepted
- [x] Operator accepted
- [x] Real replication/listen-server proof — **deferred** (accepted for close; Lobby GameMode spawn wiring absent)

## Manual Editor validation (operator) — PASSED
1. Module load — OK  
2. Class Viewer → `GP_LobbyState` — found  
3. Normal PIE — OK  
4. No map/config/Blueprint changes  
5. Replication proof deferred  

## Build results
| Target | Result |
| --- | --- |
| GPEditor Win64 Development | PASSED |
| GP Win64 Development | PASSED |
| GP Win64 Shipping | PASSED |

## Out of Scope (confirmed)
- Lobby GameMode / Lobby PC / lobby map / lobby UI  
- SessionSubsystem / Steam / ready RPC / ServerTravel  
- TeamId / AI / host fields  
- Fast Array / expected player count  
- Changes to GameMode / GameState / PC / PS / config / uproject  
- Automation tests / assets  
- GP-S12+

## Linked canonical docs
TDD/13, TDD/08, ADR-0006, ADR-0008, GP-S09, GP-S10.

## Stop Condition
GP-S11 closed as DONE. Do **not** start GP-S12. Do **not** auto-materialize the next task file.
