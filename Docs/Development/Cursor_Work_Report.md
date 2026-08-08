# Cursor Work Report — GP-S31R Landing Z + Deployment Phase

## Status
**GP-S31R_LANDING_AND_DEPLOYMENT_PRESENTATION_READY_FOR_OPERATOR_RETEST**

NOT MERGED. NOT FINALIZED.

---

## Operator context

Authored BP payload/DropPod **PASS**. Gameplay mechanics **PASS**. Remaining: half-buried spawn + missing deploy presentation window.

## Issue A — ground placement root cause

Capsule root → actor origin = capsule center. Spawn at ground Z buried the lower half.

## Capsule solution

`GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass(UClass*)` reads CDO gameplay capsule (`GetScaledCapsuleHalfHeight`). Spawn:

`Z = GroundZ + OffsetZ`

Uses collision capsule, not mesh bounds. Works for native + authored Worker/SW subclasses.

## Issue B — lifecycle before/after

**Before:** Impact → immediate payload → cleanup  

**After:** Descending → Impact/Deploying → (PayloadDeployDelay) → PayloadDeployed → Cleanup

## Deploy delay setting

`UnitDropPayloadDeployDelaySeconds` (TEMP default **1.25s**) on `UGP_OrbitalDeliverySettings`. Zero delay still valid (immediate payload after Impact). Separate from descent/cleanup.

## Presentation / replication

- Replicated `EGP_DropPodPhase` + `OnRep_Phase`
- NetMulticast Reliable: DescentStarted / Impact / PayloadDeployed → BlueprintImplementableEvents on all clients
- Native placeholder shown only while Descending; hidden on Impact
- Payload spawn authority-only, exactly once (`bPayloadSpawned`); timers cleared on EndPlay

## Owner Niagara workflow

| Event | Intended BP behavior |
|---|---|
| OnDescentStarted | show rocket, exhaust on, deploy FX off |
| OnImpact | hide rocket, exhaust off, deploy/construction FX on |
| OnPayloadDeployed | units exist; FX may fade |
| Cleanup | actor destroy removes attached FX |

No Niagara paths in C++.

## Tests / build

- Extended `gp.Resource.RunOrbitalUnitDropContractTest` (ground Z, deploy delay, zero-delay core path, cleanup)
- Operator PIE: S28 / ContainerLaunch(+HUD) / DropOff / SalvageWalker / LOS — Failures=0
- GPEditor Win64 Development + UHT — **PASS**
- GP Dev/Shipping — **NOT RUN**

## Files changed

- `GPUnitGroundPlacement.*`
- `GPDropPod.*` — phase, deploy timer, multicast, ground Z spawn
- `GPUnitDropAuthority.cpp` — pass deploy delay
- `GPOrbitalDeliverySettings.*` + `DefaultGame.ini`
- Contract test + docs

## Operator assets untouched

No BP/Niagara/map/DefaultEngine.ini committed.

## Commit SHA

*(filled after commit)*
