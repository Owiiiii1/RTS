# GP-0307 Sell + Demolish

## Goal

Описати building Sell (partial Orbital Ferronite refund) і Wall Demolish (permanent, no refund) як два distinct lifecycle-end operations.

## Inputs

- [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md) §Sell + Demolish.
- [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §Sell + Demolish System.
- Memory rule `project_sell_demolish`.

## Code Allowed

No.

## Scope

Sell (buildings), Demolish (walls), refund formula, UI surface (Sell button, Demolish cursor mode), server validation, anti-exploit guards. Не реалізовувати код.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Player Goal

Player wants to:
- Recover Orbital Ferronite from misplaced або no-longer-needed buildings (Sell).
- Clear wall perimeters strategically without economic punishment (Demolish — but no refund forces commitment).

Both operations remove building/wall з map. Differ economically і у UX entry point.

## Deliverables

- `bSellable` + `SellRefundRate` fields у `UGP_BuildingDefinition` (DA-driven).
- Per-building sellability table (Main Base / Logistics Hub / Defensive Turret / Wall-mounted Turret / Wall / Deposit).
- Refund formula: `Cost × SellRefundRate × HealthFraction`.
- Server validation rules (ownership, alive state, anti-spam debounce).
- Wall demolish cursor mode (single-click + drag-batch).
- Cascade rules: wall demolish → wall-mounted Turret cascade destroy (no refund per cascade).
- UI: Sell button on SelectionPanel, Demolish button на CommandBar, demolish cursor + banner.
- New tags: `GP.Command.Sell`, `GP.Command.Demolish`, `GP.UI.Mode.Demolish`, `GP.Building.Capability.Sellable`, `GP.Notify.BuildingSold`, `GP.Notify.WallDemolished`.
- New RPC list: `Server_SellBuilding`, `Server_DemolishWalls`.

## Validation

- Walls **never** sellable (anti-exploit — no scout-by-cancel-drag, no defensive flexibility abuse).
- Main Base **never** sellable (loss = annihilation per ADR-0009).
- Refund formula damages-aware (anti-exploit: damage-then-sell-undamaged buildings).
- Refund clamped: never > Cost (validate `SellRefundRate <= 1.0` у `IsDataValid`).
- Server-authoritative refund computation (client only previews).
- Wall demolish cascades wall-mounted Turret correctly without separate RPC.
- Anti-spam: 50 ms server debounce per caller for Sell RPCs.

## Stop Condition

Зупинитися після Sell+Demolish spec.

## Output

- Gameplay design — [`../../GDD/05_Buildings.md`](../../GDD/05_Buildings.md) §Sell + Demolish.
- Engineering spec — [`../../TDD/06_Building_Architecture.md`](../../TDD/06_Building_Architecture.md) §Sell + Demolish System (GP-0307) + Feel / MVP Fun Maximization subsection.
- Decisions:
  - Refund formula: `Cost × SellRefundRate × HealthFraction` (DA-tunable per building, placeholder 0.5 base).
  - Walls **never** sellable — permanent demolish only (anti-exploit rationale documented).
  - Wall-mounted Turret sellable separately; wall underneath untouched.
  - Demolish UX: dedicated cursor mode (red demolition icon, persistent HUD banner, drag-batch supported).
  - Sell UX: button on SelectionPanel, hover preview.
  - Cascade: wall demolish → mounted turret destroyed automatically (no separate refund — already used при initial mount; cascade destruction does not refund).
  - Cap effects (Logistics Hub +5 MaxUnits + container bonus) removed on sell via `RemoveActiveGameplayEffectBySourceEffect`.
- 14 playtest scenarios documented у TDD/06 §Sell + Demolish System.
- Code implementation deferred to follow-up task **GP-0307A** — likely added до Slice 8 (Buildings + Orbital Drops + Wall + Grid) у TDD/13 Architecture Proposal Implementation Order.
