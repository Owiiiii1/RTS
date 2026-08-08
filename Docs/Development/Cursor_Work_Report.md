# Cursor Work Report — GP-S31R Authored Payload Integration

## Status
**GP-S31R_AUTHORED_PAYLOAD_INTEGRATION_READY_FOR_OPERATOR_RETEST**

NOT MERGED. NOT FINALIZED.

---

## Operator gameplay validation

**PASS** (prior candidate), except authored Blueprint visuals missing on spawned units / DropPod.

## Bug / root cause

`AGP_DropPod::AuthoritySpawnUnitPayload` spawned `AGP_Worker::StaticClass()` / `AGP_SalvageWalker::StaticClass()`.  
`GPUnitDropAuthority` spawned `AGP_DropPod::StaticClass()`. Authored BP subclasses were bypassed.

## Fix architecture

Kept capacity/costs/descent in `UGP_OrbitalDeliverySettings` (smallest SoT; no new DropPodDefinition DA).

Added soft/authorable class refs (no hardcoded `/Game` paths):

| Field | Purpose |
|---|---|
| `WorkerPayloadClass` | `TSoftClassPtr<AGP_Worker>` |
| `SalvageWalkerPayloadClass` | `TSoftClassPtr<AGP_SalvageWalker>` |
| `UnitDropPodClass` | `TSoftClassPtr<AGP_DropPod>` |

Resolve helpers: load soft → require subclass of required base → else native fallback + warn if invalid soft set.

Manifest remains counts-only; client never submits class.

## DropPod presentation

`bUseNativePlaceholder` (default true). Authored `BP_DropPod_MVP` sets false on CDO; uses BP events `OnDescentStarted` / `OnImpact` / `OnPayloadDeployed`.

## Owner Blueprint workflow

1. Create `BP_Worker` : `AGP_Worker`, `BP_SalvageWalker` : `AGP_SalvageWalker`, `BP_DropPod_MVP` : `AGP_DropPod` (owner assets — not created by Cursor).
2. On DropPod BP: set `bUseNativePlaceholder=false`; add mesh/Niagara; implement presentation events.
3. Project Settings → Game → GP Orbital Delivery: assign the three soft classes.
4. Retest Confirm Drop — units/pod should use authored visuals.

## Capacity / cost location

Still `UGP_OrbitalDeliverySettings` (PodTransportSlotCapacity, slot costs, Orbital costs, descent tuning). Future `UGP_DropPodDefinition` documented as optional extension — not added now.

## Contracts / build

- Extended `gp.Resource.RunOrbitalUnitDropContractTest` (stub subclasses A–F + prior semantics)
- Operator PIE: rerun S28 / ContainerLaunch(+HUD) / DropOff / SalvageWalker / LOS — Failures=0
- GPEditor Win64 Development + UHT — **PASS**
- GP Dev/Shipping — **NOT RUN**

## Files changed

- `GPOrbitalDeliverySettings.*` — soft classes + resolve
- `GPDropPod.*` — settings-resolved payload spawn + `bUseNativePlaceholder`
- `GPUnitDropAuthority.cpp` — resolve DropPod class
- `GPOrbitalUnitDropContractTest.*` — stub classes + seam checks
- `DefaultGame.ini` — comment on soft class assignment
- Docs: task, AI log, this report

## Operator assets untouched

No BP_Worker / BP_SalvageWalker / BP_DropPod_MVP / Niagara / map / Blueprint/ / Materials / DefaultEngine.ini committed.

## Commit SHA

*(filled after commit)*
