# UI / UX

> **Orbital procurement (2026-08-08):** No local Build menu. Unit Order = manifest → Unit Drop Zone. Building Order = Purchase→READY→Deploy ghost. See [`10_Orbital_Delivery`](10_Orbital_Delivery.md). Command bar shows Move/Stop/Attack/Mine/Repair — **not** Build.
>
> **GP-0305R (2026-08-18):** Wall is not READY. Order Menu **Buy Wall Package** (stock 0, not in-flight). After delivery, **Build Wall** is a separate action (not an orbital purchase). State contract only — no final layout. See GDD/10 flow C.

## MVP UI Surface

Мінімально необхідний набір екранів і HUD elements для playable target.

### Screens

| Screen | Purpose |
| --- | --- |
| Main Menu | Start game, Host, Join via Steam, Singleplayer (vs AI), Quit |
| Lobby | Player list, faction display (read-only у MVP), Start Match (host only) |
| In-Match HUD | Selection panel, command bar, resource readout, score readout, opponent score, SWARM aggression indicator, minimap, match timer |
| End-of-Match | Winner display, final scores per player, match duration, return to menu |
| Pause / Settings (singleplayer only) | Resume, settings, quit to menu |

### HUD Layout (In-Match)

```
+----------------------------------------------------------+
| [Match Timer: 09:42]      [Ferronite: 240 / Score: 1,250]|
|                           [Opponent Score: 1,100]        |
|                           [SWARM Aggression: Moderate]     |
|                                                          |
|                                                          |
|                                                          |
|                                          [Minimap]       |
| +--------------------+   +---------------+               |
| | Selection Panel    |   | Command Bar   |               |
| | (selected units +  |   | (Move/Stop/   |               |
| |  group portraits)  |   |  Attack/Mine)|               |
| +--------------------+   +---------------+               |
+----------------------------------------------------------+
```

### Widgets (Naming per `/STYLE.md`)

- `WBP_GP_MainMenu`
- `WBP_GP_Lobby`
- `WBP_GP_HUD_Match` (root in-match widget)
- `WBP_GP_HUD_SelectionPanel`
- `WBP_GP_HUD_CommandBar`
- `WBP_GP_HUD_ResourceReadout` (Orbital Ferronite + Score + Opponent Score)
- `WBP_GP_HUD_MatchTimer` (countdown)
- `WBP_GP_HUD_SwarmThreat` (SWARM threat indicator — `FerroniteThreatValue`)
- `WBP_GP_HUD_Minimap` (mandatory у MVP, не optional)
- `WBP_GP_EndOfMatch`

## UX Principles

### Read-and-Act in ≤ 1s

Будь-який gameplay-critical readout (Match Timer, Ferronite Pool, Score, Opponent Score, SWARM Aggression) має бути читабельний за ≤ 1 second. Велика числа, чіткий contrast, single readout, без декорацій.

### Score Readout Prominence

Score — primary win condition (per [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md)). Has explicit visual weight:

- Player Score — top-center або top-right, велика цифра, color-coded по team color.
- Opponent Score — поруч, меншим шрифтом, для постійного gap awareness.
- Score delta (`+50` при drop-off) — flash animation на 1-2 секунди.

Без score readout гравець не розуміє, чи виграє. Це **mandatory MVP feature**.

### Match Timer Prominence

10-хвилинний countdown — primary tension driver. Display:

- Top-center або top-left.
- Велика цифра з MM:SS форматом.
- Color shift у останні 60 s (yellow) і 15 s (red) для urgency.

### Command Predictability

Коли player issues command — UI показує immediate marker (e.g., ground pulse на click location) **до** server confirm. На server confirm (replicated state) — marker валідується або зникає.

Це not "client-side gameplay" — це **presentation feedback**. Real command resolves server-side.

### Selection Clarity

Selected units — render з selection ring (cosmetic decal або material highlight). Selection state — local-only (PlayerController), не replicated.

Selection panel показує:

- Single unit selected → detail panel (HP bar, abilities, current command).
- Multi-unit selection → group portraits з count і type icons.
- Mixed selection (units + building) → priority до selected building OR group panel summary (TBD у UX pass).

### SWARM Aggression Indicator

SWARM aggression — gameplay-critical readout. Form (TBD):

