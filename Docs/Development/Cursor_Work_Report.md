# Cursor Work Report — GP-S28P3 Specification

## Status
**GP-S28P3_SPEC_READY_FOR_REVIEW**

## Baseline
`main` @ `e90b7bd48fb9080a881e6dda7be889eaa99a3161` (GP-S28P2 merged)

## P2 Post-Merge Sync
**DONE** — docs updated: P2 = DONE/MERGED; last merged stage = P2; NEXT = P3 spec review (not merge P2).

## Actual Code Audit
- Missing MainBase today → `Failed` + `FinishHaulChain(true)` clears held Mine (cargo not cleared).
- `WaitingForStorage` enum exists, never entered; overflow remains LOST via `ClearCargo`.
- MainBase registry: register/unregister/Find — **no** register multicast wake yet.
- Haul approach: actor location + `DropOffRangeCm`; `DropOffVisualAnchor` presentation-only.
- P2 Mine intent / search anchor already persist across successful haul.

## Proposed P3 State Machine
Rename unused `WaitingForStorage` → `WaitingForDropOff` on haul + Worker activity.
Transitions: Haul → WaitingForDropOff (missing/dead/unreachable) → Haul → DropOff → P2 PostDropOff.
Command replace cancels wait subscriptions; Cargo preserved; no Threat during wait.

## Wake Strategy
GameState MainBase register (and optional unregister) events + `DropOffRetrySeconds` (default 3s) on existing `UGP_ResourceGameplaySettings`. Bind only while waiting.

## Test Plan
Contract: `gp.Resource.RunDropOffResilienceContractTest` (10 cases incl. P2 regression).
Operator PIE: A–D wait/destroy/block/Move-replace scenarios.

## Out of Scope
Storage-full redesign; overflow LOST change; multi-drop-off; Hub drop-off; drop-off interface; HUD; launch/orbital; combat; construction; queue; save/load.

## Files Changed
Docs only (no C++ / Config / uasset / umap).

## Commit
*(recorded after commit)*
