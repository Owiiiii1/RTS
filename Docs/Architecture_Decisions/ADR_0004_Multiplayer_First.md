# ADR-0004 — Multiplayer First

## Status
Accepted

## Context
GrimProtocol — RTS з PvP як основним gameplay target. Multiplayer не є "add-on after singleplayer" — це primary use case.

Дві стратегії розробки:

1. **Singleplayer first, multiplayer later** — швидше playable, але:
   - Replication retrofit — це майже завжди refactor, бо singleplayer assumptions (direct state mutation, BP gameplay) баранять multiplayer.
   - Authority retrofit особливо болюча: client-side gameplay calculations треба переписувати.

2. **Multiplayer first** — повільніший initial playable, але:
   - Server authority — design choice from day one.
   - Replication — first-class concern у кожному gameplay class.
   - GAS replication semantics — враховуються одразу.
   - Singleplayer = multiplayer з 1 player (listen server with no remote client).

## Decision
**Multiplayer-first з першого дня.**

Server-authoritative за замовчуванням. Client — input intent only.

### Concrete Rules

1. Кожна gameplay system має explicit authority model. Без explicit authority — система не вважається implementation-ready.
2. Замість приховування `HasAuthority()` — first-class у кожному gameplay class.
3. Replication primitives (`UPROPERTY(Replicated)`, RepNotify, `Server_*`/`Client_*`/`Multicast_*`, `DOREPLIFETIME_*`) — частина default coding (не "advanced topic").
4. GAS — multiplayer контекст з першого дня (replication modes явні, prediction policies явні).
5. Singleplayer працює як listen-server-with-no-clients. No special-case singleplayer code paths.
6. Multicast — тільки cosmetic. Gameplay стан синхронізується replicated property + GAS, не multicast.

### MVP Multiplayer Target

- 2 players, listen server, Steam OSS, PvP, 1 map.
- Dedicated server — поза MVP, але архітектура не блокує.

## Consequences

### Positive
- No "rewrite for multiplayer" phase.
- Server-authoritative discipline → fewer cheating vectors.
- Singleplayer і multiplayer share code 100% (no branching).
- GAS replication — well-traversed water.
- Future dedicated migration — purely deployment, не code change.

### Negative
- Initial playable повільніший: треба налагодити Steam, listen server, replicated state.
- Local-only debug — складніше debug network edge cases у solo dev workflow.
- Cosmetic-only multicast розрізняти від gameplay multicast — discipline overhead.
- Some convenient patterns (direct world state read у Blueprint) — заборонені.

### Risks
- Engineer регресія до "do it client-side just this once" — discipline drift. Mitigated: hard bans у `/CONTRIBUTING.md`, PR review checklist.
- Test setup overhead — 2-player local testing потребує 2 PCs або 2-window solution. Mitigated: PIE multi-player для smoke tests.

## Alternatives Considered
- **Singleplayer first** — швидше playable solo, але рефактор cost занадто високий.
- **P2P без listen server** — fragile authority model, не Steam-native pattern, не fits GAS expectations.
- **Dedicated server from day one** — overkill для MVP.

## References
- `/CONTRIBUTING.md` → Multiplayer Discipline.
- `Docs/TDD/03_Multiplayer_Architecture.md` — implementation.
- `Docs/TDD/08_Steam_Matchmaking.md` — Steam integration.
