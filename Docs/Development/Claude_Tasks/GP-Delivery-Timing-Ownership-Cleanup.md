# GP — Delivery Timing Ownership Cleanup (Cleanup Slice G)

Status: **DELIVERY_TIMING_OWNERSHIP_CLEANUP_READY_FOR_OPERATOR_VALIDATION**

Branch: `feature/gp-delivery-timing-ownership-cleanup` from `origin/main` @ `75b13fc193531170eb3d4c1eaf9ee3f736d1d160`

Source audit: [`Configuration_Data_Ownership_Audit.md`](../Configuration_Data_Ownership_Audit.md)

## Change

Removed four Project Settings timing fallback fields from `UGP_OrbitalDeliverySettings`:

- `UnitDropDescentDurationSeconds`
- `UnitDropPayloadDeployDelaySeconds`
- `BuildingDropDescentDurationSeconds`
- `BuildingDropPayloadDeployDelaySeconds`

Canonical authored unit timing is `UGP_OrbitalUnitDropDefinition` (`DeliveryDescentSeconds` / `PayloadDeployDelaySeconds`). Native Worker / Salvage Walker bootstrap is 2.5 / 1.25 on `UGP_OrbitalUnitDropCatalog` construction. Mixed unit manifests keep max aggregation.

Canonical authored building timing is `UGP_OrbitalDropDefinition`. Native building bootstrap is 2.5 / 2.0 on `UGP_BuildingDropCatalog` construction. Pending remains `DefinitionNotReady`. Wall Package timing is independently owned and unchanged.

## INI

Committed `DefaultGame.ini` may still contain the four stale timing keys. Intentionally untouched because protected local config exists. After C++ removal those keys cannot populate runtime fields. No production GConfig/string lookup. Config hygiene later.

## Out of scope

Unit numeric/payload ownership, `UnitDropPodClass`, spawn altitude / cleanup delay settings, building payload/procurement, Wall Package redesign, `UnitDefinitionAsset` override semantics, authored content/config edits.

Operator validation: **pending**.
