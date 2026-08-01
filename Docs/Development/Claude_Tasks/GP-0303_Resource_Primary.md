# GP-0303 Resource Primary

## Goal

Описати Resource #1 як єдиний MVP resource.

## Inputs

- [`../../GDD/06_Resources.md`](../../GDD/06_Resources.md)
- [`../../TDD/02_GAS_Architecture.md`](../../TDD/02_GAS_Architecture.md)
- [`../../TDD/07_Resource_Architecture.md`](../../TDD/07_Resource_Architecture.md)
- [`../../TDD/10_Data_Assets.md`](../../TDD/10_Data_Assets.md)

## Code Allowed

No.

## Scope

Resource identity, two-state flow (Planetary raw -> Orbital spendable via shipping), UI visibility, GAS Attribute ownership. (Two-state model finalized — see Pivot Note below.)

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Deliverables

- `DA_GP_Resource_Ferronite`.
- Resource attribute ownership: `OrbitalFerronite` (spendable, `COND_OwnerOnly`), `FerroniteScore` (cumulative shipped, `COND_None`), `FerroniteThreatValue` (raw stock at base, drives SWARM).
- Spend rule: OrbitalFerronite spent on orbital drops (no spend-on-build / no spend-on-produce). Raw Planetary Ferronite is NOT spendable until shipped.
- Gain/ship/spend feedback.
- Abuse cases.

## Validation

- One resource only (Ferronite, two states).
- Spend happens only against OrbitalFerronite (post-shipping); raw Planetary Ferronite cannot be spent directly.
- Resource flow (mining, container shipping conversion, score increment, FerroniteThreatValue up/down) is server-authoritative.

## Stop Condition

Зупинитися після resource spec.

## Output

- Design spec: section **"Detailed Resource Primary Rules (GP-0303)"** у [`../../TDD/07_Resource_Architecture.md`](../../TDD/07_Resource_Architecture.md).
- Decisions:
  - Balance values (reserves, mine rate, conversion) — DataAsset-driven, placeholder defaults marked TBD per balance pass (per Data-Driven First).
  - Deposit concurrent workers: **soft cap 4 + FIFO queue**.
  - DepletedBehavior: **Destroy** actor (с 0.5 s VFX delay).
  - `EGP_DepletedBehavior` enum reserved для future `InertHusk` mode.
  - Per-instance `MaxCapacity` override allowed для rich/poor map variants без extra DataAssets.
- DataAsset schemas finalized: `DA_GP_Resource_Ferronite` (type metadata), `DA_GP_Building_FerroniteDeposit` (instance properties).
- Abuse cases table covered (12 vectors з mitigations).
- Code implementation deferred to follow-up task **GP-0303A Resource Implementation** (Code Allowed: Yes).

## Pivot Note (2026-05-16 — Two-State Resource Model)

**Major scope change** per [`../../TDD/07_Resource_Architecture.md`](../../TDD/07_Resource_Architecture.md) §"Container System Update" і memory rule `project_container_system`:

- **Ferronite now has two states:** Planetary (containers at MainBase, NOT spendable) → Orbital (currency after container ship-to-orbit, spendable).
- **Old `Ferronite` player attribute REPLACED** з `OrbitalFerronite` (spendable currency only after shipping).
- **Container System** introduced: `U