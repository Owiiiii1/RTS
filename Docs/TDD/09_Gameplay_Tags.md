# Gameplay Tags

## Scope

Цей документ — **single authoritative tag registry** для GrimProtocol MVP. Усі `GP.*` tags, що використовуються у gameplay/UI/AI коді, мають бути присутні тут. Реєстрація — нативна, через `FGPGameplayTags` (per [`Claude_Tasks/GP-S02_Native_Gameplay_Tags`](../Development/Claude_Tasks/GP-S02_Native_Gameplay_Tags.md)).

## Mandatory Rules

- Root namespace: `GP`.
- Native tags реєструються у `GPGASRuntime` через `FGPGameplayTags` struct (`AddNativeGameplayTag` у module startup), accessor `FGPGameplayTags::Get()`.
- Runtime code не використовує magic-string tags.
- Новий tag додається разом з описом owner system і use case.
- Tags описують state/capability/identity, а не замінюють Data Assets.
- Tags нижче у "Deprecated / Pre-Pivot Tags" — **не використовувати у new code**. Вони пережиток pre-orbital-pivot (local production/construction) моделі або rejected aliases.

### Tech-lead lock (2026-08-01, GP-S02)

- Teams: `GP.Team.Player.One` / `GP.Team.Player.Two` — **not** `GP.Team.A` / `GP.Team.B`.
- Commands include `GP.Command.AttackMove` plus Sell / Demolish / CancelOrder.
- Unit identity includes `GP.Unit.Type.SalvageWalker`.
- Notify: `GP.Notify.InsufficientOrbitalFerronite` — **not** `GP.Notify.InsufficientOrbital`.
- [`13_Architecture_Proposal`](13_Architecture_Proposal.md) §Gameplay Tags must stay synchronized with this file.

## Baseline Taxonomy (Active MVP)

```text
# Match flow / state
GP.Match.State.Loading
GP.Match.State.WaitingForPlayers
GP.Match.State.Playing
GP.Match.State.Paused
GP.Match.State.Finished

# Match outcome reasons (win/lose attribution — per GDD/08_Win_Lose_Conditions)
GP.Match.WinReason.DeliveryQuota
GP.Match.WinReason.TimerScore
GP.Match.WinReason.Annihilation
GP.Match.WinReason.OpponentDisconnect

# Unit identity
GP.Unit.Type.Worker
GP.Unit.Type.SalvageWalker
GP.Unit.Type.Combat
GP.Unit.Type.Support
GP.Unit.Type.Building

# Unit runtime state
GP.Unit.State.Moving
GP.Unit.State.Mining
GP.Unit.State.Repairing
GP.Unit.State.Attacking
GP.Unit.State.AttackCooldown
GP.Unit.State.Dead
GP.Unit.State.Stunned

# Building identity (orbital-delivered + map-placed)
GP.Building.Type.MainBase
GP.Building.Type.LogisticsHub
GP.Building.Type.DefensiveTurret
GP.Building.Type.Wall
GP.Building.Type.WallTurret
GP.Building.Type.FerroniteDeposit

# Building role classification
GP.Building.Role.Command
GP.Building.Role.Logistics
GP.Building.Role.Defense
GP.Building.Role.Resource

# Commands (UI buttons + command validation)
GP.Command.Move
GP.Command.Stop
GP.Command.Attack
GP.Command.AttackMove      # A-move — ACTIVE у MVP (TDD/04 / GP-0204)
GP.Command.Mine
GP.Command.Repair          # Worker repair — ACTIVE у MVP
GP.Command.Sell            # building sold for partial OrbitalFerronite refund
GP.Command.Demolish        # wall demolished permanently (no refund)
GP.Command.OrderDrop       # order an orbital drop via Logistics Hub / Order Menu
GP.Command.CancelOrder     # cancel a pending/in-flight orbital order

# Orbital delivery classification (UGP_OrbitalDropDefinition.DropTags)
GP.Drop.Type.Unit
GP.Drop.Type.Building
GP.Drop.Type.WallPackage  # Wall Package to MainBase (not READY / not per-segment)
GP.Drop.Type.Wall         # superseded per-segment classification (do not use for purchase)
GP.Drop.Type.Module        # reserved payload kind; tag registered for DA classification

# Orbital pod state
GP.State.PodInFlight       # loose tag on AGP_DropPod while descending

# Resource identity
GP.Resource.Type.Ferronite
GP.Resource.Node

# Abilities / ability state
GP.Ability.Repair
GP.Ability.State.Channeling

# Capabilities / selection
GP.Capability.Selectable
GP.Capability.Inspectable
GP.Selection.Type.Unit
GP.Selection.Type.Building

# Faction
GP.Faction.Corporate

# Teams
GP.Team.Neutral
GP.Team.Player.One
GP.Team.Player.Two

# Effect sources
GP.Effect.Source.UnitCapBuilding

# HUD / client notifications
GP.Notify.InsufficientOrbitalFerronite
GP.Notify.UnitCapReached
GP.Notify.WorkerIdle
GP.Notify.BaseUnderAttack
GP.Notify.DropRejected
GP.Notify.MatchEndingSoon
GP.Notify.CommandRejected
```

