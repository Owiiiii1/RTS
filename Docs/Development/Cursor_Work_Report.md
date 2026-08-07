# Cursor Work Report — GP-S28P3 Spec Correction (Subscriptions)

## Status
**GP-S28P3_SPEC_READY_FOR_REVIEW**

## Baseline
`docs/gp-s28p3-dropoff-resilience-spec` (docs-only; main @ `e90b7bd…` includes P2)

## Review correction
Fixed contradiction: mid-haul MainBase destruction cannot rely on bindings that exist only while WaitingForDropOff.

## Active-haul interruption subscription
While `ReturningToBase` / `DroppingOff`: bind GameState `OnMainBaseUnregistered` (preferred; matches ResourceNode registry pattern) filtered to **current haul target** → cancel move → WaitingForDropOff; preserve Cargo / Mine intent / search anchor. Clear binding on drop-off success, enter wait, command replace, haul leave, EndPlay.

## Waiting registration wake subscription
While WaitingForDropOff only: bind `OnMainBaseRegistered` → wake once → unsubscribe → clear retry → attempt haul. Unrelated register/unregister = no-op. Unregister while waiting = no-op.

## Docs-only
No C++ / Config / Blueprint / map / content changes.

## Commit
`e90fc0b244af884bfeb63a7592662d25abb24ecf`
