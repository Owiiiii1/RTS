# Win / Lose Conditions

## MVP Primary Condition

**Delivery-quota victory.**

Перший гравець, чий `UGP_PlayerAttributeSet.FerroniteScore` досягає або перевищує `DeliveryQuotaFerroniteScore` (DA placeholder — **5000**, TBD у balance pass), **миттєво** виграє матч. Server тригерить `MatchState = Finished` у момент перетину порогу. Це primary path до перемоги — гонка за виконанням орбітального контракту.

> Worldbuilding rationale (per [`Lore_Setting`](Lore_Setting.md)) — кожна корпорація має контрактну квоту ферроніту, який треба доставити на орбіту. Хто першим виконує квоту — закриває контракт і виграє тендер.

### Score Source — FerroniteScore, NOT OrbitalFerronite

Метрика перемоги — **`FerroniteScore`**: cumulative, monotonic, `COND_None` (видимий усім), нараховується при кожному launch контейнера на орбіту і **ніколи не зменшується**.

`OrbitalFerronite` **ніколи не є метрикою перемоги.** Причина: `OrbitalFerronite` — spendable resource (player витрачає його на orbital drops). Якби перемога рахувалась по `OrbitalFerronite`, то витрачання ресурсу на drops карало б гравця зниженням score — це anti-fun і ламає основний loop. `FerroniteScore` відокремлює "скільки доставлено всього" (score) від "скільки можна витратити зараз" (spend), тому витрачання `OrbitalFerronite` на дропи ніяк не зачіпає прогрес до перемоги.

## Fallback Condition — Timer Expiry

Якщо жоден гравець не досяг квоти до закінчення 10-хвилинного match timer (per [`07_Match_Flow`](07_Match_Flow.md)), server порівнює `FerroniteScore` усіх гравців. Гравець з найбільшим cumulative `FerroniteScore` — winner.

```
if (any player FerroniteScore >= DeliveryQuotaFerroniteScore)
    -> immediate win for that player (WinReason = DeliveryQuota)
else if (MatchTimeRemaining <= 0)
    -> winner = argmax over players of FerroniteScore (WinReason = TimerScore)
```

## Secondary Loss — Annihilation

Якщо у гравця знищено Main Base, цей гравець **миттєво програє**, за умови `bAnnihilationCountsAsWin == true` (default **true**, tunable у DA / `AGP_GameState`).

Rationale: без Main Base гравець не має containers, не може ship raw Ferronite на орбіту → не може нарощувати `FerroniteScore` → втрачає всякий шлях до перемоги. Тому втрата Main Base оформлена як immediate loss (а не повільне economic вмирання), щоб уникнути порожнього "dead but still running" стейту. Опонент перемагає з `WinReason = Annihilation`.

Якщо `bAnnihilationCountsAsWin == false` (post-MVP toggle) — втрата Main Base переводить гравця у `Spectating`, а матч іде до timer expiry / quota.

## Tie-Break Ladder

При рівних показниках (наприклад, рівний `FerroniteScore` на 10:00) winner визначається послідовно:

1. **`FerroniteScore`** — найбільший cumulative shipped score.
2. **`OrbitalFerronite`** — найбільший залишок spendable ресурсу.
3. **`CurrentUnits`** — більша жива армія / робоча сила.
4. **Deterministic seed** — детермінований match seed (server-authoritative, без RNG на момент порівняння), щоб гарантувати єдиний winner без `Draw`.

Tie-break параметри живуть у Data Asset / `AGP_GameState`. Tunable, не hardcoded.

## Win Reasons (Gameplay Tags)

`MatchResult.WinReason : FGameplayTag`, одне з:

- `GP.Match.WinReason.DeliveryQuota` — перший досяг `DeliveryQuotaFerroniteScore`.
- `GP.Match.WinReason.TimerScore` — найвищий `FerroniteScore` на timer expiry.
- `GP.Match.WinReason.Annihilation` — опонент втратив Main Base (за `bAnnihilationCountsAsWin`).
- `GP.Match.WinReason.OpponentDisconnect` — опонент від'єднався.

