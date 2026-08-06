# Cursor Work Report — GP-S28P2 Search-Anchor Correction

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28p2-depletion-resource-reassignment` (same branch; no merge; main untouched)

## Prior implementation
`6c10937ffa3e1060e79ab1e8481e05c9f6aac6ed`

## Operator failure
- 1 Worker, 1 MainBase, 2 ResourceNodes; Node A depletes; Worker hauls and unloads (`HaulDropOffComplete … Accepted=20`, `ReturnToDeposit=false`)
- Actual: Worker entered `WaitingForResource` instead of retargeting Node B

## Confirmed root cause
`FindAutoResourceCandidate` set a single `Origin` = Worker location at MainBase for both radius filtering and nav path start. After unload, Node B was outside `ResourceSearchRadiusCm` from the base despite being next to the depleted mining cluster.

## Exact search-anchor correction
- `MineSearchAnchorLocation` + `bHasMineSearchAnchor` on Mine executor
- Set on Mine accept to original ResourceNode location; survives Destroy/haul; cleared via `ResetMineExecutor` (Move/Attack/Stop/replace/cancel/EndPlay)
- Kept in `WaitingForResource`
- `FGP_ResourceNodeSearchQuery.SearchCenter` = anchor (radius); `PathStart` = Worker current location (nav / max path)

## New diagnostics
Non-shipping event logs on reassignment only: reason (`PostDepletion` / `PostDropOff` / `WaitingWake` / `SlotFullAlternative`), centers, rejects, selected candidate, or `ResourceReassignmentNoCandidate`. No Tick spam.

## Updated test
`gp.Resource.RunDepletionReassignmentContractTest` — post-drop-off anchor regression (MainBase outside radius, Node B inside anchor radius, retarget after unload), Move clears anchor, Waiting wake radius, CargoFull haul first, no permanent Tick.

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Dev / Shipping | Deferred to finalization |

## Operator-local assets — untouched / uncommitted
- `GP/Config/DefaultEngine.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/Content/GrimProtocol/Blueprint/**`
- `GP/Content/GrimProtocol/Materials/**`

## Commit SHA
`563a025296fd5311ce8066259f22dba891063950`
