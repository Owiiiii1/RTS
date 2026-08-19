# GP — Unit Drop Nested Readiness

Status: **UNIT_DROP_NESTED_READINESS_READY_FOR_OPERATOR_VALIDATION**

Branch: `feature/gp-unit-drop-nested-readiness`

## Problem

Authored `UGP_OrbitalUnitDropDefinition` Ready previously meant only the top-level DataAsset was loaded. Nested `UnitDefinition` and `PayloadClass` were already-loaded-only resolvers, so cold start could treat a product as Ready and then substitute deprecated settings/native payload class or spawn without the product UnitDefinition.

## Contract

For a configured authored unit product slot, Ready requires:

1. Top-level `UGP_OrbitalUnitDropDefinition` loaded
2. `UnitDefinition` soft ref non-null and loaded
3. `PayloadClass` soft ref non-null, loaded, and a valid slot subclass (`AGP_Worker` / `AGP_SalvageWalker`)

Pending: purchase/spend/manifest/pod rejected as `DefinitionNotReady`. 
Invalid/failed nested dependency: Failed + native bootstrap, explicit log, not stuck Pending.

Loads are `AssetManager` / `StreamableManager` async only.

## Payload precedence

1. Canonical Ready authored `UGP_OrbitalUnitDropDefinition.PayloadClass`
2. Native bootstrap product (empty PayloadClass) → deprecated settings `WorkerPayloadClass` / `SalvageWalkerPayloadClass`
3. Native C++ class

Deprecated settings classes must not replace a valid authored product class because it was cold.

## UnitDefinition spawn behavior

`AGP_DropPod` still assigns product `UnitDefinition` only when `UnitDefinitionAsset` is empty. Explicit BP/CDO `UnitDefinitionAsset` is unchanged (not audit slice H).

## Native / empty

Unconfigured slots keep native bootstrap. Configured+failed authored slots fall back to native.
