# UI / UX

> **Orbital procurement (2026-08-08):** No local Build menu. Unit Order = manifest → Unit Drop Zone. Building Order = Purchase→READY→Deploy ghost. See [`10_Orbital_Delivery`](10_Orbital_Delivery.md). The Context Action Grid is not a Build Menu.
>
> **GP-0305R (2026-08-18):** Wall is not READY. **Buy Wall Package** (stock 0, not in-flight). After delivery, **Build Wall** is a separate action (not an orbital purchase). State contract only. See GDD/10 flow C.
>
> **Production HUD layout (2026-08-21):** The coarse HUD (resource/score top-right, selection bottom-left, command bar bottom-center, minimap top/bottom-right) is **SUPERSEDED**. Canonical IA: two horizontal bars × three blocks. See [`Claude_Tasks/GP-Production-HUD-Layout-Spec`](../Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md).
>
> **Production HUD procurement (2026-08-21):** Orbital procurement is **not** a permanent global HUD panel. Canonical visible entry: select MainBase → **PURCHASE** in the bottom-right Context Action Grid → UNITS / BUILDINGS / DEFENSE. A global `O` Order Menu is **SUPERSEDED** as the production HUD path. TEMP HUD debug procurement may remain as temporary scaffolding. Backend authority unchanged.

## MVP UI Surface

Мінімально необхідний набір екранів і HUD elements для playable target.

### Screens

| Screen | Purpose |
| --- | --- |
| Main Menu | Start game, Host, Join via Steam, Singleplayer (vs AI), Quit |
| Lobby | Player list, faction display (read-only у MVP), Start Match (host only) |
| In-Match HUD | Two-bar layout: Threat+Score, Match Timer, Planet/Orbit/Cap; Minimap placeholder, Selection/Info, Context Action Grid |
| End-of-Match | Winner display, final scores per player, match duration, return to menu |
| Pause / Settings (singleplayer only) | Resume, settings, quit to menu |

### HUD Layout (In-Match) — canonical

Two horizontal bars. Three major blocks per bar. The central battlefield stays unobstructed.

```
+-----------------------------------------------------------------------+
| [ THREAT + SCORE ]       [ MATCH TIMER ]       [ PLANET / ORBIT / CAP ]|
+-----------------------------------------------------------------------+
|                                                                       |
|                         GAME WORLD                                    |
|                                                                       |
+-----------------------------------------------------------------------+
| [ MINIMAP ]       [ SELECTION / CURRENT INFO ]       [ ACTION GRID ]  |
|   square                  wide rectangle                table/grid     |
+-----------------------------------------------------------------------+
```

The previous layout (resource/score stack top-right, selection bottom-left, command bar
bottom-center, minimap top-right or bottom-right) is **SUPERSEDED**.

**Top left — Threat + Score**

- Ferronite Threat = danger/pressure presentation. Bind the bar to
  `UGP_MatchViewModel.FerroniteThreatNormalized` (`[0,1]`).
- Normalization: `Clamp(FerroniteThreatValue / (MainBase storage GetTotalCapacity() × GetThreatPerStoredUnit()), 0, 1)`.
  Invalid/zero denominator → 0. This is presentation only — not a gameplay ThreatMax.
  Raw `FerroniteThreatValue` remains available. No hardcoded 1000.
- Player Ferronite Score.

Raw Ferronite stored at MainBase is the same underlying quantity that currently drives threat.
The HUD may show it twice: Threat as pressure, Planet Ferronite as the exact number.
Do not invent a second currency. Operator-validated: authored `PB_Threat` binds
`FerroniteThreatNormalized` and `ThreatToColor` interpolates green → yellow → red.
`WBP_GP_HUD` and `ThreatToColor` remain operator-local / uncommitted. Do not claim final art
or final Threat tuning complete. Current gameplay values can fill the bar quickly; later
UX/balance tuning may revisit the presentation scale.

**Top center — Match Timer**

- Strongest isolated central readout. Countdown.

**Top right — Economy + Unit Cap**

- Planet Ferronite (exact stored amount)
- Orbital Ferronite (spendable)
- Unit Capacity (`CurrentUnits / MaxUnits`)

**Bottom left — Minimap block**

- Square reserved region. Minimap function is not implemented in the next visual HUD slice.
  Placeholder sizing/alignment only.

**Bottom center — Selection / Current Info** (widest lower block)

- **Single-entity mode** (exactly one unit or one building): icon, display name, current health,
  relevant stats only (Health/Max, Damage, Armor, Move Speed, later cargo/work where applicable).
- **Group mode** (multiple units): 10×3 icon grid (30 visible slots). Each icon has a small
  health bar beneath it. Overflow beyond 30 is TBD / UX DESIGN REQUIRED. Do not cap gameplay
  selection to 30.

**Bottom right — Context Action Grid + Message Strip**

- Not a permanent Build Menu or global Order Menu. Mode follows selection.
- A small **Message Strip** sits directly above this block for short contextual
  procurement/action feedback (may be empty). Not a global toast system.
- **Unit Action Mode:** Move, Stop, Attack-Move ("идти с атакой"), Patrol (planned / not implemented).
  Direct RMB target Attack remains a separate contextual behavior and is not Attack-Move.
