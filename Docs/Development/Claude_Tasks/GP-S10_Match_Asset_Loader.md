# GP-S10 — UGP_MatchAssetLoader
(PreloadForMatch and Resolve API)

## Slice Group
Slice 2 — Match Flow + Asset Loader

## Code Allowed
Yes — after SPEC_READY + explicit implementation assignment.

## Depends On
- GP-S09 DONE (prior Slice 2 stage; not a hard code dependency).
- ADR-0002 (Asset Manager only; soft refs).
- ADR-0006 (`UGP_MatchAssetLoader` is a documented allowed `UGameInstanceSubsystem`).
- Module `GPRuntime`.
- Engine `UAssetManager` / `FStreamableManager` (UE 5.8.1).

## Goal
Implement `UGP_MatchAssetLoader : UGameInstanceSubsystem` that accepts an explicit `TArray<FSoftObjectPath>`, async-preloads via Asset Manager StreamableManager, retains `FStreamableHandle`, reports Idle/Loading/Loaded/Failed, and resolves already-loaded objects/classes without sync loads, Content scans, Data Asset creation, or GameMode match-gate wiring.

## Status
**DONE**

Tech lead accepted. Operator accepted. Do **not** start GP-S11 until explicitly assigned (do not auto-materialize GP-S11 task file).

### Closed with
- `UGP_MatchAssetLoader : UGameInstanceSubsystem`
- raw `TArray<FSoftObjectPath>` input
- AssetManager StreamableManager (`UAssetManager::GetStreamableManager().RequestAsyncLoad`)
- async-only loading
- Idle / Loading / Loaded / Failed
- deterministic normalization (`FSoftObjectPathLexicalLess` + unique)
- invalid path → Failed
- empty request → Loaded
- same-set Loading/Loaded no-op
- Failed retry creates new generation
- different set cancels/releases and starts new generation
- stale callback protection (generation payload)
- retained `FStreamableHandle` after Loaded
- per-path completion verification
- no partial-ready
- Resolve only in Loaded state
- Resolve only for active requested set
- typed object Resolve
- typed class Resolve
- `ReleaseMatchAssets`
- safe `Deinitialize`
- no synchronous loading
- no GameMode changes
- no Primary Assets
- no Data Assets
- no Blueprint API
- no Tick
- no RPC
- no config/assets/tests
- operator Editor/PIE validation **PASSED**
- real asset async success/failure proof **deferred**
- GP-S11 not started

### Files
- `GP/Source/GPRuntime/Public/Assets/GPMatchAssetLoader.h`
- `GP/Source/GPRuntime/Private/Assets/GPMatchAssetLoader.cpp`

---

## Tech-lead locks (OD-1…OD-20) — RESOLVED + IMPLEMENTED

All OD-1…OD-20 locks from the SPEC_READY pass are implemented as closed above.

---

## Acceptance Criteria
- [x] Compiles (GPEditor Dev, GP Dev, GP Shipping) — PASSED
- [x] Subsystem created with GameInstance (operator PIE) — PASSED
- [x] Preload uses `UAssetManager::GetStreamableManager().RequestAsyncLoad`
- [x] Generation-safe repeated requests; stale callbacks no-op
- [x] Empty → Loaded + success; invalid → Failed without async; missing after load → Failed + release
- [x] Loaded retains handle; Failed/Release clears
- [x] Resolve only when Loaded + in set + loaded + cast; Failed → always null; no sync load
- [x] No GameMode/config/DA/PrimaryAsset/tests/Blueprint/GP-S11
- [x] Editor / module load (operator) — PASSED
- [x] `GP_MatchAssetLoader` found in Class Viewer (operator) — PASSED
- [x] PIE (operator) — PASSED
- [x] GP-S10 related errors — ABSENT
- [x] Tech lead accepted
- [x] Operator accepted
- [x] Real asset async success/failure proof — **deferred** (accepted for close; no stable match asset set)

## Manual Editor validation (operator) — PASSED
1. Open project — OK  
2. Module load — OK  
3. Class Viewer → `GP_MatchAssetLoader` — found  
4. Normal PIE — OK  
5. No map/config/Blueprint changes  
6. Real asset preload proof deferred  

## Build results
| Target | Result |
| --- | --- |
| GPEditor Win64 Development | PASSED |
| GP Win64 Development | PASSED |
| GP Win64 Shipping | PASSED |

## Out of Scope (confirmed)
- GameMode / match start gate
- Data Asset classes/instances / PrimaryAsset config
- Automation tests / test `.uasset`
- Blueprint API / sync Resolve / Content scan
- Network readiness / loading screen
- GP-S11+

## Linked canonical docs
TDD/13, TDD/10, TDD/14, ADR-0002, ADR-0006, GDD First_Playable, GP-S09.

## Stop Condition
GP-S10 closed as DONE. Do **not** start GP-S11. Do **not** auto-materialize the next task file.
