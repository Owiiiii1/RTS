# GP-S30 — Container Launch / Orbital Conversion

## Status
**GP-S30_FINALIZATION_READY_FOR_MERGE_REVIEW**

## Slice Group
Slice 8 — Buildings + Orbital Drops (economy unlock vertical; first coding slice after GP-S29R)

## Code Allowed
**YES** — finalization complete on feature branch (not yet merged).

## Depends On
- `main` @ `89ce3c50ebd05a4bf1e58a5b4e117544dc68cb8f`
- GP-S28 Storage + Worker haul (merged)
- ADR-0009 Orbital Delivery (Accepted)

## Branch
`feature/gp-s30-container-launch-orbital-conversion`

## Goal
Authority-side MainBase container launch that converts Ready planetary Ferronite into spendable `OrbitalFerronite` + cumulative `FerroniteScore`, and lowers `FerroniteThreatValue`.

## Operator validation — PASS

Operator PIE confirmed:

- HUD restored after RebuildWidget lifecycle fix
- Ferronite + Orbital counters visible
- Launch Container button visible
- Workers mine/haul successfully
- Launch enabled when container Ready
- Button launch succeeds (no console)
- After telegraph: Planetary decreases; Orbital increases; HUD updates

**Note:** HUD Ferronite is **total stored across containers**, not one-container capacity. Default: capacity **100** × count **5** = total **500**.

## Delivered

### Production flow
Worker → mine → haul → Storage → Ready → Launch Container → Launching → completion → Empty; OrbitalFerronite + FerroniteScore via Instant GEs; Threat decreases.

### TEMP HUD
- Top: `Ferronite` + `Orbital`
- Bottom: `Launch Container` UButton
- Root `SelfHitTestInvisible`; button Visible
- WidgetTree built in `RebuildWidget` (lifecycle fix)

### Interaction
HUD → `RequestLaunchReadyContainer` → `Server_RequestLaunchReadyContainer` → own-team `TryLaunchReadyContainer`

### Storage defaults
| Knob | Value |
| --- | --- |
| ContainerCapacity | 100 |
| ContainerCount | 5 |
| Total default | 500 |
| Orbital/Score rates | 1:1 |
| Launch duration | 2.5 s |

## Contracts (finalization)
| Command | Result |
| --- | --- |
| `gp.Resource.RunContainerLaunchHUDContractTest` | Failures=0 |
| `gp.Resource.RunContainerLaunchContractTest` | Failures=0 |
| `gp.Resource.RunS28RegressionSuite` | Failures=0 |
| `gp.Combat.RunLOSFireGateContractTest` | Failures=0 |
| `gp.Combat.RunSalvageWalkerContractTest` | Failures=0 |
| `gp.Combat.RunHealthBarContractTest` | Failures=0 |
| `gp.Combat.RunTeamColorContractTest` | Failures=0 |

## Builds (finalization)
- GPEditor Win64 Development + UHT — **PASS**
- GP Win64 Development — **PASS**
- GP Win64 Shipping — **PASS** (see Cursor_Work_Report)

## Stop Condition
Merge/close when operator requests. Do **not** auto-start GP-S31.
