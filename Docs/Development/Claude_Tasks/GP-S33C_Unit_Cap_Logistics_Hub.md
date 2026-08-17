# GP-S33C — Unit Cap + Logistics Hub Capacity

## Status
**GP-S33C_FINALIZATION_READY_FOR_MERGE**

**NOT MERGED.** Do not claim `main` contains GP-S33C.

## Slice Group
Post-GP-S33M (RTS Movement Reconciliation is on `main` @ `e40d545b89c27e2d9738082009fb691a5c8e5a2a`)

## Branch
`feature/gp-s33c-unit-cap-logistics-hub`  
Base: `main` @ `e40d545b89c27e2d9738082009fb691a5c8e5a2a`  
Implementation head: `f8a8f2a70b8eb9531cebb68d37c4dfd26cf9a5ab`  
Finalization head: `8a2c08611f2a8d5a3373be0031d94a20b6cf9bf3`

## Goal
Authoritative player unit capacity (separate from DropPod Transport Slots) with base MaxUnits=5, living Worker/Salvage Walker CurrentUnits accounting, pending orbital reservation, and +5 MaxUnits per living deployed Logistics Hub via GAS.

## In scope (delivered)
1. Native infinite `UGP_GE_UnitCap_Base5` (+5 MaxUnits) applied once per PlayerState ASC
2. Authoritative CurrentUnits for Worker / Salvage Walker only
3. Pending orbital reservation (entity count, not transport slots)
4. Manifest cap: `Current + Pending + ManifestCount <= MaxUnits`; full reject `UnitCapReached`
5. Native infinite `UGP_GE_UnitCap_Plus5` on living deployed Logistics Hub; remove once on death/destroy
6. Over-cap after Hub death: Current unchanged; new orders blocked
7. TEMP HUD `UNITS Current / Max` + Unit Cap reached feedback
8. Contract `gp.Resource.RunUnitCapLogisticsHubContractTest`

## Out of scope (deferred)
- Logistics Hub container-cap bonus (`+N` TBD — do not invent)
- CommonUI / production Order Menu
- Local unit production
- BuildingDefinition DA for cap contribution
- Match Win/Lose (not started)

## Architecture
- Server-authoritative; MaxUnits/CurrentUnits OwnerOnly via existing AttributeSet
- Base cap: infinite GE, once per PlayerState (`bBaseUnitCapApplied`)
- Reservation: `PendingOrbitalUnitCount` on PlayerState
- Hub bonus: infinite GE, `FActiveGameplayEffectHandle` owned by the Hub
- Editor-placed owned live Hub grants bonus once (gameplay belongs to the deployed entity)
- READY / ghost / descending DropPod never grant bonus

## Operator validation
**FINAL PASS** (2026-08-17): start `UNITS 2 / 5`; fill to `5 / 5`; next manifest `Unit Cap reached` without Orbital spend; death `5 / 5 → 4 / 5` then re-order; live Hub `MaxUnits 5 → 10`; Hub destroy lowers Max; over-cap allowed without kill/clamp; new orders rejected while over-cap.

## Finalization note
Movement contract `A_ArrivedOrProgress` failed twice against map-authored Salvage Walkers at arena origin. Isolation-pad path restored in the contract only. Cap/Hub gameplay unchanged. GPEditor+UHT / GP Development / GP Shipping **PASS**. All listed regressions **Failures=0**.

## Stop Condition
Implementation + operator FINAL PASS complete. **NOT MERGED.** Do not start Match Win/Lose.
