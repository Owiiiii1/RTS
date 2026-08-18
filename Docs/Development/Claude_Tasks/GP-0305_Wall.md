# GP-0305 Wall

> **SUPERSEDED IN PART (2026-08-18) by [GP-0305R_Wall_Package_Reconciliation.md](GP-0305R_Wall_Package_Reconciliation.md).**
>
> Still valid: Wall as 2×2 `AGP_Wall`, 8-dir auto-connect, clearance, Wall-mounted Turret, BuildGrid occupancy.
>
> **Rejected / do not implement:** purchase individual Wall segment → READY → drag → `PathLength × WallSegmentCost` → orbital pod **per segment**.
>
> **Canon:** buy a **Wall Package of 5** → one rocket to MainBase → inventory 0..5 → **Build Wall** consumes stock (no second Orbital spend, no READY, no per-segment rocket).
>
> Next implementation: **GP-S42A**, not the old GP-S42 pod-per-segment slice.

## Goal

Описати Wall будівлю + Wall-mounted Turret variant + Build Grid System як defensive perimeter mechanic.

## Inputs

- [`GP-0305R_Wall_Package_Reconciliation.md`](GP-0305R_Wall_Package_Reconciliation.md) — **acquisition / inventory / Build Wall (canon)**.
- [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md) §Wall, §Build Grid System.
- [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §Build Grid System, §Wall System.
- [`../../GDD/10_Orbital_Delivery.md`](../../GDD/10_Orbital_Delivery.md).
- [`../../TDD/14_Orbital_Delivery.md`](../../TDD/14_Orbital_Delivery.md).

## Code Allowed

No.

## Scope

Wall building identity, drag-build mechanic, 8-direction auto-connect, clearance rule (2 cells), wall-mounted turret variant, grid integration. Не реалізовувати код.

Acquisition/deployment in this file’s original “drop cascade” wording is **historical**. Use GP-0305R + current GDD/TDD.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Збудувати defensive perimeter навколо economy, що замикає choke points і дозволяє mount turrets для passive defense. Wall — це **інженерне рішення** проти organic SWARM threat, а не армія.

## Deliverables (building identity — still valid)

- `DA_GP_Building_Wall` schema (footprint 2×2, clearance behavior).
- `DA_GP_Building_WallTurret` schema (footprint 2×2, mount constraint, stat trade-offs).
- 8-direction auto-connect visual rules (16 mesh states cover all combinations).
- Clearance rule: 2 cells from any non-wall structure.
- Wall-mounted turret placement rules (must be on existing wall cell, one-per-wall).
- Grid integration: `UGP_BuildGridSubsystem` query + occupancy.

Package size, cost, MainBase inventory, and Build Wall consumption: **GP-0305R**.

## Validation

- Wall snaps to grid cells, не free-form.
- Wall auto-connects to adjacent walls without explicit player action.
- Wall cannot be placed within 2 cells of other structures.
- Wall-mounted Turret cannot exist without wall.
- Wall destroyed → mounted turret cascades destroyed.
- Server-authoritative pathfinding (no client-side decisions).
- Soft refs only для content (mesh, material, sound bundles).
- Placement consumes MainBase Wall inventory, not OrbitalFerronite / READY / per-segment pods.

## Stop Condition

Зупинитися після Wall spec. Implementation is GP-S42A → S42B → S42C (see GP-0305R). Historical decisions below that mention pod cascade / per-segment Orbital cost are **superseded**.

## Output (historical — filter through GP-0305R)

- Engineering deep-spec: [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §Wall System.
- Gameplay spec: [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md) §Wall.
- Still valid decisions: footprint 2×2; 8-dir bitfield; A* on BuildGrid; clearance 2 from non-wall; WallTurret separate DA; cascade destroy.
- Classes: `AGP_Wall`, `AGP_WallTurret`, `UGP_WallConnectionComponent`, `UGP_BuildGridSubsystem`, `AGP_GhostWallSegment`, plus GP-0305R `UGP_WallPackageDefinition` + `UGP_WallSegmentInventoryComponent`.
- Tags: `GP.Building.Type.Wall`, `GP.Building.Type.WallTurret`, `GP.Capability.WallMountable`, `GP.Capability.HostsWallMount`, `GP.Drop.Type.WallPackage`.
- Implementation: **GP-S42A / S42B / S42C**; WallTurret later. Old TDD/13 S42+S45 pod-cascade slices are superseded.