- **Building Action Mode:** building-specific actions. Only **MainBase** owns **PURCHASE**.
- **MainBase PURCHASE** (design / not implemented): replaces the grid with UNITS / BUILDINGS /
  DEFENSE inside the same panel. Bottom-center stays on MainBase info.
- BUILDINGS / Defensive Turret LAUNCH = existing Purchase → READY → immediately enter deploy ghost.
  Wall Package LAUNCH = existing Buy Wall Package (no READY, no placement mode).
- TEMP HUD procurement controls remain temporary scaffolding.

**Visual prototype:** medium/dark grey major blocks, lighter grey inner cells, thin borders,
modest rounding, stronger contrast for selected/hover. No final art, textures, or icons required.

### Widgets (Naming per `/STYLE.md`)

Future authored names; none of these visual widgets are implemented yet.

- `WBP_GP_MainMenu`
- `WBP_GP_Lobby`
- `WBP_GP_HUD` (root; native base `UGP_HUDRootWidget`)
- Top-bar blocks: Threat+Score, Match Timer, Planet/Orbit/Cap
- Bottom-bar blocks: Minimap placeholder, Selection/Info, Context Action Grid + Message Strip
  (MainBase PURCHASE lives in this panel; not a fullscreen Order Menu)
- `WBP_GP_EndOfMatch`

## UX Principles

### Read-and-Act in ≤ 1s

Будь-який gameplay-critical readout (Match Timer, Ferronite Pool, Score, Opponent Score, SWARM Aggression) має бути читабельний за ≤ 1 second. Велика числа, чіткий contrast, single readout, без декорацій.

### Score Readout Prominence

Score — primary win condition (per [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md)). Has explicit visual weight:

- Player Score — top-left Threat+Score block, велика цифра, color-coded по team color.
- Opponent Score — not in the approved two-bar prototype; placement TBD. Do not restore the old top-right stack.
- Score delta (`+50` при drop-off) — flash animation на 1-2 секунди.

Без score readout гравець не розуміє, чи виграє. Це **mandatory MVP feature**.

### Match Timer Prominence

10-хвилинний countdown — primary tension driver. Display:

- Top-center. Isolated strongest readout.
- Велика цифра з MM:SS форматом.
- Color shift у останні 60 s (yellow) і 15 s (red) для urgency.

### Command Predictability

Коли player issues command — UI показує immediate marker (e.g., ground pulse на click location) **до** server confirm. На server confirm (replicated state) — marker валідується або зникає.

Це not "client-side gameplay" — це **presentation feedback**. Real command resolves server-side.

### Selection Clarity

Selected units — render з selection ring (cosmetic decal або material highlight). Selection state — local-only (PlayerController), не replicated.

Bottom-center Selection / Current Info:

- Exactly one unit or building → single-entity mode (icon, name, health, relevant stats).
- Multiple units → 10×3 icon grid with per-icon health bars (30 visible slots; overflow TBD).
- Mixed selection (units + building) remains forbidden by selection rules.

### SWARM Aggression Indicator

SWARM / Ferronite Threat lives in the **top-left Threat + Score** block. Form:

- Pressure bar bound to `FerroniteThreatNormalized` (derived from actual MainBase storage
  capacity × ThreatPerStoredUnit). Not a gameplay threshold. No hardcoded ThreatMax.
- Raw `FerroniteThreatValue` remains available for numeric readout if authored.
- Operator-validated: `PB_Threat.Percent` updates in PIE; operator-local `ThreatToColor`
  interpolates green → yellow → red on Fill Color and Opacity.
- `WBP_GP_HUD` and `ThreatToColor` remain uncommitted. Final art is not claimed complete.
- Current gameplay/storage tuning can reach high/full threat quickly; later UX/balance
  tuning may revisit the presentation scale. This is not a defect in the normalization slice.
- Optional later minimap wave marker is minimap-slice work, not this layout.

Гравець має знати рівень threat за ≤ 1 second.

### Minimap

**Mandatory у MVP as a reserved bottom-left square.** Function is a later slice; the next visual HUD
only reserves the placeholder. When implemented it показує:

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
- `O` → optional later convenience to open MainBase procurement. **Not** the canonical production HUD entry.
  Canonical path: select MainBase → PURCHASE. Global Order Menu is superseded for production HUD.
- ~~`B` → Build menu~~ — removed (orbital model; no Worker Build).
- `P` → Patrol mode (**PLANNED / DESIGN TARGET**; Patrol is not runtime-complete).
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
- Planet Ferronite (exact stored amount) → MainBase `UGP_StorageComponent` (same underlying quantity that currently drives threat).
- SWARM / Ferronite Threat (pressure presentation) → `UGP_MatchViewModel.FerroniteThreatNormalized`
  (bar) derived from `AGP_GameState` per-team threat and local MainBase storage capacity ×
  ThreatPerStoredUnit. Raw `FerroniteThreatValue` remains. Not a gameplay threshold.
- Context Action Grid commands → local selection + `AllowedCommands` (not a Build Menu).
- MainBase PURCHASE / Message Strip → existing `UGP_OrbitalDeliverySubsystem` catalogs, manifests,
  READY inventory, Wall Package stock, and `PodTransportSlotCapacity` (server-authoritative).

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
- UI TDD — [`../TDD/12_UI_Architecture.md`](../TDD/12_UI_Architecture.md).
- Approved HUD IA — [`../Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md`](../Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md).
