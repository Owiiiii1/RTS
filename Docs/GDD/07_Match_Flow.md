# Match Flow

> **SWARM roadmap gate (2026-08-20):** SWARM is the final gameplay implementation stage of MVP and
> requires a dedicated design/reconciliation review first. Timings, wave/director behavior, spawn
> rules, roster, and targeting below are placeholders until that review. The established invariant is
> only that `FerroniteThreatValue` reflects raw stock at MainBase (drop-off up, launch down);
> `FerroniteScore` and `OrbitalFerronite` do not drive SWARM pressure.

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
- SWARM waves починають спавнитися після `WaveStartDelay` (initial grace period, TBD у balance pass, e.g. 60 s). Подальша інтенсивність — розмір wave і частота — є функцією `AGP_GameState.FerroniteThreatValue` = **raw Ferronite, що зараз зберігається у MainBase containers** (per [`../TDD/07_Resource_Architecture`](../TDD/07_Resource_Architecture.md)). Drop-off raw Ferronite ↑ `FerroniteThreatValue` (небезпечніше), launch до орбіти ↓ `FerroniteThreatValue` (безпечніше). `FerroniteScore` / `OrbitalFerronite` НЕ масштабують SWARM. Час матчу не масштабує SWARM напряму. Це central tension per Pillar 6: hoard raw Ferronite = dangerous, ship = safe. Greed-vs-safety loop. Заборонено: predetermined time-based escalation.
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
0:00  [match start]   Initial state: MainBase + 2 Workers, OrbitalFerronite = 0. Низький threat, no SWARM.
~1:00 [first wave]    Threat перетнув перший поріг (накопичено raw у containers) -> мала тестова wave.
mid   [escalation]    Гравець, що hoardить raw Ferronite, тримає high FerroniteThreatValue -> більші waves;
                      гравець, що часто ship -> low threat, спокійніше. Defensive drops (Turret/Wall/Salvage Walker).
late  [quota push]    Lagging player наздоганяє: швидше mine -> threat росте -> ризик. Або push до Delivery Quota.
end   Quota hit -> immediate win; інакше 10:00 timer expiry -> highest FerroniteScore wins.
```

Це не явні phases і не time-based curve у data — escalation = функція поточного `FerroniteThreatValue`. Документовано для design alignment.

## SWARM Wave Escalation

SWARM waves тригерить `AGP_GameMode` server-side per схемою з [`03_Factions`](03_Factions.md). Параметри escalation:

- `WaveBaseInterval`: 60 s між waves у early match (TBD).
- `WaveSize` зростає від `AGP_GameState.FerroniteThreatValue` (raw Ferronite stored-at-base; ↑ on drop-off, ↓ on launch).
- `WaveSpawnPoints` поза стартовими зонами, ближче до active mining зон.

### Threat Curve (Recommendation, MVP)

Escalation керується **поточним `FerroniteThreatValue`**, не годинником. Таблиця нижче — рекомендований mapping порогів threat → інтенсивність (illustrative bands):

| FerroniteThreatValue band | Typical Wave Size | Pressure Level |
| --- | --- | --- |
| ~0 (just shipped / start) | 0 | Quiet (build-up grace). |
| Low stored stock | 3-5 grunts | Low. |
| Moderate stored stock | 6-10 | Moderate. |
| High stored stock (hoarding) | 10-15 | High. |
| Very high (heavy hoard) | 15-25 | Peak. |

Numbers і band thresholds — TBD у balance pass. Tunable через DA. Гравець керує власним pressure level: ship швидко → опускається у нижчі bands; hoard → піднімається у вищі.

## Timing (MVP)

- Match duration: **10:00 hard cap** (600 seconds).
- SWARM first wave: ~60 s після match start.
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
- **SWARM waves overwhelm обох гравців:** match закінчується по timer (або по annihilation, якщо MainBase падає першим). Final `FerroniteScore` + tie-break ladder визначає winner.

## References

- Win/lose conditions, score model, tie-break — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- SWARM faction і wave design — [`03_Factions`](03_Factions.md).
- Resource score generation — [`06_Resources`](06_Resources.md).
- State machine technical — [`../TDD/00_Technical_Overview`](../TDD/00_Technical_Overview.md), [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md).
- Steam flow — [`../TDD/08_Steam_Matchmaking`](../TDD/08_Steam_Matchmaking.md).
- Worldbuilding — [`Lore_Setting`](Lore_Setting.md).
