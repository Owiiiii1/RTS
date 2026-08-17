# GP-S33C — Unit Cap + Logistics Hub Capacity

## Status
**GP-S33C_IMPLEMENTATION_READY_FOR_OPERATOR_VALIDATION**

NOT MERGED.

## Slice Group
Post-GP-S33M (RTS Movement Reconciliation is on `main` @ `e40d545b89c27e2d9738082009fb691a5c8e5a2a`)

## Branch
`feature/gp-s33c-unit-cap-logistics-hub`  
Base: `main` @ `e40d545b89c27e2d9738082009fb691a5c8e5a2a`  
Head: `f8a8f2a70b8eb9531cebb68d37c4dfd26cf9a5ab`

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
- GP Development / GP Shipping (finalization after operator PASS)

## Architecture
- Server-authoritative; MaxUnits/CurrentUnits OwnerOnly via existing AttributeSet
- Base cap: infinite GE, once per PlayerState (`bBaseUnitCapApplied`)
- Reservation: `PendingOrbitalUnitCount` on PlayerState
- Hub bonus: infinite GE, `FActiveGameplayEffectHandle` owned by the Hub
- Editor-placed owned live Hub grants bonus once (gameplay belongs to the deployed entity)
- READY / ghost / descending DropPod never grant bonus

## Operator validation (manual — stop before merge)
1. PIE with 2 starting Workers → HUD `UNITS 2 / 5`
2. Order units until Current=5 → next order rejected Unit Cap reached
3. Kill one own unit → Current 5→4; one new order possible
4. Purchase Logistics Hub READY → Max remains 5
5. Deploy: descending Max remains 5; live Hub Max 5→10
6. Fill above 5 if practical
7. Destroy Hub → Max returns toward 5; existing units remain; new orders blocked if Current > Max

## Stop Condition
Implementation candidate ready. **NOT MERGED.** Await operator PIE PASS, then finalization builds (GP Dev / Shipping) in a later pass.
