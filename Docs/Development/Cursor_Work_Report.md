# Cursor Work Report — GP-0305R Wall Package Delivery Reconciliation

## Status
**GP-0305R_WALL_PACKAGE_RECONCILIATION_READY_FOR_REVIEW**

**NO CODE.**

**NOT MERGED.**

## Branch / base / head
- Branch: `docs/gp-0305r-wall-package-reconciliation`
- Base: `origin/main` @ `5dd56cacdd7b25d4a0d3fc167b69b7a2e3a005a0`
- Factual: **GP-S41M is already on this `main` tip**. Stale “NEXT = GP-S41M / not merged on feature branch” cursor text is reconciled.
- Head: this docs-only commit (see git after push)

## New canonical Wall acquisition / deployment
1. Buy a **Wall Package** from the orbital / order menu (OrbitalFerronite). Not an individually deployable building.
2. Purchase does **not** enter placement.
3. One rocket delivers the package to the player's **MainBase**.
4. Arrival: MainBase Wall inventory = **5**.
5. **Build Wall** (stock > 0) enters drag-placement and consumes already-delivered segments.
6. Placed segments are real `AGP_Wall` (2×2, health, auto-connect, later WallTurret).
7. MainBase destroyed: remaining stored segments are **lost**.

**Rejected:** purchase individual segment → placement → orbital pod **per segment**.

## Package / inventory / depot
- Wall Package = **exactly 5** segments (`UGP_WallPackageDefinition.SegmentCount`).
- MainBase inventory max = **5**. No stacking. Cannot buy while stock ≠ 0 or a package is in flight.
- Depot presentation: visible blocks = remaining stock **0..5**. `WallInventoryChanged(NewCount)`. Meshes are presentation only.

## UI state / action contract
| Condition | Buy Wall Package | Build Wall |
| --- | --- | --- |
| Stock 0, no in-flight | Available | Unavailable |
| Package in flight | Unavailable (pending) | Unavailable |
| Stock 5 | Unavailable (full) | Available |
| Stock 1..4 | Unavailable | Available |
| Stock returns to 0 | Available again | Unavailable |

## Drag-build resource semantics
- Preview length ≤ inventory.
- Confirm: server validates inventory + cells + clearance + existing wall rules; consume **N once**; spawn N `AGP_Wall` **immediately operational**.
- Cancel / failed preview: consume nothing.
- No per-segment rocket. No `PathLength × WallSegmentCost`. No second Orbital spend. No Building READY decrement. No Worker builders.

## DataAsset ownership
- **Package:** new `UGP_WallPackageDefinition` / `DA_GP_WallPackage` — DisplayName, Icon, Cost, SegmentCount=5, delivery timing, `GP.Drop.Type.WallPackage`.
- **Do not** force the package through `UGP_OrbitalDropDefinition` READY-building semantics.
- **Building identity stays** on `DA_GP_Building_Wall` / `UGP_BuildingDefinition` (footprint, UnitDefinition, tags).
- `DA_GP_OrbitalDrop_Wall` superseded for MVP acquisition.
- **Inventory owner:** `UGP_WallSegmentInventoryComponent` on `AGP_MainBase` (replicated 0..5 + pending). Not Ferronite storage. Not READY. Not a generic inventory.

## Implementation slice split (no code in this task)
| Slice | Scope |
| --- | --- |
| **GP-S42A** (next implementation) | Package DA + purchase/delivery + inventory 0..5 + depot seam + Build Wall **availability**. No drag. |
| **GP-S42B** | `AGP_Wall` + connection bitfield. No player drag. |
| **GP-S42C** | Build Wall drag, inventory-limited preview, atomic consume + spawn. |

Old TDD/13 GP-S42 / S45 pod-cascade must **not** be implemented. WallTurret remains later.

## Files changed
Docs only. See git. Includes:
- `Docs/Development/Claude_Tasks/GP-0305R_Wall_Package_Reconciliation.md` (created)
- `Docs/Development/Claude_Tasks/GP-0305_Wall.md` (supersession)
- GDD: `02`, `05`, `06`, `10`, `12`
- TDD: `06`, `09`, `10`, `13`, `14`
- ADR-0009 refinement note
- `DOCUMENTATION_INDEX.md`, `AI_Project_Log.md`, `Claude_Tasks/README.md`, `Claude_Task_Backlog.md`
- this report

## Docs-only validation
- No C++ / Build.cs / config / content / maps / Blueprints / DataAssets in this commit
- No Unreal tests / builds
- Canonical GDD/TDD Wall acquisition is package → inventory → Build Wall
- Next implementation slice is **GP-S42A**, not old GP-S42
- Remote `main` baseline recorded as `5dd56cac…` (GP-S41M already on main)

## Open Questions
None blocking. Package Orbital cost remains TBD balance. Non-blocking: depot mesh layout, later place-animation.

## Pillar / MVP gate
Package delivery preserves orbital logistics, avoids rocket-per-block, keeps walls as prefabricated material, shows stock at MainBase, finite defensive inventory, no Worker builders. Pillar 8 five-question gate **passed** (recorded in GP-0305R).
