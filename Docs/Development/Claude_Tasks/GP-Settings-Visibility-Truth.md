# GP — Settings Visibility Truth (Cleanup Slice A)

Status: **SETTINGS_VISIBILITY_TRUTH_FINALIZED_READY_FOR_MERGE**

Branch: `feature/gp-settings-visibility-truth` from `origin/main` @ `283297012c1cefe162028a7ba4166c02a81230cc`

Source audit: [`Configuration_Data_Ownership_Audit.md`](../Configuration_Data_Ownership_Audit.md)

## Goal

Make Project Settings → Game → GP Orbital Delivery match actual ownership. No runtime precedence, INI, or authored-content change.

## Editor treatment

- Canonical DataAsset refs and true global transport/world tunables remain normal editable settings.
- Eight deprecated-active compatibility bridges keep Config + names/defaults/readers; they are no longer `EditAnywhere`.
- Four product timing seeds remain editable but labeled as fallback seeds, normally overwritten by product definitions.
- `DefensiveTurretPayloadClass` stays editable, labeled LEGACY override (still outranks BuildingDefinition.SpawnedClass).
- `BuildingPlacementOverlapMarginCm` stays Config, hidden from normal edit, marked unused. Not wired into gameplay.

## Out of scope

Slices B–N. Field removal. Asset/INI migration. Gameplay changes.

Operator validation: **PASS**.

