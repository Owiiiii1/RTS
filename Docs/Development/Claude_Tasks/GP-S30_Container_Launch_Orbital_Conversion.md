# GP-S30 — Container Launch / Orbital Conversion

## Status
**GP-S30_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

## Slice Group
Slice 8 — Buildings + Orbital Drops (economy unlock vertical; first coding slice after GP-S29R)

## Code Allowed
**YES** — implementation candidate on feature branch (not finalization / not merge).

## Depends On
- `main` @ `89ce3c50ebd05a4bf1e58a5b4e117544dc68cb8f` (post-S29R audit polish)
- Prior candidate: `b865b080be6628d710485d965d28e6c6fa4e205d`
- GP-S28 Storage + Worker haul (merged)
- ADR-0009 Orbital Delivery (Accepted)

## Branch
`feature/gp-s30-container-launch-orbital-conversion`

## Goal
Authority-side MainBase container launch that converts Ready planetary Ferronite into spendable `OrbitalFerronite` + cumulative `FerroniteScore`, and lowers `FerroniteThreatValue`.

## Operator validation (PRIMARY — no console required)

TEMP HUD (`UGP_TEMP_S28P_PlanetaryFerroniteHUD`):

1. **Top counters (same HUD zone):** `Ferronite: <planetary>` and `Orbital: <orbital>`
2. **Bottom-center button:** `Launch Container` (real UButton hit-test)
3. Button → `AGP_PlayerController::RequestLaunchReadyContainer` → `Server_RequestLaunchReadyContainer` → own-team MainBase `TryLaunchReadyContainer`
4. Button enabled only when Ready available and no launch in flight (UI affordance; server revalidates)
5. Root HUD `SelfHitTestInvisible` — empty screen does not block RTS selection; only the button consumes clicks

**Diagnostic fallback only (not operator acceptance):** `gp.Resource.LaunchReadyContainer`

## Delivered (candidate)

### Production launch API
- `UGP_StorageComponent::TryLaunchReadyContainer()` — authority-only Ready→Launching→Empty
- Instant GEs `UGP_GE_AddOrbital` / `UGP_GE_AddScore`; Threat decrease; TeamId ownership

### TEMP HUD / interaction (this follow-up)
- Orbital counter bound to ASC `OrbitalFerronite` attribute change delegate (no Tick)
- Planetary counter unchanged (Storage `GetTotalStored`)
- Launch button → PC Server RPC (WithValidation) → production Storage API
- Does **not** invoke debug console command

### Contracts
| Command | Result |
| --- | --- |
| `gp.Resource.RunContainerLaunchContractTest` | Failures=0 |
| `gp.Resource.RunContainerLaunchHUDContractTest` | Failures=0 |
| `gp.Resource.RunS28RegressionSuite` | Failures=0 |

## Out of Scope (unchanged)
- Order Menu / DropPod / OrbitalDeliverySubsystem / production HUD rewrite / Score on-screen / WBP assets
- GP Dev / Shipping (finalization only)

## Build (candidate)
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **NOT RUN**
- GP Win64 Shipping — **NOT RUN**

## Stop Condition
Operator PIE: fill Ready → click Launch Container → Planetary down / Orbital up / button disable during Launching.
Finalization is a **separate** instruction. **No merge** yet.
