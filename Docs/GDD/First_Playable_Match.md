# First Playable Match

> **SWARM concept approved (2026-09-02):** [`14_SWARM`](14_SWARM.md) / [`../TDD/17_SWARM_Architecture`](../TDD/17_SWARM_Architecture.md).
> Discrete numbered waves, Grunt counts, first-wave timing, fixed spawn points, and nearest-asset
> targeting in older story beats are **superseded**. Runtime implementation **not started**. SWARM
> remains the final gameplay implementation stage and is separate from the RTS AI Opponent.

## Scope

Один end-to-end story, що описує **повний цикл** GrimProtocol матчу — від запуску гри до повернення у головне меню. Без gaps у правилах, без implicit decisions. Це **canonical reference** для всіх gameplay tasks Phase 1–5; будь-яке нове правило має відповідати цій story або призводити до її оновлення.

Story — pillars-driven (per [`01_Game_Pillars`](01_Game_Pillars.md)) і узгоджена з:

- [`00_Project_Overview`](00_Project_Overview.md) — MVP scope.
- [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md) — loop primitives.
- [`07_Match_Flow`](07_Match_Flow.md) — phase і state transitions.
- [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md) — score model.
- [`09_UI_UX`](09_UI_UX.md) — HUD readouts.

## Player Goal

Гравець хоче побачити **повний матч** GrimProtocol від запуску до результату. Він має розуміти:

- з чого почати,
- куди натискати,
- що відбувається на полі,
- як вимірюється успіх,
- чим завершується матч.

Кожен крок — readable за ≤ 1 секунду після завантаження екрану.

## Two Match Modes (MVP)

### Singleplayer Path

Один гравець проти primitive AI opponent на тій самій map. Local session, без Steam, без network. Призначення — швидкий solo playable test без партнера.

### Multiplayer Path (PvP)

Steam matchmaking, 2 players (host + 1 client), listen server. Те ж саме gameplay loop, та сама map, той самий timer, той самий win condition. Призначення — primary MVP target.

Обидва шляхи мають **спільний gameplay core**. AI opponent і human opponent — обидва генерують FerroniteScore через shipping containers до орбіти, обидва підпадають під SWARM scaling (driven by FerroniteThreatValue — raw stock at base), обидва закінчуються по delivery quota АБО 10-min timer.

## End-to-End Story (Singleplayer)

### Step 1 — Main Menu

Player запускає гру → екран `WBP_GP_MainMenu`:

- "Singleplayer (vs AI)" — primary action.
- "Multiplayer (Steam)" — host or join.
- "Quit" — exit application.

Player натискає "Singleplayer (vs AI)".

### Step 2 — Loading

`AGP_GameMode` ініціалізується у локальному session-у:

- Map завантажується (single MVP map).
- AI opponent ініціалізується як `AGP_AIController` (server-side).
- Spawn points для player і AI зарезервовані.
- `MatchState = Loading`.

UI показує loading screen з coverage progress (TBD у [`09_UI_UX`](09_UI_UX.md)).

### Step 3 — Match Start

`MatchState = Playing`. Server виконує:

- Spawn player Main Base + 2 Workers у player's start zone (`DA_GP_Faction_Default.StartingUnits`).
- Spawn AI Main Base + 2 Workers у AI's start zone.
- Spawn 1 rich Ferronite Deposit (~2000 capacity) біля кожного start zone.
- Spawn 2-3 additional deposits (~1500 capacity) поза стартовими зонами.
- Set `FerroniteScore = 0` і `OrbitalFerronite = 0` для player і AI.
- Set `FerroniteThreatValue = 0` (raw stock at base) для player і AI.
- Start `MatchTimeRemaining = 600.0` countdown.

Player бачить:

- Camera positioned над власною Main Base.
- HUD з усіма readouts ([`09_UI_UX`](09_UI_UX.md)): Match Timer, OrbitalFerronite, FerroniteScore, Opponent Score, SWARM Threat, Minimap.
- 2 Workers idle біля бази.

### Step 4 — Early Mining (0:00 – 1:00)

Player issues `GP.Command.Mine` на Workers → найближчий Ferronite Deposit. Workers рухаються, починають mining cycle:

