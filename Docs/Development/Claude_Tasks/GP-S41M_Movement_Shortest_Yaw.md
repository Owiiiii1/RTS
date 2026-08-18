# GP-S41M — Movement Shortest Yaw Path

## Status
**DEFECT_RECORDED / NOT STARTED**

**Code Allowed: NO** until this slice is explicitly assigned.

Do **not** implement in GP-S40R. GP-S40R is finalized separately.

## Slice Group
Post-GP-S40R movement defect (unrelated to retaliation)

## Observation (operator, GP-S40R PIE)

A unit issuing a Move may rotate toward the destination using the **long yaw path** instead of the shortest angular path.

Example observation: the unit may rotate ~350° when ~10° would be sufficient.

Root cause is **not** claimed. No production change in this file.

## Intended next behavior

Movement facing should take the shortest yaw delta to the Move destination.

## Out of scope here

Retaliation, combat, LOS, Attack FSM, operator DataAssets / maps / config.
