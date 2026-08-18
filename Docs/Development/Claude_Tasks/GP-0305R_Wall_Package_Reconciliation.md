# GP-0305R — Wall Package Delivery Reconciliation

## Status
**GP-0305R_WALL_PACKAGE_RECONCILIATION_READY_FOR_REVIEW**

**NOT MERGED.**

**Code Allowed: NO**

## Slice Group
Design / reconciliation — Wall acquisition + deployment

## Branch
`docs/gp-0305r-wall-package-reconciliation`  
Base: `origin/main` @ `5dd56cacdd7b25d4a0d3fc167b69b7a2e3a005a0`  
(`GP-S41M` is already on this `main` tip.)

## Owner decision
The old Wall flow is **rejected**:

Purchase individual Wall segment → placement → orbital pod/rocket **per segment** → segment lands.

## New canonical MVP flow

1. Wall is purchased as a **package**, not as an individually deployable building.
2. One Wall Package contains exactly **5 Wall Segments**.
3. Purchased from the orbital / order menu for **OrbitalFerronite**.
4. Purchase does **not** enter building-placement mode.
5. Purchase starts orbital delivery to the player's **MainBase**.
6. **One** rocket/drop carries the entire package of 5.
7. On successful arrival: MainBase Wall inventory becomes **5** available segments.
8. MVP MainBase Wall inventory capacity: **maximum 5**.
   - Buy Wall Package is allowed at stock **0..4** (not while pending or at 5).
   - Price is always the full `PackageDefinition.Cost`. Excess segments on arrival are wasted (no refund, no prorating).
   - No inventory above 5.
9. MainBase has a presentation-only Wall Depot. Visible blocks = actual remaining inventory (0..5). Gameplay stock is authoritative; meshes are never gameplay state.
10. When inventory > 0, player gets **Build Wall**.
11. Build Wall is **not** an orbital purchase. It enters wall drag-placement.
12. Drag consumes already-delivered segments (example: stock 5, confirm path of 3 → place 3 `AGP_Wall`, stock = 2, depot shows 2).
13. Preview length is limited by actual inventory.
14. Building Wall after delivery does **not** spend OrbitalFerronite again, summon another rocket, or consume Building READY inventory.
15. After placement, Wall remains a real `AGP_Wall` (2×2, health/destruction, auto-connect, later WallTurret). Only acquisition/deployment is special.

## DataAsset ownership

**Do not** put Wall Package through `UGP_OrbitalDropDefinition` READY-building semantics. That type means: spend → READY++ → later ghost deploy of **one** building via DropPod to a grid cell. A Wall Package spends once, delivers **five** segments to MainBase inventory, and never enters placement on purchase.

**Owner:** new `UGP_WallPackageDefinition` (`UPrimaryDataAsset`) in the orbital definition family (same catalog/soft-ref pattern as drop defs).

Proposed fields:

| Field | Role |
| --- | --- |
| DisplayName | Order Menu label |
| Icon | Soft texture |
| Cost | OrbitalFerronite; GAS spend on accepted purchase |
| SegmentCount | **5** (MVP; data-driven, not a random actor constant) |
| Descent / deploy delay | Soft / per-purchase timing, same pattern as other orbital defs |
| DropTags | `GP.Drop.Type.WallPackage` |
| Delivery presentation | Soft rocket/DropPod presentation refs if needed |

Content name: `DA_GP_WallPackage` (native catalog identity is enough until an authored `.uasset` exists).

Settings may hold a **soft ref** to the authored package DA (same seam as Hub/Turret drop defs). Cost/SegmentCount live **on the package DA**, not on `UGP_OrbitalDeliverySettings` and not on `AGP_MainBase`.

`DA_GP_Building_Wall` / `UGP_BuildingDefinition` still own Wall **building** identity: footprint 2×2, UnitDefinition, tags, clearance/host-mount flags, sell/demolish. Combat/building identity does **not** move into package data.

`UGP_OrbitalDeliverySubsystem` owns Wall **Package** delivery (one DropPod to MainBase). It does **not** own per-segment `AGP_Wall` placement. GP-S42C owns/routes surface placement through wall / BuildGrid authority.

`DA_GP_OrbitalDrop_Wall` (READY + per-segment drop) is **superseded** for MVP acquisition. `DA_GP_OrbitalDrop_WallTurret` stays a normal READY building drop (later slice).

Faction catalog: replace `DA_GP_OrbitalDrop_Wall` in the orderable list with `DA_GP_WallPackage`. `DA_GP_Building_Wall` remains in AllowedBuildings.

## MainBase Wall inventory

**Owner:** `UGP_WallSegmentInventoryComponent` on `AGP_MainBase`.