- Mine tick → `CarriedFerronite` зростає.
- При `CarriedFerronite >= 50` → Worker auto-returns до Main Base.
- На drop-off → raw (Planetary) Ferronite додається у MainBase containers; `FerroniteThreatValue` зростає (raw stock at base).
- Worker auto-returns до deposit.

Коли container заповнений → ships to orbit: raw Ferronite конвертується у `OrbitalFerronite` (spendable, `COND_OwnerOnly`) + `FerroniteScore` (cumulative shipped, `COND_None`) інкрементується; `FerroniteThreatValue` падає на launch.

Player бачить:

- FerroniteScore збільшується (HUD flash на ship event).
- OrbitalFerronite (spendable currency) наповнюється після shipping.
- SWARM Threat (FerroniteThreatValue) росте з raw stock, падає на launch.

Player вирішує — spend OrbitalFerronite через Order Menu (Logistics Hub / `UGP_OrbitalDeliverySubsystem`) на orbital drop (`AGP_DropPod`):

- Worker (cost TBD).
- Logistics Hub (cost TBD — passive cap + storage building, не виробляє).
- Defensive Turret (cost TBD).

Жодного local production / construction: усе прибуває з орбіти через drop pods.

AI робить аналогічні дії у власному tempo (state machine з [`03_Factions`](03_Factions.md): mine → ship → order → defend).

### Step 5 — SWARM Pressure Appears

Як тільки raw Ferronite накопичується в MainBase containers, per-team `FerroniteThreatValue` росте і
майбутній director починає **безперервний** потік (runtime **not started**). Немає numbered first wave,
фіксованого `WaveSpawnPoint` чи Grunt count як approved requirement.

- Спавн — з зовнішнього замкненого spline за межами бази / start zone.
- Стратегічна ціль потоку — **MainBase цієї команди**. SWARM атакує те, що стоїть на шляху.
- Кругова оборона (Walls / Turrets / combat units), не кілька відомих входів.

Player реагує:

- Якщо є Salvage Walker — командує `GP.Command.Attack`.
- Якщо є Defensive Turret — turret auto-engages SWARM у range.
- Якщо нічого немає — Workers і база під загрозою; знищення MainBase = annihilation.

UI показує:

- Threat indicator (driven by `FerroniteThreatValue`) — поточний raw stock pressure.
- Optional later minimap approach pulse (minimap slice).

### Step 6 — Mid Match (3:00 – 7:00)

Player escalates через orbital orders:

- Більше Workers (orbital drop) → більше mining → більше shipping → більше Score.
- Logistics Hub (orbital drop) додає +5 MaxUnits + container capacity → ширший shipping pipeline.
- 1-2 Salvage Walker (orbital drop) patrol біля бази, escort Workers до remote deposits.
- 2-3 Defensive Turret (orbital drop) біля choke points.

AI продовжує state machine (orbital model):

- `Mine` → mining + fill containers.
- `Ship` → пріоритезує container launch до орбіти.
- `Order` → spends OrbitalFerronite на orbital drops (Workers / Salvage Walker / Turret / Logistics Hub).
- `Defend` → react to SWARM threat / enemy near base.

SWARM pressure escalates per поточний `FerroniteThreatValue`:

- Інтенсивність — функція raw stock at base: зростає на drop-off, падає на launch. Чим більше raw stock тримає гравець / AI — тим більша загроза.
- Час матчу сам по собі не масштабує SWARM. Якщо гравець швидко shipить (низький raw stock) — pressure лишається нижчим, навіть пізно в матчі.
- Greed-vs-safety loop: тримати raw stock = більше score-ready Ferronite, але більше SWARM pressure.
- Numbered waves / wave-size tables — **superseded**. Канон: [`14_SWARM`](14_SWARM.md).

Player і AI score race паралельно. Player бачить opponent score у HUD у real time.

### Step 7 — Late Match (7:00 – 9:30)

- Score gap visible. Lagging side агресивніше mineить + shipить, агресивніше push.
- SWARM typically (але не гарантовано) сягає peak — emergent наслідок високого `FerroniteThreatValue` заради shipping. Fast-shipping (low-stock) matches буде з відносно нижчим SWARM.
- Player decides між final push (attack AI base) і final mine + ship (last-second score acceleration; тримати raw stock для shipping = більше threat, але більше score).

### Step 8 — Final 30 Seconds (9:30 – 10:00)

