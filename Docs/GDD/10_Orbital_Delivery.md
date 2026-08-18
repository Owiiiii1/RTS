# Orbital Delivery System

## Core Fantasy

Гравець керує **видобувною експедицією** на ворожій планеті. Усе, що з'являється на планеті після initial landing — прибуває з орбіти. Player **нічого не виробляє локально**. Reference fantasy: Helldivers orbital deployment / supply drops.

**Canonical pipeline:**

```
Planetary Ferronite → containers → launch → Orbital Ferronite
→ orbital procurement → orbital delivery → deployed asset
```

**NO:** barracks/factory production, Worker Build ability, local construction queue, local unit production.

Цей doc описує **гравецьке відчуття і правила**. Engineering — [`../TDD/14_Orbital_Delivery`](../TDD/14_Orbital_Delivery.md).

> **Owner refinement (2026-08-08):** Unit delivery and building procurement are **two flows** sharing one DropPod/rocket presentation. Units land at the MainBase **Unit Drop Zone** (no free world placement). Buildings are **purchased into orbital inventory (READY)** then **deployed later** via ghost placement — spend happens at purchase, not at placement confirm.
>
> **Owner refinement (2026-08-18, GP-0305R):** Wall is a **third flow**. Buy a **Wall Package of 5** → one rocket to MainBase → MainBase Wall inventory (max 5). **Build Wall** places from that inventory. Not READY. Not per-segment pods. See [`../Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md`](../Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md).

## Two-State Resource (Recap)

Per [`06_Resources`](06_Resources.md):

| State | Source | Use |
| --- | --- | --- |
| **Planetary** | Mined by Workers, dropped at MainBase | Containers. **Not spendable.** Vulnerable. |
| **Orbital** | Container shipped to orbit | **Spendable currency** for orbital procurement. Safe. |

## Philosophy — Reaffirm

| Rule | Meaning |
| --- | --- |
| No local production | Nothing is trained or Worker-constructed on the planet. Wall segments are placed from already-delivered package stock (not a new local production path). |
| Initial exception only | MainBase + **2 Workers** pre-deployed. |
| All else from orbit | Additional Workers, Salvage Walkers, Logistics Hub, Turrets, Wall Packages, Wall-mounted Turrets — orbital only. Wall **material** arrives as a package; segments are not individual pods. |
| Shared delivery actor | Units, READY buildings, and Wall **Package** use the **same** MVP DropPod/rocket visual family. `AGP_Wall` segments do **not**. |

## Procurement Flows

| Flow | Spend moment | Placement | Payload |
| --- | --- | --- | --- |
| **A. Unit Delivery** | On Confirm Order (manifest) | Fixed: MainBase **Unit Drop Zone** | 1..N units packed by **Transport Slots** |
| **B. Building Procurement + Deployment** | On **Purchase** → READY inventory | Later: ghost placement → DropPod to chosen cell | Exactly **one** READY building item |
| **C. Wall Package** | On **Buy Wall Package** | None at purchase. Later **Build Wall** from MainBase inventory | One rocket: **5** segments into MainBase Wall stock (max 5) |

Buildings (Hub / Turret / WallTurret) do **not** land in the Unit Drop Zone. Units do **not** use free world placement for normal orders. Wall Package lands at **MainBase** (not a grid ghost, not READY).

---

## A — Unit Delivery

### MainBase Unit Drop Zone

Player does **not** choose a world landing point for normal unit orders.

All ordered player units arrive at an authored **Unit Drop Zone** relative to MainBase:

- Authored scene anchor / component on MainBase (exact class = implementation decision).
- Server-resolved; replicated delivery targets the same authoritative point.
- Owner can move the landing pad in BP/MainBase visual **without** C++ rewrite of offsets.
- Location must **not** be hardcoded as `BaseLocation + FVector(...)`.

Unit Drop Zone is for **unit transport pods only**.

### Transport Slots (pod capacity)

Unit orbital delivery uses **Transport Slot Capacity** of one pod — **different** from `MaxUnits` / `CurrentUnits`.

| Concept | Meaning |
| --- | --- |
| `PodTransportSlotCapacity` | Capacity of **one** delivery pod (data-driven). |
| `UnitTransportSlotCost` | Slots consumed per unit type (data-driven). |
| `MaxUnits` / `CurrentUnits` | Total army/workforce cap (player attribute). |

**MVP tuning example (not immutable balance):**

| Field | Example |
| --- | --- |
| Pod capacity | **4** slots |
| Worker | **1** slot |
| Salvage Walker | **2** slots |