Not Ferronite storage. Not Building READY. Not a generic inventory framework.

| Fact | Rule |
| --- | --- |
| Authority | Server |
| Replication | Owner-readable (UI / depot) |
| Count | `int32` **0..5** |
| Capacity | 5, matching package `SegmentCount` |
| Delivery pending | Replicated bool; blocks repurchase while in flight |
| Arrival | `Accepted = min(SegmentCount, Capacity - stock)` using **actual stock at arrival**. Always clears pending. Excess wasted. |
| Placement | Atomic consume of confirmed path length N |
| Cancel / failed preview | Consumes **nothing** |
| UI / meshes | Read this count only; no duplicated SoT |

**MainBase destroyed:** remaining stored Wall Segments are **lost** with the MainBase. No refund, no drop, no transfer.

## Wall Depot presentation

Designer-connectable seam on `BP_GP_MainBase`. No mesh assets assumed in code.

Contract: `WallInventoryChanged(NewCount)` (0..5).

BP may toggle five pre-authored mesh slots or any other simple presentation. Gameplay must not inspect those meshes. Do **not** spawn a gameplay actor per stored block.

## UI state / action contract

No final visual layout.

| Condition | Buy Wall Package | Build Wall |
| --- | --- | --- |
| Stock 0, no in-flight package | Available | Unavailable |
| Package in flight | Unavailable (pending) | Unavailable |
| Stock 5 (just delivered / full) | Unavailable (full / no stack) | Available |
| Stock 1..4 | Available (full package price) | Available |
| Stock returns to 0 | Available again | Unavailable |

Purchase never enters placement. Build Wall never spends OrbitalFerronite.

## Drag-build (updated)

**Wrong (removed):** Order Menu Wall → drag → `PathLength × WallSegmentCost` → drop pod per segment.

**Correct:**

Build Wall → drag start → live grid path preview → path length ≤ inventory → release/confirm → server validates inventory, cells, clearance, existing wall rules → consume N **once** → spawn N `AGP_Wall` → auto-connect → inventory = Remaining − N.

No per-segment orbital drop. No per-segment Orbital spend. No READY decrement.

**MVP placement:** segments become **operational immediately** after confirmed placement. Cosmetic place animation may come later. No Worker builders.

Failed/cancelled preview: no consume, no spawn.

## Why package delivery (pillars)

- Preserves orbital logistics fantasy (Pillar 1 / ADR-0009).
- Avoids rocket-per-block absurdity (Pillar 7 readability, Pillar 2 engineer not soldier).
- Walls read as pre-fabricated engineering material sitting at MainBase.
- Visible physical logistics at the depot (stock 0..5).
- Finite defensive inventory is a real decision (Pillar 4 capacity-as-strategy).
- No local Worker construction (ADR-0009).

### Pillar 8 — 5-question MVP gate

1. Fun now? Yes — buy a crate, see five blocks at base, drag a short perimeter from stock.
2. Clear to a new player? “Buy a wall package; it lands at your base; then build from those blocks.”
3. New decision? Yes — spend Orbital on a finite 5-segment stock vs Hub/Turret/units; then where to spend those five cells.
4. Cheap to implement? Yes — three small slices (data+inventory, wall actor, drag consume).
5. Scales via content? Package Cost / SegmentCount / timing stay on the DataAsset; later packages or larger capacity are content, not new rules.

Gate **passed**.

## Implementation plan (no code in this task)

| Slice | Scope | Operator-testable without the next slice |
| --- | --- | --- |
| **GP-S42A** | `UGP_WallPackageDefinition`, purchase + one rocket to MainBase, inventory 0..5, pending flag, depot `WallInventoryChanged`, Build Wall **availability only** | Buy package → stock 5 + depot; cannot rebuy; no drag yet |
| **GP-S42B** | `AGP_Wall` + `UGP_WallConnectionComponent`, 2×2, 8-dir bitfield, local neighbor refresh | Spawn/place walls in a contract or debug path; connect visuals; **no** player drag |
| **GP-S42C** | Build Wall mode, preview, server validation, inventory-limited length, atomic consume + spawn, cancel = no consume | Full MVP wall build from stock |

WallTurret remains a **later** slice (old GP-S43 intent). Not GP-S42.

Old TDD/13 **GP-S42 / S44 / S45** “pod cascade per segment” is superseded. Next implementation slice is **GP-S42A**, not old GP-S42.

## Open Questions

None blocking. Package Orbital cost remains TBD balance (existing pattern). Non-blocking: depot mesh layout, later place-animation.

## Supersedes

[`GP-0305_Wall.md`](GP-0305_Wall.md) — keep as historical spec; acquisition/deployment flow is this document + updated GDD/TDD.