- Players panic-mine + panic-ship — Workers метушаться між deposits, Main Base containers shipають to orbit.
- Last-second container launches тригерять FerroniteScore increments (raw stock → Orbital). Drop-offs підіймають `FerroniteThreatValue`; launches його скидають. Strategic decision: тримати vs shipити raw stock в останні секунди.
- SWARM Threat on peak якщо raw stock високий — emergent, не scripted.

### Step 9 — Match End (10:00)

Match завершується по **одному з двох**:

1. **Delivery quota** (instant win, до timer): перший player чий `FerroniteScore >= DeliveryQuotaFerroniteScore` (placeholder 5000) → негайна перемога, `WinReason = GP.Match.WinReason.DeliveryQuota`.
2. **Timer expiry** (`MatchTimeRemaining <= 0`): якщо ніхто не дотягнув quota — higher `FerroniteScore` wins, `WinReason = GP.Match.WinReason.TimerScore`.

Також: MainBase destroyed = loss if `bAnnihilationCountsAsWin` (`WinReason = GP.Match.WinReason.Annihilation`); multiplayer opponent disconnect → `GP.Match.WinReason.OpponentDisconnect`.

На trigger:

- Server transitions `MatchState = Finished`.
- Server populates `AGP_GameState.MatchResult`:
  - `WinnerTeamId`.
  - `WinReason` (`GP.Match.WinReason.{DeliveryQuota, TimerScore, Annihilation, OpponentDisconnect}`).
  - `MatchDuration`.
  - `FinalScores` map (`FerroniteScore` per player).
- RepNotify тригерить end-of-match UI на клієнтах (singleplayer — local UI).

### Step 10 — End-of-Match Screen

`WBP_GP_EndOfMatch` показує:

- Winner banner з team color.
- Final scores per player ("You: 1,250 / AI: 1,100").
- Win reason ("Score Lead").
- Match duration ("10:00").
- "Return to Menu" button.

Через 15 seconds (або після click) — return до Main Menu. Session closes.

## End-to-End Story (Multiplayer PvP)

Відрізняється від singleplayer тільки у:

### Pre-Match

- Player натискає "Multiplayer (Steam)" у Main Menu.
- `WBP_GP_Lobby` — host створює session OR client joins-by-friend / matchmaking.
- Steam OSS handles connection.
- Коли 2 players у lobby — host натискає "Start Match" → `MatchState = Loading`.

### During Match

- Однаковий core gameplay як у singleplayer.
- Opponent — human player у іншому team color.
- Server (host) — authoritative для всього (commands, score, SWARM, building damage).
- Client — отримує replicated state, відправляє command intent через `Server_RequestCommand` RPC.

### Disconnect Handling (Multiplayer-Specific)

- **Host disconnect:** session terminates → client returns to Main Menu з error.
- **Client disconnect:** server marks player as lost у `AGP_PlayerState.bConnected = false`. Match **продовжується до timer expiry**. Remaining player продовжує mineити, score-ить, defend. На 10:00 — winner = remaining player by default (опонент має frozen score з моменту disconnect).

  WinReason для цього кейсу — `GP.Match.WinReason.OpponentDisconnect`. Worldbuilding rationale — корпорація може втратити comms, але контракт все одно діє, і remaining corporation повинна донести до 10:00 mark.

### Post-Match

- End-of-match screen однаковий як у singleplayer.
- Опція "Return to Lobby" (TBD) або "Return to Menu".

## Required Authority Notes (TDD-Bound)

Усі ці concerns мають server authority (per [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md)):

| Concern | Authority | Notes |
| --- | --- | --- |
| Match state transitions (`Loading → Playing → Finished`) | Server (host) | Replicated through `AGP_GameState.MatchState`. |
| `MatchTimeRemaining` countdown | Server | Replicated, low-frequency (1 Hz). |
| `FerroniteScore` increment | Server | Replicated `COND_None`, monotonically increasing on container launch. |
| `OrbitalFerronite` | Server | Replicated `COND_OwnerOnly`, spendable currency after shipping. |
| `FerroniteThreatValue` | Server | Replicated, raw stock at base (up on drop-off, down on launch); drives SWARM. |
| SWARM unit spawn / AI tick | Server | Multicast тільки death VFX cosmetic. |
| AI opponent decision tick | Server (singleplayer host) | Не client-side. Low frequency (2-5s). |
| Score tie-break execution | Server | Runs on `MatchState = Finished` transition. |
| `MatchResult` struct write | Server | RepNotify тригерить client end-of-match UI. |
| Disconnect detection | Server | `AGP_PlayerState.bConnected` replicated. |

