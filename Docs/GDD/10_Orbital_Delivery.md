# Orbital Delivery System

## Core Fantasy

Гравець керує **видобувною експедицією** на ворожій планеті. Усе, що з'являється на планеті після initial landing — прибуває з орбіти. Player не будує локально. Player order-ить drop pods за Orbital Ferronite. Reference fantasy: Helldivers orbital deployment / supply drops.

Цей doc описує **гравецьке відчуття і правила**. Engineering implementation — [`../TDD/14_Orbital_Delivery`](../TDD/14_Orbital_Delivery.md).

## Two-State Resource (Recap)

Per [`06_Resources`](06_Resources.md) — Ferronite існує у двох станах:

| State | Source | Use |
| --- | --- | --- |
| **Planetary** | Mined by Workers, dropped at MainBase | Sits у Containers. **Не spendable.** Vulnerable. |
| **Orbital** | Container shipped to orbit | **Spendable currency** для orbital drops. Safe (off-planet). |

Conversion: Container full → launch (2-3 s delay, vulnerable window) → Container departs → Orbital Ferronite += Container.Volume.

## What's Orderable (MVP)

| Drop Type | Cost (DA placeholder) | Effect |
| --- | --- | --- |
| **Worker** | Low | Mining / repair unit. Drops near MainBase or chosen point. |
| **Salvage Walker** | Mid | Combat unit. Drops near base or controlled zone. |
| **Defensive Turret** | Mid | Static defense. Player picks location. |
| **Logistics Hub** | High | +5 MaxUnits + expanded container cap. Player picks location. |

Балансові значення — DA-driven, TBD у balance pass per [Pillar 9 — Data-Driven First].

Post-MVP (per Out of Scope): modules, repair drops, upgrade containers, special structures, black market alternates.

## Order Flow (Player View)

1. Player opens **Order Menu** (hotkey `O` or HUD button).
2. Sees list of orderable types з:
   - Icon, name, 1-sentence description.
   - Cost (Orbital Ferronite).
   - Can-afford indicator (green / grey).
   - Cooldown if any (post-MVP for spam-prevention).
3. Player selects type → enters **drop-targeting mode**:
   - Cursor shows drop reticle.
   - Valid drop zones highlighted (controlled territory + actively-visible terrain).
   - Invalid drop zones tinted red (unexplored, water, hostile-occupied — see Validation).
4. Player clicks valid zone → server validates → spend Orbital Ferronite → drop pod scheduled.
5. 2-3 s telegraph: pod descends from sky (visible to all players + SWARM AI). Reticle marker visible on minimap.
6. Pod lands → asset deployed → pod actor destroyed.
7. Player gains control of new asset (or asset is auto-positioned for static structures).

## Drop Zone Validation

| Constraint | Validation |
| --- | --- |
| Player owns valid drop right | Spent Orbital Ferronite successfully (server-side spend gate). |
| Cell у player's actively-visible FoW | Must be currently visible (not just explored). Prevents blind drops behind enemy lines. |
| Cell is navigable terrain (NavMesh-projectable) | `ProjectPointToNavigation` snap with `MaxNavSnapExtent` (per drop type). |
| Not under stationary blocker (existing building, cliff, water) | Sphere overlap check sized to drop type footprint. |
| Within map bounds | World bounds OR explicit drop-allowed boundary actor. |
| Not too close to enemy unit (combat units only, post-MVP) | Reserved hook. Не enforced у MVP. |

Invalid → red reticle tint + `Client_NotifyCommandRejected(OrderDrop, EReason::InvalidDropZone)` + no spend.

## Risk Layer

Drop pod **telegraphs**:

- Visible descent VFX (column of light + smoke trail).
- 3D positional audio.
- Minimap marker.
- 2-3 s delay between order and arrival.

Це створює **window of vulnerability**:
- Opponent може концентрувати fire на drop site.
- SWARM AI може redirect waves до landing zone (post-MVP enhancement).
- Bad drop location = wasted Orbital Ferronite (if pod gets destroyed in flight? або if blocked? — TBD; default `pod always lands`).

## Initial Match State

At match start:

