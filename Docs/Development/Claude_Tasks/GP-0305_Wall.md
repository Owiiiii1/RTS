# GP-0305 Wall

## Goal

Описати Wall будівлю + Wall-mounted Turret variant + Build Grid System як defensive perimeter mechanic.

## Inputs

- [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md) §Wall, §Build Grid System.
- [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §Build Grid System, §Wall System.
- [`../../GDD/10_Orbital_Delivery.md`](../../GDD/10_Orbital_Delivery.md).
- [`../../TDD/14_Orbital_Delivery.md`](../../TDD/14_Orbital_Delivery.md).
- Memory rule `project_build_grid_and_walls`.

## Code Allowed

No.

## Scope

Wall building identity, drag-build mechanic, 8-direction auto-connect, clearance rule (2 cells), wall-mounted turret variant, grid integration. Не реалізовувати код.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Збудувати defensive perimeter навколо economy, що замикає choke points і дозволяє mount turrets для passive defense. Wall — це **інженерне рішення** проти organic SWARM threat, а не армія.

## Deliverables

- `DA_GP_Building_Wall` schema (footprint 2×2, clearance behavior, drag-build flag).
- `DA_GP_Building_WallTurret` schema (footprint 2×2, mount constraint, stat trade-offs).
- 8-direction auto-connect visual rules (16 mesh states cover all combinations).
- Drag-build flow (start cell → A* path → drop cascade).
- Clearance rule: 2 cells from any structure (excluding wall-to-wall).
- Wall-mounted turret placement rules (must be on existing wall cell, one-per-wall).
- Grid integration: `UGP_BuildGridSubsystem` query + occupancy.

## Validation

- Wall snaps to grid cells, не free-form.
- Wall auto-connects to adjacent walls without explicit player action.
- Wall cannot be placed within 2 cells of other structures.
- Wall-mounted Turret cannot exist without wall.
- Wall destroyed → mounted turret cascades destroyed.
- Server-authoritative pathfinding (no client-side decisions).
- Soft refs only для content (mesh, material, sound bundles).

## Stop Condition

Зупинитися після Wall spec.

## Output

- Engineering deep-spec lives у [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §Build Grid System + §Wall System (GP-0305).
- Gameplay spec у [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md) §Wall + §Wall-mounted Turret + §Build Grid System.
- Decisions:
  - Footprint Wall = 2×2; Wall-mounted Turret = 2×2; free-standing Defensive Turret = 4×4 (SC2-calibrated sizes).
  - Auto-connect 8-direction bitfield → mesh-swap (16 visual states cover all cases).
  - Drag-build via server-side A* pathfinding (`UGP_BuildGridSubsystem::PathfindFreeCells`).
  - Clearance rule: 2 cells from non-wall structures; walls touch each other freely.
  - Single-click placement OR drag-build — both supported.
  - Wall-mounted Turret: separate DA, 2×2 footprint, requires wall under, lower stats than free-standing.
  - Cascade destroy: wall destroyed → mounted turret destroyed.
- New classes added до TDD/13 Architecture Proposal class list: `AGP_Wall`, `AGP_WallTurret`, `UGP_WallConnectionComponent`, `UGP_BuildGridSubsystem`, `AGP_GhostWallSegment`.
- New tags reserved: `GP.Building.Type.Wall`, `GP.Building.Type.WallTurret`, `GP.Capability.WallMountable`, `GP.Capability.HostsWallMount`, `GP.Drop.Type.Wall`.
- 15 playtest scenarios documented у TDD/06 §Wall System.
- Implementation slices у TDD/13 Slice 8 (GP-S35 grid subsystem, GP-S42 Wall + ConnectionComponent, GP-S43 WallTurret, GP-S45 drag-build flow).
- Code implementation deferred to follow-up tasks (multi-slice, per Slice 8 of TDD/13).
