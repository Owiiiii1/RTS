# Fog of War

## Scope

3-level FoW як MVP feature (pivot 2026-05-16 — раніше було "deferred"). Цей doc — гравецьке відчуття і design rules. Engineering implementation — [`../TDD/15_Fog_of_War`](../TDD/15_Fog_of_War.md).

## Three States Per Map Cell

| State | Player Sees | Live Updates |
| --- | --- | --- |
| **Unexplored** | Чорна territory. Нічого: ані ландшафту, ані ресурсів, ані ворогів. | None. |
| **Explored** | Бачить terrain, map structure. **Last-known state** статичних об'єктів (buildings, deposits — на момент останнього sight). | None (frozen snapshot). |
| **Actively Visible** | Full real-time: вороги, рух, події, current state. | Yes (live). |

## Sight Sources

Кожен owned об'єкт із `SightRadius > 0` контрибут'ить vision до своєї території:

| Source | Sight (placeholder, DA-driven) |
| --- | --- |
| Worker | Short |
| Salvage Walker | Mid (combat unit — більше) |
| Defensive Turret | Mid (stationary, wider than Worker) |
| MainBase | Wide |
| Logistics Hub | Mid |
| Drop pod in-flight | Temporary wide at landing site (telegraph effect) |

## Design Intent

**Why FoW now (was previously out of MVP):**

- Orbital drop mechanic створює legitimate scouting decision. Без FoW — drop "anywhere" не має cost.
- PvP read of opponent requires hidden information. Без FoW — обидва гравці бачать все, no surprise factor.
- Greed-vs-safety risk loop (per Container System) працює only with hidden enemy intent.
- SWARM wave anticipation потребує scouting effort.

**What FoW enables:**

- Scout fantasy (sending Worker forward).
- Ambush moments (combat unit appears from explored-but-not-visible area).
- Tactical reveal cost (must commit vision to attack target).
- Memory of map (explored stays explored).

## Per-Player FoW

Кожен player team має свій exploration / visibility grid. Player A's exploration не shared з Player B (no allies у MVP).

## Drop Targeting Interaction

Per [`10_Orbital_Delivery`](10_Orbital_Delivery.md) drop zone validation:

- **Орбітальний drop** дозволяється тільки на **Actively Visible** cells.
- Explored-but-not-visible: red reticle, no drop.
- Unexplored: black mask, cursor не register hit.

Це принципово: orbital drop **потребує** vision. Player must scout territory before deploying там. Combat unit deploy у explored area = blind drop (could land into ambush). Player вирішує.

## Selection / Inspect Interaction

Per [`../TDD/04_RTS_Selection_And_Commands`](../TDD/04_RTS_Selection_And_Commands.md) GP-0202 updated rules:

- LMB on hidden actor: no-op (cursor doesn't hit hidden actors).
- Marquee: тільки visible actors enter SelectedUnits.
- Inspect enemy: тільки visible enemies inspectable.
- Control group recall: members у hidden areas — last-known position used; UI shows "unknown" status.

## Combat Interaction

Per GP-0204 auto-acquire updated:

- `UGP_TargetingComponent` filter: target must be у player's `VisibleByTeam` grid.
- LOS multi-trace (existing 3-trace) — additional physical line check after FoW filter.
- Explicit Attack command на target that becomes hidden: command persists, attacker chases last-known location; re-engage on re-sight.

## Last-Known State

Explored-but-not-visible static actors (buildings, deposits) show on minimap and main view at their **last-known position** with last-known visual state:

- Building takes damage while you can see it → see damage visualization. Lose vision → frozen at that damage level until re-sight.
- Building destroyed while invisible → still appears on minimap "as last seen" з faded icon. Re-explore → reveals destruction.
- Deposit depletes invisible → still shows full capacity until re-sight.

**Fading rules:**
- Buildings: last-known state lasts indefinitely while у Explored zone.
- Units (dynamic actors): last-known marker fades 5 s after losing vision. After 5 s, marker disappears.

## Minimap

Three-layer render:

- **Black** mask for Unexplored.
- **Dim grey** for Explored (terrain visible, last-known structures shown faded).
- **Full color** for Actively Visible.

Camera viewport rectangle still drawn. Drop pod incoming markers shown to dropping player (and revealed on impact to opponent's visible area).

## Initial Match FoW State

- Map starts fully **Unexplored** for both players.
- Initial MainBase + 2 Workers landing sites → small **Actively Visible** bubble around each side's starting cluster.
- Player must move units to explore beyond initial bubble.

## Validation per Pillars

**Pillar 8 (Simple Core):**
- 1-2 sentence: "Three layers: black (never seen), grey (seen once but not now), full (currently looking)."
- Fun у v1: yes — scout fantasy + ambush.
- New decision: vision pyramid placement, pre-attack reveal, scouting investment.
- Cheap: bit-grid + relevance API, не custom shader.
- Scales: more units / structures з різним `SightRadius` add via DataAsset.

**Pillar 1 (Extraction):** scout cost = part of expansion calculus.

**Pillar 6 (SWARM):** scouting effort scales з ambition; player must invest у sight to see incoming waves.

## Edge Cases

| Case | Behavior |
| --- | --- |
| Last vision source dies | Cell transitions Visible → Explored. Live updates freeze. |
| Drop pod telegraph reveals enemy zone temporarily | During pod descent, landing cell + small radius temporarily Visible. After landing, dropped asset becomes vision source. |
| Worker mines у Explored-not-Visible deposit (logically impossible — must move to deposit) | Worker arriving at deposit re-creates Visible bubble. Deposit current state revealed. |
| Combat unit auto-acquires enemy that becomes hidden mid-fire | CombatComponent finishes current attack tick; next tick — target.Visible check fails — disengage, chase last-known. |
| Player tries to drop into Explored area | Reticle red, rejection. |
| Player ally vision sharing | Out of MVP (no allies). |
| Stealth abilities | Out of MVP. |

## Out of MVP

- Allied vision sharing (no allies).
- Stealth units / cloak abilities.
- Scanner reveal abilities (ping reveal).
- "Always visible" landmarks (e.g., neutral structures).
- Fog re-application (visibility can be lost — only Visible → Explored downgrade, no Explored → Unexplored).
- Vision-cone direction (omni 360° у MVP).
- Height / elevation vision effects.

## References

- Core fantasy — [`00_Project_Overview`](00_Project_Overview.md).
- Orbital Delivery (drop zone FoW interaction) — [`10_Orbital_Delivery`](10_Orbital_Delivery.md).
- Engineering implementation — [`../TDD/15_Fog_of_War`](../TDD/15_Fog_of_War.md).
- Selection rules — [`../TDD/04_RTS_Selection_And_Commands`](../TDD/04_RTS_Selection_And_Commands.md).
- Pillars — [`01_Game_Pillars`](01_Game_Pillars.md) (Pillar 8 — Simple Core check).
