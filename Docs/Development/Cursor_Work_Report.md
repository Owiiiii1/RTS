# Cursor Work Report — GP-S28P2 Depletion / Registry / Reassignment (Candidate)

## Status
**GP-S28P2_CODE_READY_OPERATOR_VALIDATION_PENDING**

## Branch
`feature/gp-s28p2-depletion-resource-reassignment`

## Base
`main` @ `86bcc9740fde0f19ac40c70f2f49298680f5f7d6` (Merge GP-S28P1)

## Operator-local assets — intentionally untouched
Not staged / deleted / restored / modified by this work:
- `GP/Config/DefaultEngine.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/Content/GrimProtocol/Blueprint/**`
- `GP/Content/GrimProtocol/Materials/**`
- local Niagara if present

## Depletion sequence
1. `ConsumeResource` crosses Previous>0 → New≤0  
2. Set replicated `bHasDepleted`  
3. Unregister from GameState; reject new Mine/slots  
4. Snapshot clear occupancy **without** promotion  
5. Broadcast `OnResourceDepleted` once  
6. Disable CollisionBox + nav influence; `ClearVisual` generated parts only  
7. Deferred Destroy via `DepletionDestroyDelaySeconds` (default **0.25**); `0` → next tick  

## Registry API
- `RegisterResourceNode` / `UnregisterResourceNode` / prune  
- `FindResourceCandidates` / `FindBestResourceCandidate`  
- Nav: `UNavigationSystemV1::FindPathSync` (no partial paths)  
- Sort: free slot → path length → direct distance → actor name  
- No production `GetAllActorsOfClass`  

## Search defaults
- `ResourceSearchRadiusCm` = **3000**  
- `MaxResourcePathLengthCm` = **6000**  
- `bAllowManualTargetOutsideAutoSearchRadius` = **true**  

## Reassignment / WaitingForResource
UnitCommand: free-slot prefer; deplete/destroy empty-cargo → retarget or `WaitingForResource`; wake on `OnResourceNodeRegistered` + ≤1 Hz safety timer; command replace unbinds. CargoFull hauls before alt Mine.

## FIFO
Per-node MaxConcurrentMiners=4 preserved; depletion/EndPlay clear without promote; normal Release still promotes head.

## Tests
| Command | Result |
| --- | --- |
| `gp.Resource.RunDepletionReassignmentContractTest` | Implemented; **operator PIE pending** (not claimed Failures=0 non-interactively) |
| `gp.Resource.RunS28RegressionSuite` | Includes new test; **operator PIE pending** |

## Builds
| Target | Result |
| --- | --- |
| GPEditor Win64 Development + UHT | **PASSED** |
| GP Dev / Shipping | Deferred to finalization |

## C++ files changed (intended)
- `GPGameState.h/.cpp`
- `GPResourceNode.h/.cpp`
- `GPResourceNodeSearch.h` (new)
- `GPWorker.h/.cpp`
- `GPUnitCommandComponent.h/.cpp`
- `GPDepletionReassignmentContractTest.cpp` (new)
- `GPContractIsolationAndSuite.cpp`

## Docs changed
- `Claude_Tasks/GP-S28P2_Depletion_Resource_Reassignment.md` (new)
- `AI_Project_Log.md`, `DOCUMENTATION_INDEX.md`, `Claude_Tasks/README.md`
- `Resource_Playable_Pass_Audit.md` (P2 addendum only)
- `Cursor_Work_Report.md`

## Assets / map / LFS
**Unchanged in git** (operator locals remain local).

## main / audits
`main` not modified by this branch work beyond creating feature from `86bcc974…`. Audit branches untouched.

## Commit SHA
`6c10937ffa3e1060e79ab1e8481e05c9f6aac6ed`

## Git status / sync
Branch pushed; operator-local Content/Config/map remain uncommitted; `main` untouched.
