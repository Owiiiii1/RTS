# Match Flow

> **SWARM concept (2026-09-02):** approved in [`14_SWARM`](14_SWARM.md). Continuous per-team pressure.
> Discrete wave placeholders below are **superseded**. Runtime director **not started**. Invariant:
> `FerroniteThreatValue` = raw stock at MainBase (drop-off up, launch down);
> `FerroniteScore` and `OrbitalFerronite` do not drive SWARM.

## Match Length

**MVP target: 10 хвилин per match (hard cap).** Швидкий, інтенсивний loop. Без soft cap, без tie-break extension у MVP.

Worldbuilding rationale — корпоративні підрозділи мають вузьке вікно operations перед тим, як орбітальний контракт закривається. Mechanically — швидкий match зменшує snowball, тримає гравця у активній decision-making зоні.

## States Overview

`EGP_MatchState`:

1. `Loading` — host/client завантажують map, готують GameMode.
2. `WaitingForPlayers` — host у lobby, чекає client.
3. `Playing` — match триває (timer веде countdown від 10:00 до 0:00).
4. `Paused` — only для singleplayer (multiplayer pause заборонено у MVP).
5. `Spectating` — гравець, що вибув, але match продовжується. У MVP досягається лише якщо `bAnnihilationCountsAsWin == false` (post-MVP toggle); за default annihilation = immediate loss, тож цей state у MVP практично не використовується.
6. `Finished` — win/lose visualized після timer expiry, quota hit, або annihilation; match ends.

Tag mapping — `GP.Match.State.{Loading, WaitingForPlayers, Playing, Paused, Spectating, Finished}`.

## State Transitions

```
Loading
   |
   v
WaitingForPlayers
   |
   v
Playing  <-->  Paused  (singleplayer only)
   |
   |--> Spectating  (only if bAnnihilationCountsAsWin == false; otherwise MainBase loss -> Finished)
   |
   v
Finished  (triggered by Delivery Quota hit, MainBase annihilation, OR 10-min timer expiry)
```

`AGP_GameMode` — server-authoritative owner state machine. State замінюється тільки на сервері; replicated через `AGP_GameState`.

## Phase Breakdown

### Loading

- Map loaded.
- `AGP_GameMode` Init: factions assigned, spawn points reserved.
- Players preload assets.

### WaitingForPlayers

- Host у lobby (Steam matchmaking).
- Client підключається через Steam invite або join-by-friend.
- Коли `PlayerCount == ExpectedCount` (2 у MVP) — auto-transition у `Playing`.
- Singleplayer flow — AI opponent ініціалізується у тому ж стейті перед transition.

### Playing

- Match timer starts (`AGP_GameState.MatchTimeRemaining = 600.0`). Countdown.
- Players spawn з initial state (per `DA_GP_Faction_Default` — 1 Main Base + 2 Workers pre-deployed, `OrbitalFerronite = 0`).
- Core loop activated: mine → carry до MainBase containers → launch до орбіти (+OrbitalFerronite +FerroniteScore) → order orbital drops → expand / defend проти SWARM.
- SWARM — безперервний per-team потік від `FerroniteThreatValue` (raw Ferronite у MainBase containers). Drop-off ↑, launch ↓. `FerroniteScore` / `OrbitalFerronite` НЕ масштабують SWARM. Час матчу не масштабує SWARM напряму. Pillar 6 greed-vs-safety. **Заборонено:** predetermined numbered-wave schedule. Канон: [`14_SWARM`](14_SWARM.md).
- Quota hit (`FerroniteScore >= DeliveryQuotaFerroniteScore`) → immediate `Finished`. MainBase destruction → immediate `Finished` (annihilation). Timer expiry → automatic transition у `Finished`.

### Spectating

- За default (`bAnnihilationCountsAsWin == true`) MainBase loss = immediate `Finished` (loss), тож `Spectating` у MVP не активується.
- Якщо `bAnnihilationCountsAsWin == false` (post-MVP toggle): player без Main Base → `Spectating`, може рухати камеру (`AGP_CameraPawn`), не може issue commands, match триває до timer expiry, накопичений `FerroniteScore` залишається при final tally.

### Finished