Cross-link TDD update — таблиця Authority Map у [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md) має покривати ці concerns explicitly (виконано у GP-0101 pass).

## Match Cannot Get Stuck

Per validation criteria GP-0101 — "Match cannot get stuck without end condition". Гарантовано наступним:

- **Delivery quota** — instant win як тільки будь-який player досягає `DeliveryQuotaFerroniteScore`.
- **10-min hard timer.** `MatchState` тригерить `Finished` рівно на 0:00, незалежно від game state, якщо quota не досягнута.
- **Score завжди порівнюваний.** Навіть при tie `MatchResult.WinnerTeamId = -1` (Draw) — це **valid result**, match закінчується.
- **Both players без economy assets** — не блокує end. Match все одно triggers Finished по timer; final `FerroniteScore` визначає winner (або draw).
- **Both players disconnect** — match aborts, return-to-menu з error. Не "stuck" — clearly terminated.

Жоден gameplay state не блокує `MatchState = Finished` transition. Server timer завжди runs.

## Read-and-Act Timeline (Player Clarity Check)

Per Pillar Clarity / Pillar 1 — new player повинен зрозуміти що робити за ≤ 30 секунд:

| Time | What Player Sees | What Player Understands |
| --- | --- | --- |
| 0:00 | HUD з Timer / FerroniteScore / OrbitalFerronite / Opponent Score / SWARM Threat. Workers idle. Main Base. Deposit nearby. | "У мене Workers і база. Поруч ресурс. Треба mineити і shipити." |
| 0:05 | Player command Workers → Mine. | "Workers рухаються до deposit." |
| 0:30 | Перший container ships to orbit → FerroniteScore flash. | "Ага, shipити raw Ferronite = очки + spendable currency." |
| 1:00 | SWARM pressure відчутний, якщо raw stock уже росте. | "Це загроза від мого raw stock. Треба захист АБО shipити швидше." |
| 5:00 | Score gap у HUD. | "Я програю за FerroniteScore. Треба mineити + shipити швидше." |
| 9:30 | Timer на 30 секундах. Score race. | "Останні секунди — final push." |
| 10:00 | End screen. | "Match закінчений. Хто переміг — readable." |

Якщо хоча б один з цих moments не intuitive — це clarity gap у `09_UI_UX` або loop design.

## Open Questions

- **Pause у singleplayer:** чи зупиняє match timer? Recommendation: **yes**. Без pause гравець не може відірватися навіть для перерви. Зафіксувати у [`07_Match_Flow`](07_Match_Flow.md) Singleplayer Specifics.
- **Rematch button у end-of-match screen:** TBD. У MVP — defer до Backlog.
- **Spectator after disconnect (multiplayer):** TBD. Recommendation: disconnected player returns to menu immediately, не spectate. Already у TDD/03.
- **Map (one MVP map) — лор + visual:** окремий task `GP-0901 First Playable Map Design` (TBD).
- **AI opponent visual / team color** — TBD. Recommendation: AI grayscale або один з team colors з prefix "AI Corp".
- **Loading screen content** — TBD у [`09_UI_UX`](09_UI_UX.md).

## References

- MVP scope — [`00_Project_Overview`](00_Project_Overview.md).
- Game pillars — [`01_Game_Pillars`](01_Game_Pillars.md).
- Loop primitives — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md).
- Factions і AI scope — [`03_Factions`](03_Factions.md).
- Units і unit roles — [`04_Units`](04_Units.md).
- Buildings — [`05_Buildings`](05_Buildings.md).
- Resources — [`06_Resources`](06_Resources.md).
- Match flow і timing — [`07_Match_Flow`](07_Match_Flow.md).
- Win conditions і tie-break — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- HUD і UX — [`09_UI_UX`](09_UI_UX.md).
- Multiplayer authority — [`../TDD/03_Multiplayer_Architecture`](../TDD/03_Multiplayer_Architecture.md).
- Steam flow — [`../TDD/08_Steam_Matchmaking`](../TDD/08_Steam_Matchmaking.md).
- Lore — [`Lore_Setting`](Lore_Setting.md).