- Bar (left-to-right, fills with aggression level).
- Color shift (green → yellow → orange → red).
- Optional icon (e.g., warning marker on minimap при wave спавн).

Гравець має знати рівень threat за ≤ 1 second.

### Minimap

**Mandatory у MVP.** Показує:

- Player base (own and opponent, if visible).
- Ferronite deposits (location + capacity status).
- Own units і buildings.
- SWARM spawn locations (на момент wave спавн — pulse marker).
- Camera viewport rectangle.

Mini-map clickable — re-center camera. Mini-map тримає player у курсі map state, особливо при mid-match expansion.

## Command Issuance (Hybrid Model)

Owner decision: **hybrid right-click context + hotkeys.**

### Right-Click Context Default

- RMB на ground → Move.
- RMB на enemy → Attack.
- RMB на Ferronite Deposit → Mine (only Worker).
- RMB на own building → Repair або Drop-off (context-dependent, MVP — Drop-off implicit при Move).
- ~~RMB на construction site → Continue Build~~ — removed (no local construction).

Smart context, що читає target і issues найбільш intuitive command.

### Modal Hotkeys

- `A` → Attack-move mode (next click — attack-move command).
- `M` → Move mode (next click — explicit move, no auto-target).
- `S` → Stop selected units.
- `O` → Orbital Order / procurement UI (units + buildings panels).
- ~~`B` → Build menu~~ — removed (orbital model; no Worker Build).
- `P` → Patrol mode (if/when AttackMove/Patrol ships).
- `Esc` → Cancel current mode (incl. building deploy ghost).

### Other Mandatory Hotkeys

- `LMB` — select / marquee select.
- `Shift+LMB` — additive selection.
- `Ctrl+Number` — group bind (TBD у MVP — рекомендується defer, але hotkey-handled).
- `Number` (1-9) — group recall.
- `Space` — center camera on selection.

### Camera Controls

- `WASD` або edge-scroll — pan.
- `Mouse Wheel` — zoom.
- `Q / E` — rotate.
- `Z` (TBD) — reset rotation і zoom to default.

## UI Source of Truth

UI — read-only consumer gameplay state:

- Orbital Ferronite (spendable) → `UGP_PlayerAttributeSet.OrbitalFerronite` (`COND_OwnerOnly`, via `AGP_PlayerState`).
- Score → `UGP_PlayerAttributeSet.FerroniteScore` (cumulative shipped; `COND_None`, visible to all).
- Unit cap → `UGP_PlayerAttributeSet.CurrentUnits / MaxUnits`.
- Selected units → `UGP_SelectionComponent.SelectedUnits` (local).
- Match state → `AGP_GameState.MatchState`.
- Match timer → `AGP_GameState.MatchTimeRemaining`.
- SWARM pressure → `AGP_GameState.FerroniteThreatValue` (raw Ferronite stored at MainBase; up on drop-off, down on orbital launch).

Widgets reading state — через AbilitySystemComponent attribute change delegates або prop binding (не tick polling).

## End-of-Match Screen

Mandatory display:

- Winner banner (з team color).
- Final score per player.
- Win reason (e.g., "Score Lead", "Opponent Disconnect", "Tie-Break: Mining Rate", "Draw").
- Match duration.
- Score timeline (optional MVP — line graph of score over time).
- "Return to Menu" / "Rematch" (rematch — TBD).

## Out of MVP

- Research panel.
- Build queue UI.
- Notification toast system.
- Replay UI.
- In-game chat.
- Fog of war minimap rendering (vision system — Backlog).
- Tooltips / tutorials.
- Score history / leaderboard (across matches).
- Off-world upgrade panel (orbit meta — Backlog).
- Pings / contextual map markers.

## References

- Selection / command pipeline — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md), [`../TDD/04_RTS_Selection_And_Commands`](../TDD/04_RTS_Selection_And_Commands.md).
- Player state attributes — [`../TDD/02_GAS_Architecture`](../TDD/02_GAS_Architecture.md).
- Match state і timer — [`07_Match_Flow`](07_Match_Flow.md).
- Score model — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- SWARM aggression — [`03_Factions`](03_Factions.md), [`07_Match_Flow`](07_Match_Flow.md).
- UI TDD page — `(TDD — TBD)` planned `TDD/11_UI_Architecture.md`. Tracked у `Claude_Task_Backlog`.
