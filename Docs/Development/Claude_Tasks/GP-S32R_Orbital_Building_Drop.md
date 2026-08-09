# GP-S32R — Orbital Building Drop

## Status
**GP-S32R_FINALIZATION_READY_FOR_MERGE**

NOT MERGED.

## Slice Group
Slice 8 — Buildings + Orbital Drops

## Branch
`feature/gp-s32r-orbital-building-drop`  
Base: `origin/main` @ `427a2aa6d9bc4c3733a044ec9b09d053d10dcc59`  
Operator-validated candidate: `71c19854f45fd659e35cc9b8ab4cc609e9799674`

## Scope (shipped)
- `UGP_OrbitalDeliverySettings` building catalog keys (cost, payload soft ref, drop timing, deploy radius)
- Native `AGP_LogisticsHub` (capsule root, no MaxUnits bonus) + authored BP child seam
- `UGP_OrbitalBuildingInventoryComponent` on `AGP_PlayerState` (OwnerOnly READY)
- `GPBuildingDropAuthority` — Purchase (spend once → READY++) / Deploy (validate → pod → consume READY)
- `AGP_DropPod` building payload kind + `AuthorityInitBuildingDrop`
- `GPBuildingGroundPlacement` capsule offset helper
- Local `AGP_BuildingPlacementGhost` + PC placement mode (LMB confirm / RMB cancel)
- Placement input ownership: clear selection on enter; command/selection gates; suppress-until-release
- TEMP HUD BUILDINGS panel (Purchase / Deploy READY)
- Contract: `gp.Building.RunOrbitalBuildingDropContractTest`

## Architecture
- **Purchase:** validate catalog + MainBase + Orbital → `UGP_GE_SpendOrbital` once → Ready++
- **Deploy:** validate READY + interim placement → spawn DropPod → consume READY (no Orbital spend)
- Interim placement (`INTERIM_MVP_PLACEMENT_VALIDATION`): radius from MainBase + deterministic building capsule extents
- Catalog path: settings keys (not full `UGP_BuildingDefinition` DA)

## Operator FULL PASS
Purchase / READY / ghost / deselect / RMB cancel without command / LMB deploy / shared DropPod / building spawn / authored LogisticsHub BP child — **PASS**.

## Deferred
full BuildingDefinition catalog · multiple types · real-mesh ghost · BuildGrid/FoW · LogisticsHub bonuses · turret/wall · Order Menu polish

## Final regressions / builds
All listed contracts **Failures=0**. GPEditor + GP Development + GP Shipping **PASS**.

## Stop Condition
Human merge only. Next after merge: **ROADMAP_RECONCILIATION_AUDIT_POST_GP-S32R** (do not auto-start GP-S34).
