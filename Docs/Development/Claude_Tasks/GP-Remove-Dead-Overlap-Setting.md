# GP — Remove Dead Overlap Setting (Cleanup Slice B)

Status: **DEAD_OVERLAP_SETTING_REMOVAL_FINALIZED_READY_FOR_MERGE**

Branch: `feature/gp-remove-dead-overlap-setting` from `origin/main` @ `f38e803771261c60d865949c693a52a73fbcedb2`

Source audit: [`Configuration_Data_Ownership_Audit.md`](../Configuration_Data_Ownership_Audit.md)

## Change

Removed `UGP_OrbitalDeliverySettings::BuildingPlacementOverlapMarginCm` from the C++ configuration API.

No production runtime reader existed. Placement behavior is unchanged. No replacement margin.

## INI

Committed `DefaultGame.ini` still contains `BuildingPlacementOverlapMarginCm=25.000000`. Intentionally untouched because protected local config exists. After C++ removal the stale key cannot populate a runtime field. Config hygiene later.

Operator validation: **PASS**.


## Out of scope

Placement validation, footprint, SAT/OBB, deploy radius, NavigationObstacle, authored content/config edits.
