# GP-S30 — Container Launch / Orbital Conversion

## Status
**GP-S30_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

## Slice Group
Slice 8 — Buildings + Orbital Drops (economy unlock vertical; first coding slice after GP-S29R)

## Code Allowed
**YES** — implementation candidate on feature branch (not finalization / not merge).

## Depends On
- `main` @ `89ce3c50ebd05a4bf1e58a5b4e117544dc68cb8f` (post-S29R audit polish)
- GP-S28 Storage + Worker haul (merged)
- ADR-0009 Orbital Delivery (Accepted)
- Audit: [`../Next_Slice_Audit_Post_S29R.md`](../Next_Slice_Audit_Post_S29R.md)

## Branch
`feature/gp-s30-container-launch-orbital-conversion`

## Goal
Authority-side MainBase container launch that converts Ready planetary Ferronite into spendable `OrbitalFerronite` + cumulative `FerroniteScore`, and lowers `FerroniteThreatValue`.

## Delivered (candidate)

### Production launch API
- `UGP_StorageComponent::TryLaunchReadyContainer()` — authority-only
- Deterministic first Ready-by-index selection
- Lifecycle: Ready → Launching (timer telegraph) → Empty
- During Launching: no fill into that slot; second launch rejected (`LaunchInFlight`)
- FIFO fill policy preserved for non-Launching containers

### GAS Instant GEs (created — verified absent pre-impl)
- `UGP_GE_AddOrbital` — Instant Additive `OrbitalFerronite` SetByCaller `GP.Launch.OrbitalMagnitude`
- `UGP_GE_AddScore` — Instant Additive `FerroniteScore` SetByCaller `GP.Launch.ScoreMagnitude`
- No direct attribute Set/Add from Storage

### Data-driven rates (canonical GDD MVP 1:1)
- `UGP_ResourceDefinition::OrbitalConversionRate` default **1.0**
- `UGP_ResourceDefinition::ScoreConversionRate` default **1.0** (already existed)
- `UGP_ResourceGameplaySettings::ContainerLaunchDurationSeconds` default **2.5** (`DefaultGame.ini`)

### Ownership
- MainBase `TeamId` → first matching `AGP_PlayerState` in `GameState->PlayerArray`
- Missing owner / ASC: reject before accept; at completion fail-safe restore Ready (no empty, no reward)

### Threat
- On completion: `AddFerroniteThreatValueForTeam(TeamId, -(amount * ThreatPerStoredUnit))` (clamp ≥ 0 via GameState)

### Operator trigger
- `gp.Resource.LaunchReadyContainer [NameSubstring]` → production `TryLaunchReadyContainer`

### Contract
- `gp.Resource.RunContainerLaunchContractTest` → Complete Failures=0
- `gp.Resource.RunS28RegressionSuite` → Complete Failures=0 (S30 not embedded in suite)

## Out of Scope (unchanged)
- OrbitalDeliverySubsystem / DropPod / Order Menu / Logistics Hub / walls / turrets
- Pathfinding / Targeting / AttackMove / CombatComponent
- SWARM / win condition / FoW / Steam
- GP Dev / Shipping builds (finalization only)
- Operator-local Content / DefaultEngine.ini

## Build (candidate)
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **NOT RUN**
- GP Win64 Shipping — **NOT RUN**

## Stop Condition
Operator validates launch in PIE. Finalization (Dev+Shipping + merge docs) is a **separate** instruction.
Do not merge this branch without operator PASS + finalization pass.
