# GP-S31R — Minimal Orbital Unit Drop

## Status
**FINALIZATION_READY_FOR_MERGE**

NOT MERGED.

## Slice Group
Slice 8 — Buildings + Orbital Drops

## Branch
`feature/gp-s31r-minimal-orbital-unit-drop`  
Base: `main` @ `118660bb24bda51c7d5e5c1b97cbc1b9d5cb0d4c`  
Operator FULL PASS tip: `8c661ba7…`

## Operator validation — FULL PASS
- Orbital unit manifest / slots / costs / spend
- Authored Worker + Salvage Walker payload BPs
- Authored BP_DropPod_MVP
- Descent + OnImpact deployment Niagara
- Independent PayloadDeployDelay + CleanupDelay
- Capsule-ground placement
- Spawned Worker Mine; Salvage Walker Move/Attack
- Units appear before pod cleanup

## Final architecture
- Manifest counts-only; server resolves classes from `UGP_OrbitalDeliverySettings`
- Instant `UGP_GE_SpendOrbital` (no direct attribute Set/Add)
- Shared `AGP_DropPod` + multicast presentation phases
- MainBase authored `UnitDropZone`
- Transport slots ≠ MaxUnits
- `GPUnitGroundPlacement` capsule half-height
- No building READY/placement in this slice

## Contracts
All listed resource/combat contracts Failures=0 (finalization regression).

## Builds
| Target | Result |
|---|---|
| GPEditor Win64 Development + UHT | **PASS** |
| GP Win64 Development | **PASS** |
| GP Win64 Shipping | **PASS** |

## Next slice (not started)
**NEXT_BUILDING_DROP_SLICE_PENDING_ID** — Orbital Building Procurement + READY Inventory + Placement + Drop (reuse DropPod / spend / lifecycle).

## Stop Condition
Ready for human merge review. Do **not** auto-merge.
