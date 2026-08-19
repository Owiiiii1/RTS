# GP — Unit Payload Compatibility Cleanup (Cleanup Slice D)

Status: **UNIT_PAYLOAD_COMPAT_CLEANUP_READY_FOR_OPERATOR_VALIDATION**

Branch: `feature/gp-unit-payload-compat-cleanup` from `origin/main` @ `47a220b480e455f1cf5dfb6ca0613c13cf760a53`

Source audit: [`Configuration_Data_Ownership_Audit.md`](../Configuration_Data_Ownership_Audit.md)

## Change

Removed `UGP_OrbitalDeliverySettings::WorkerPayloadClass` and `SalvageWalkerPayloadClass`, plus settings resolvers `ResolveWorkerPayloadClass` / `ResolveSalvageWalkerPayloadClass` / `IsWorkerPayloadClassConfigInvalid` / `IsSalvageWalkerPayloadClassConfigInvalid`.

Canonical authored payload is `UGP_OrbitalUnitDropDefinition.PayloadClass`. Native bootstrap is `AGP_Worker` / `AGP_SalvageWalker` on `UGP_OrbitalUnitDropCatalog` construction. DropPod continues to resolve payload from the catalog. Nested PayloadClass pending remains `DefinitionNotReady`.

## INI

Committed `DefaultGame.ini` may still contain `WorkerPayloadClass` / `SalvageWalkerPayloadClass`. Intentionally untouched because protected local config exists. After C++ removal those keys cannot populate runtime fields. No production GConfig/string lookup. Config hygiene later.

## Out of scope

Unit numeric ownership, `UnitDropPodClass`, timing fallbacks, building payload/procurement, Wall Package, `UnitDefinitionAsset` override semantics, authored content/config edits.

Operator validation: **pending**.