Valid packings (examples): 4 Workers; 2 Salvage Walkers; 2 Workers + 1 Salvage Walker; etc.

**Rule:**

```
sum(UnitCount × UnitTransportSlotCost) <= PodTransportSlotCapacity
```

Future-proof schema (do not implement now): larger pods, doctrines/upgrades modifying capacity, heavier units with higher slot costs.

### Unit procurement flow (player view)

1. Player has Orbital Ferronite.
2. Opens Unit Order / Orbital Procurement UI.
3. Builds a **pod manifest** up to transport-slot limit.
4. UI shows: slots used/capacity, unit counts, per-unit Orbital cost, per-unit slot cost, **total Orbital cost**.
5. Confirm Order.
6. Server validates: funds, transport slots, unit-cap, MainBase / Unit Drop Zone validity.
7. Orbital Ferronite spends **exactly once** (GAS Instant spend GE).
8. One DropPod/rocket scheduled to Unit Drop Zone.
9. Rocket descends (~2–3 s, data-driven).
10. Landing FX → units spawn/deploy at/around Drop Zone (deterministic offsets).
11. Units become selectable/controllable.

**No world-placement mode** for normal unit deliveries.

### Unit Cap vs Transport Slots

- If complete manifest would exceed free `MaxUnits` room → **reject entire order** (simplest deterministic MVP). Do **not** silently partial-fill.
- Do not conflate `PodTransportSlotCapacity` with `MaxUnits`.

### Multi-unit spawn

Do not spawn multiple units at the identical transform. Server uses deterministic authored/simple formation offsets around the Drop Zone / pod center. Collision-safe spacing; no complex formation AI.

---

## B — Building Procurement + Deployment

Buying and deploying are **two actions**.

### Purchase → Orbital Building Inventory

1. Player selects a building type (Logistics Hub, Defensive Turret, Wall-mounted Turret, …). **Not Wall** — Wall uses flow C.
2. Pays Orbital Ferronite **immediately** (GAS spend).
3. Building becomes owned **orbital inventory** item with status **READY**.
4. It does **not** descend yet.

Example UI: `Logistics Hub ×1 — READY`, `Defensive Turret ×2 — READY`.

Authoritative MVP representation (conceptual): `(BuildingType / DropDefinition, ReadyCount)` — server-authoritative, supports multiples of same type, owner-visible UI, decrement exactly once on accepted deploy. Exact type = implementation decision for the building slice.

### Deploy READY item

1. Player clicks a READY item → **building deployment mode**.
2. Semi-transparent building ghost follows cursor; footprint shown; valid/invalid visuals.
3. Final FoW/grid rules layer in when those systems exist.
4. **LMB** on valid location: consume **one** READY item → schedule DropPod to that location. **No second Orbital charge.**
5. **RMB / Esc:** cancel placement; item stays READY; no refund (purchase already completed).

Server must prevent: double-deploy of same item, duplicate RPC double-spawn, losing READY on client-only cancel.

### Building drop target

Building pods use player-confirmed placement. Same DropPod/rocket actor/visual as units. Payload = exactly one READY building.

---

## C — Wall Package

1. **Buy Wall Package** when MainBase Wall stock is **0..4** and no package is in flight.
2. Spend full `UGP_WallPackageDefinition.Cost` once. Do **not** enter placement. Price does not prorate.
3. One rocket delivers the package to MainBase **UnitDropZone**. Payload is inventory, not an `AGP_Wall` actor.
4. On arrival: `Accepted = min(5, Capacity - current stock)`. Excess wasted. No refund.
5. **Build Wall** (separate action) drags a path limited by stock; confirm consumes N and places N `AGP_Wall` immediately. No second Orbital spend. No READY. No extra rocket.

Cannot repurchase at stock 5 or while a package is in flight. MainBase destroyed → remaining stock is lost.

---

## Shared MVP Drop Rocket / Presentation

Units and buildings share the **same** delivery animation family.

Desired MVP sequence:

1. Rocket/pod appears above landing target.
2. Descends vertically (~2–3 s, data-driven).
3. Simple cylinder/rocket body + Niagara exhaust while descending.
4. Impact Niagara (smoke/dust).
5. Rocket clears/opens/disappears.
6. Payload appears.
7. DropPod cleans up.

**Gameplay native (`AGP_DropPod`):** authority, replication, descent lifecycle, timing, landing, payload spawn, presentation hooks.  
**Authored visual (e.g. `BP_DropPod_MVP`):** mesh, exhaust Niagara, impact Niagara, scale/offsets — soft-ref / BP class. Owner replaces visuals without rewriting delivery gameplay.

