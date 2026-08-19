# GP — Unit Numeric Compatibility Cleanup (Cleanup Slice C)

Status: **UNIT_NUMERIC_COMPAT_CLEANUP_FINALIZED_READY_FOR_MERGE**

Branch: `feature/gp-unit-numeric-compat-cleanup` from `origin/main` @ `967e6ea3a5b81ddc1a2c19c4bfe292f5ef989507`

Source audit: [`Configuration_Data_Ownership_Audit.md`](../Configuration_Data_Ownership_Audit.md)

## Change

Removed four deprecated Project Settings numeric bridges from `UGP_OrbitalDeliverySettings`:

- `WorkerTransportSlotCost`
- `SalvageWalkerTransportSlotCost`
- `WorkerOrbitalDropCost`
- `SalvageWalkerOrbitalDropCost`

Canonical authored Cost / TransportSlotCost is `UGP_OrbitalUnitDropDefinition`. Native bootstrap Worker 25/1 and Salvage Walker 50/2 is owned by `UGP_OrbitalUnitDropCatalog` native product construction. Authority and TEMP HUD read catalog getters, not Project Settings.

## INI

Committed `DefaultGame.ini` still contains the four stale keys. Intentionally untouched because protected local config exists. After C++ removal they cannot populate runtime fields. No production GConfig/string lookup. Config hygiene later.

## Out of scope

Payload bridges (`WorkerPayloadClass`, `SalvageWalkerPayloadClass`), `UnitDropPodClass`, unit/building timing fallbacks, building numeric/payload bridges, Wall Package, authored content/config edits.

Operator validation: **PASS**.