- Player has **MainBase** already deployed (the expedition's landing pod, manually positioned for both sides at map start points).
- Player has **2 initial Workers** already deployed near MainBase (came with initial expedition, no order required).
- Player starts with **0 Orbital Ferronite** (empty pool — per Pillar 3 «empty pool» rule).
- All other content (additional Workers, Salvage Walker, Turret, Logistics Hub) — requires орбітальний цикл.

Worker initial drop NOT through order menu — they're pre-deployed. First "real" order requires shipping enough resource to afford something.

## Core Loop (Expanded — see [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md))

1. Initial landing (state given).
2. Initial Workers scout / mine nearby deposit.
3. Resource drops at MainBase → enters Container → raw Ferronite stored at base → FerroniteThreatValue rises (more swarm pressure).
4. Container fills → ships to orbit → OrbitalFerronite + FerroniteScore increments; FerroniteThreatValue DROPS (raw stock left the base — swarm relief).
5. Player orders Worker / Walker / Turret з OrbitalFerronite pool.
6. Drop pod arrives → asset deployed.
7. Expand control of map.
8. Defend проти SWARM (intensity scales з FerroniteThreatValue = raw Ferronite stored at base RIGHT NOW; hoarding raises it, shipping lowers it).
9. Optionally engage opponent.
10. Win when delivery quota (FerroniteScore) reached OR highest FerroniteScore at timer expiry.

## Match Stages (per Match_Flow)

| Stage | Orbital Delivery Role |
| --- | --- |
| **Early** | Розвідка + перші mining cycles. Containers fill slowly. Maybe 1 small drop (extra Worker). |
| **Mid** | Container ships регулярні. First Salvage Walker / Defensive Turret drops. Логістика налаштована. |
| **Late** | Multiple containers ship simultaneously. SWARM peak. Big drops (Logistics Hub multi-layer). Final-second mass shipping race. |

## Validation per Pillars

**Pillar 8 (Simple Core) check:**
- 1-2 sentence: "Spend Orbital Ferronite, click on map, pod drops with your asset."
- Fun у v1: yes — pod-drop game feel.
- New decision: where/when/what to drop.
- Cheap: ONE pod actor, ONE menu, ONE drop animation.
- Scales via content: more droppable types via DataAsset.

**Pillar 1 (Industrial Extraction First):** orbital delivery is the means; mining/shipping is the score-driver. Consistent.

**Pillar 3 (One Resource):** single currency (Ferronite, у its orbital state). No split currencies.

**Pillar 6 (SWARM as Environmental Pressure):** raw Ferronite hoarded at base raises FerroniteThreatValue → more swarm pressure; shipping it to orbit lowers the threat. Greed-vs-safety loop.

## Open Questions

1. **Drop interruption**: if pod is destroyed mid-flight (post-MVP combat ability), is Orbital Ferronite refunded? Recommend: full refund of spend (drop = service-not-rendered).
2. **Drop cap**: max simultaneous in-flight pods? Recommend: 3 per player у MVP.
3. **Drop point persistence**: rally-point-like memory of last-drop-location? Reserve post-MVP.
4. **Initial Workers spawn point**: tight cluster vs spread? Tight cluster recommended for new-player legibility.
5. **Drop pod cosmetic**: shared visual for all drop types vs per-type animation? Recommend shared у MVP, per-type у post-MVP.

## Out of MVP

- Repair modules (orbital service for damaged units).
- Upgrade containers (orbital tech tree).
- Special structures (research, scanners, communication relays).
- Modules для existing units (combat retrofit).
- Drop cooldowns / rate limits.
- Drop pod intercept abilities (player-vs-player anti-drop).
- Black market alternate adressees (different orbital recipients with different reward catalogs).

## References

- Core Loop — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md).
- Resources / Containers — [`06_Resources`](06_Resources.md).
- Match Flow — [`07_Match_Flow`](07_Match_Flow.md).
- Win Conditions — [`08_Win_Lose_Conditions`](08_Win_Lose_Conditions.md).
- Fog of War (drop targeting depends on visibility) — [`11_Fog_of_War`](11_Fog_of_War.md).
- Engineering implementation — [`../TDD/14_Orbital_Delivery`](../TDD/14_Orbital_Delivery.md).
- Pillars — [`01_Game_Pillars`](01_Game_Pillars.md) (Pillar 1, 3, 6, 8).