Conceptual hooks: OnDescentStarted, descent progress/transform, OnImpact, OnPayloadDeploy (exact C++ names = implementation).

### Payload spawn timing (MVP recommendation)

Authority spawn occurs **once** at deterministic landing-complete. Impact FX can mask reveal; optional short configurable cosmetic delay. Prefer multiplayer-safe single authoritative spawn over cinematic delayed authority.

---

## What's Orderable (MVP roster)

| Asset | How acquired | Notes |
| --- | --- | --- |
| **Worker** | Unit manifest → Unit Drop Zone | Slot cost example 1 |
| **Salvage Walker** | Unit manifest → Unit Drop Zone | Slot cost example 2 |
| **Logistics Hub** | Purchase → READY → deploy ghost | Cap / logistics expansion |
| **Defensive Turret** | Purchase → READY → deploy ghost | Static defense |
| **Wall Package** | Purchase → one rocket to MainBase → stock 5 | Then **Build Wall** from inventory |
| **Wall-mounted Turret** | Purchase → READY → deploy on wall | Mounted defense |
| **MainBase** | Initial only | Not purchased |
| **Ferronite Deposit** | Environment | Not purchased |

Roster = MVP **target catalog**, not one implementation task. Next playable slice may ship **units only**.

Costs / slot values — DataAsset-driven, TBD balance (Pillar 9).

---

## Drop Zone Validation

### Unit pods

| Constraint | Validation |
| --- | --- |
| Manifest affordability | Orbital >= total cost; spend once via GAS |
| Transport slots | Sum of slot costs <= pod capacity |
| Unit cap | Full manifest must fit `MaxUnits`; else reject |
| Unit Drop Zone | Valid authored MainBase anchor for owning team |

No free-world FoW click for normal unit drops.

### Building pods (deploy)

| Constraint | Validation |
| --- | --- |
| READY inventory | Exactly one READY item of requested type available |
| Placement | Grid / footprint / blockers (FoW when available) |
| Spend | **None at deploy** — already paid at purchase |

Invalid → notify + no inventory consume / no second spend.

---

## Risk Layer

Drop pod **telegraphs** (visible descent, audio, minimap, 2–3 s). Window of vulnerability for opponent / SWARM. Default MVP: pod always lands.

---

## Initial Match State

- **MainBase** pre-deployed.
- **2 Workers** pre-deployed near MainBase (not ordered).
- **0 Orbital Ferronite**.
- All additional content requires orbital cycle.

---

## Core Loop (Expanded)

1. Initial landing (given).
2. Workers mine → containers → Threat ↑.
3. Launch → OrbitalFerronite + FerroniteScore ↑; Threat ↓.
4. **Units:** fill manifest → Confirm → spend → pod → Unit Drop Zone → control units.
5. **Buildings:** Purchase → READY → later ghost deploy → pod → building operational.
6. **Walls:** Buy Wall Package → one rocket to MainBase → Build Wall from inventory (no READY, no per-segment rocket).
7. Defend / expand / score race.

See [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md).

---

## Validation per Pillars

**Pillar 8:** “Ship Ferronite to orbit, spend Orbital Ferronite, drop units at base pad, deploy READY buildings, or land a Wall Package at MainBase and build from stock.”  
**Pillar 1 / 3 / 6:** mining/shipping remains the score and threat loop; orbital is the acquisition sink.

## Open Questions

1. Drop interruption / refund mid-flight (post-MVP).
2. Max simultaneous in-flight pods (recommend soft cap later).
3. Exact Unit Drop Zone component vs scene socket naming.
4. ~~Wall drag-build vs single READY segment UX~~ — **resolved GP-0305R:** package of 5 + inventory Build Wall.
5. Whether unit-cap check also reserves slots during in-flight pods (recommend count at confirm).

## Out of MVP

- Repair modules, upgrade containers, special structures, unit modules.
- Drop cooldowns, intercept abilities, black market.
- Separate unit vs building drop animations (post-MVP art variety).
- Implementing full building roster in the first unit-drop slice.

## References

- Core Loop — [`02_Core_Gameplay_Loop`](02_Core_Gameplay_Loop.md)
- Units — [`04_Units`](04_Units.md)
- Buildings — [`05_Buildings`](05_Buildings.md)
- Resources — [`06_Resources`](06_Resources.md)
- FoW (building deploy / future validation) — [`11_Fog_of_War`](11_Fog_of_War.md)
- Engineering — [`../TDD/14_Orbital_Delivery`](../TDD/14_Orbital_Delivery.md)
- ADR — [`../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)