## Implementation Notes

- Server відстежує перетин квоти при кожному `FerroniteScore` write (launch контейнера) — event-driven, не tick-polling.
- Server слухає `MatchTimeRemaining` countdown у `AGP_GameMode::Tick` (low-frequency, e.g. 1 Hz) для fallback path.
- Main Base destruction event → server негайно перевіряє `bAnnihilationCountsAsWin` → transition у `Finished`.
- На finish — server compute result, replicate `MatchResult` struct.
- `FerroniteScore` — replicated (`COND_None`), monotonic, server-only writes. Не зменшується при building destruction чи spend.

Це чисто server-authoritative flow. Жодного client-side win condition.

## Replicated State

`AGP_GameState`:

- `MatchTimeRemaining : float` — replicated, RepNotify (HUD updates).
- `DeliveryQuotaFerroniteScore : float` — quota threshold (з match config / DA, placeholder 5000).
- `bAnnihilationCountsAsWin : bool` — toggle для annihilation loss (default true).
- `MatchResult : FGP_MatchResult` (struct):
  - `WinnerTeamId : int32` (`-1` для draw — у MVP недосяжний завдяки deterministic seed tie-break)
  - `WinnerReason : FGameplayTag` (`GP.Match.WinReason.*`)
  - `MatchDuration : float`
  - `FinalScores : TMap<int32, float>` (team_id → final `FerroniteScore`)

Replicated, RepNotify тригерить client-side end-of-match UI.

## Secondary Conditions (MVP)

- **Player disconnect (multiplayer):** server marks disconnected player як lost, остатній player виграє з `GP.Match.WinReason.OpponentDisconnect`.
- **Main Base destroyed:** immediate loss за `bAnnihilationCountsAsWin` (див. вище).

## Edge Cases

- **Host disconnect:** match aborts, no winner (per [`07_Match_Flow`](07_Match_Flow.md) multiplayer specifics).
- **Both players reach quota same tick:** tie-break ladder вирішує (FerroniteScore exact → OrbitalFerronite → CurrentUnits → seed).
- **Both players score 0 на 10:00:** tie-break ladder спускається до deterministic seed → один winner (немає `Draw` у MVP).
- **SWARM знищує гравця у останню секунду:** якщо це Main Base destruction — immediate annihilation loss; інакше score замерзає, winner по cumulative.

## Surrender / Forfeit

**Disabled у MVP.** Немає кнопки "Surrender". Гравець, що хоче вийти, disconnects → marked as lost. Surrender UI / vote-to-end — у `Backlog`.

## Tunable Knobs (TBD Placeholders)

Усі balance-значення живуть у DA / match config, не hardcoded:

- `DeliveryQuotaFerroniteScore` — placeholder **5000**.
- `MatchDuration` (timer hard cap) — **600 s** (per [`07_Match_Flow`](07_Match_Flow.md)).
- `bAnnihilationCountsAsWin` — default **true**.
- Tie-break ordering — фіксований ladder вище; deterministic seed source — TBD.

## Out of MVP Win Conditions

- Territory control.
- Diplomatic (peace treaty / surrender).
- VIP unit alive condition.

Усі — у `Backlog/`.

## References

- Match timer і phases — [`07_Match_Flow`](07_Match_Flow.md).
- Ferronite score generation, Container System — [`06_Resources`](06_Resources.md).
- Core loop — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md).
- Orbital Delivery — [`10_Orbital_Delivery`](10_Orbital_Delivery.md).
- Multiplayer disconnect handling — [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md), [`../TDD/08_Steam_Matchmaking`](../TDD/08_Steam_Matchmaking.md).
- Game pillars (Match-Based RTS, Indie-Honest Scope) — [`01_Game_Pillars`](01_Game_Pillars.md).
- Worldbuilding — [`Lore_Setting`](Lore_Setting.md).