- Тригери: Delivery Quota hit (immediate), MainBase annihilation (immediate), або timer expiry на 0:00.
- Server compute result per [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md) (quota / timer score / annihilation; tie-break ladder: FerroniteScore → OrbitalFerronite → CurrentUnits → deterministic seed).
- Replicated win/lose result у `AGP_GameState.MatchResult` (struct з winner team ID, final `FerroniteScore` map, `WinReason` tag).
- Client UI показує end-of-match screen з score breakdown.
- Через X seconds (TBD, e.g. 15 s) — return to lobby або menu.

## Single Phase Structure (MVP)

MVP не має явних early / mid / late phases. Один безперервний "**Mine, Ship, Defend**" phase з organic escalation, що керується `FerroniteThreatValue` (stored-at-base stock), не годинником:

```
0:00  [match start]   Initial state: MainBase + 2 Workers, OrbitalFerronite = 0. Низький threat.
mid   [escalation]    Hoard raw Ferronite → high FerroniteThreatValue → щільніший continuous SWARM;
                      часто ship → lower threat. Circular walls / turrets / industrial combat coverage.
late  [quota push]    Lagging player наздоганяє: швидше mine → threat росте → ризик. Або push до Delivery Quota.
end   Quota hit -> immediate win; MainBase loss -> annihilation; інакше 10:00 timer → highest FerroniteScore.
```

Це не явні numbered-wave phases і не time-based wave curve. Escalation = функція поточного per-team `FerroniteThreatValue`.

## SWARM Pressure Escalation

Канон: [`14_SWARM`](14_SWARM.md) + [`../TDD/17_SWARM_Architecture`](../TDD/17_SWARM_Architecture.md).

**Superseded:** `WaveBaseInterval`, `WaveSize`, `WaveSpawnPoints`, `WaveStartDelay` як обов'язкова модель,
і illustrative wave-size tables. Не вигадувати нові числа.

Threat bands (conceptual) задають active budget, кількість spline-напрямків, replenishment, roster,
Large cap. Інтенсивність змінюється поступово. Вже створені істоти не despawn-яться при зниженні threat.
Director / spawn stream — **future implementation**.

## Timing (MVP)

- Match duration: **10:00 hard cap** (600 seconds).
- End-of-match screen display: 15 s before forced return to lobby/menu.

## Multiplayer Specifics

- Host = listen server. Якщо host disconnects → match aborts (clients return to menu з error).
- Mid-match join — disabled у MVP.
- Pause — disabled у multiplayer.
- Surrender / forfeit — disabled у MVP (немає кнопки). Гравець, що залишає матч → disconnect → mark as lost player. Other player продовжує матч до timer expiry і виграє за score.

## Singleplayer Specifics

- AI opponent ([`03_Factions`](03_Factions.md)) — `AGP_AIController : AAIController` (no client RPC; server helpers викликаються напряму) — запускається разом з player у `Playing` state.
- Pause доступний (paused гра не зупиняє timer? — TBD; recommendation: pause stops timer, бо це singleplayer).
- AI також накопичує `FerroniteScore` (mineить, ship у власну орбіту через containers). Player виграє по quota / по highest `FerroniteScore` на 10:00 / по annihilation AI MainBase.

## Edge Cases

- **Player disconnect mid-match (multiplayer):** server marks player як lost, опонент продовжує сам до timer expiry, виграє по умовчанню.
- **Both players disconnect:** match aborts.
- **Score tie на 10:00:** tie-break ladder per [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md) — `FerroniteScore` → `OrbitalFerronite` → `CurrentUnits` → deterministic seed (єдиний winner, без draw).
- **SWARM overwhelm:** якщо падає MainBase — immediate annihilation; інакше матч іде до quota/timer. Final `FerroniteScore` + tie-break ladder визначає winner.

## References

- Win/lose conditions, score model, tie-break — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- SWARM faction і continuous pressure — [`14_SWARM`](14_SWARM.md), [`03_Factions`](03_Factions.md).
- Resource score generation — [`06_Resources`](06_Resources.md).
- State machine technical — [`../TDD/00_Technical_Overview`](../TDD/00_Technical_Overview.md), [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md).
- Steam flow — [`../TDD/08_Steam_Matchmaking`](../TDD/08_Steam_Matchmaking.md).
- Worldbuilding — [`Lore_Setting`](Lore_Setting.md).