## Deprecated / Pre-Pivot Tags — do not use in new code

Ці tags належать pre-pivot local-production / local-construction моделі або rejected aliases. Orbital Delivery (per [`14_Orbital_Delivery`](14_Orbital_Delivery.md), [`ADR_0009_Orbital_Delivery_Pillar`](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)) видалив локальне виробництво/будівництво. Не реєструвати, не посилатися у new code. Залишені тут для traceability під час cleanup.

```text
# Local-build / production commands — replaced by GP.Command.OrderDrop (orbital)
GP.Command.Build               -> use GP.Command.OrderDrop
GP.Command.QueueProduction     -> removed (no local production queue)
GP.Command.CancelProduction    -> use GP.Command.CancelOrder
GP.Command.SetRallyPoint       -> removed (no production rally)

# Production buildings — removed from roster (no local fabrication)
GP.Building.Type.Barracks      -> removed
GP.Building.Type.AssemblyYard  -> removed

# Renamed resource tag
GP.Resource.Primary            -> renamed to GP.Resource.Type.Ferronite

# Rejected team aliases (GP-S02 tech-lead lock)
GP.Team.A                      -> use GP.Team.Player.One
GP.Team.B                      -> use GP.Team.Player.Two

# Rejected notify alias (GP-S02 tech-lead lock)
GP.Notify.InsufficientOrbital  -> use GP.Notify.InsufficientOrbitalFerronite
```

`GP.Resource.Primary` — старий generic-name для primary resource. **Reconciled / renamed** to `GP.Resource.Type.Ferronite` (explicit resource identity). Будь-який код/DA, що посилається на `GP.Resource.Primary`, мігрує на `GP.Resource.Type.Ferronite`.

## Ownership

| Tag Area | Owner Module | Notes |
| --- | --- | --- |
| `GP.Match.State.*` | `GPRuntime` | Match flow reads tags; registration lives in `GPGASRuntime` native tag registry. |
| `GP.Match.WinReason.*` | `GPRuntime` | Win/lose attribution (per `GDD/08_Win_Lose_Conditions`). |
| `GP.Unit.*` | `GPRuntime` | Unit Data Assets grant identity/capability tags. |
| `GP.Building.*` | `GPRuntime` | Building Data Assets grant identity tags. |
| `GP.Command.*` | `GPRuntime` | Command validation and UI command buttons reference these tags. |
| `GP.Drop.*` | `GPRuntime` | `UGP_OrbitalDropDefinition.DropTags` classify orbital payloads. |
| `GP.State.*` | `GPRuntime` | Loose actor state (e.g., `PodInFlight` on `AGP_DropPod`). |
| `GP.Resource.*` | `GPRuntime` + `GPGASRuntime` | Data Assets define resource identity; Attributes hold runtime quantities. |
| `GP.Ability.*` | `GPGASRuntime` | GAS ability activation and costs. |
| `GP.Capability.*` / `GP.Selection.*` | `GPRuntime` | Selection / inspect capability identity. |
| `GP.Faction.*` / `GP.Team.*` | `GPRuntime` | Faction / team identity. |
| `GP.Effect.Source.*` | `GPGASRuntime` | Effect provenance tags. |
| `GP.Notify.*` | `GPUIRuntime` (consumes) / `GPRuntime` (emits) | Client HUD notifications. |

## First Playable Mapping

- Selection does not require replicated tags in MVP; it is client-local.
- Move command uses `GP.Command.Move`.
- Attack command uses `GP.Command.Attack`; attack-move uses `GP.Command.AttackMove`.
- Worker repair command uses `GP.Command.Repair` (ACTIVE у MVP).
- Order an asset from orbit uses `GP.Command.OrderDrop`; cancel uses `GP.Command.CancelOrder`.
- Worker uses `GP.Unit.Type.Worker`; Salvage Walker uses `GP.Unit.Type.SalvageWalker`.
- Logistics Hub (order surface) uses `GP.Building.Type.LogisticsHub`.
- Drop pod in flight carries `GP.State.PodInFlight`; payload classified via `GP.Drop.Type.*`.
- Primary resource uses `GP.Resource.Type.Ferronite` (renamed from deprecated `GP.Resource.Primary`).
- Teams use `GP.Team.Player.One` / `GP.Team.Player.Two` (not A/B).

## References

- Gameplay design — [`../GDD/02_Core_Gameplay_Loop.md`](../GDD/02_Core_Gameplay_Loop.md).
- GAS architecture — [`02_GAS_Architecture.md`](02_GAS_Architecture.md).
- Data Assets — [`10_Data_Assets.md`](10_Data_Assets.md).
- Architecture proposal tag section — [`13_Architecture_Proposal.md`](13_Architecture_Proposal.md) (must mirror this file).
