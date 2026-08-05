# GP-S26B2A — Blueprint Authored Visuals

## Status
**GP-S26B2A_FINALIZED_READY_FOR_MERGE**

## Baseline
`main` @ `215b4b603e7fd333ef9b379103329bfac03edbf4`

Branch: `feature/gp-s26b2a-blueprint-authored-visuals`  
Candidate: `3a6d9533039180a4b75d40dc6063abd01d1b91e2`

## Abandoned experiment
`feature/gp-s26b2a-editable-visual-profiles` @ `54bfe62d5c6b54edfa7cdff02ff48e221f9a98ff` — **abandoned, never merged**. Not cherry-picked. Remote left in place; unused.

## Shipped
- `EGP_VisualSourceMode::{NativeFallback, AuthoredComponents}`
- Unit / ResourceNode visual components: Class Defaults mode, `RefreshVisualMode`, BuiltVisual-only cleanup
- Example BPs: `BP_Unit_AuthoredExample`, `BP_ResourceNode_AuthoredExample` (LFS)
- Inspect fields extended; native definitions/builder retained
- No DataAssets / profiles / map population / materials / animations

## Operator validation matrix (accepted)

| Area | Result |
| --- | --- |
| BP_Unit_AuthoredExample AuthoredComponents | **PASS** (GeneratedPartCount=0, Authored=4) |
| BP_ResourceNode_AuthoredExample AuthoredComponents | **PASS** (GeneratedPartCount=0, Authored=6) |
| Direct AGP_Unit native fallback | **PASS** (Parts=3 Body/Forward/Weapon) |
| Direct AGP_ResourceNode native fallback | **PASS** (Parts=5 Base/Core/AccentA/B/C) |
| No native overlay on authored | **PASS** |
| Authored components survive | **PASS** |
| Collision/nav warnings = 0 | **PASS** |
| Resource gameplay Box / amounts | **PASS** |
| Listen server | **PASS** |
| L_PrototypeArena not populated | **PASS** (temp placements not committed) |

## Builds (finalization)
- GPEditor Dev+UHT: retained from candidate (no C++ changes)
- GP Win64 Development — **PASSED**
- GP Win64 Shipping — **PASSED**

## Known limitations
- Authored team tint not automatic (Blueprint materials later)
- Examples use Engine BasicShapes (not production art)
- Abandoned DataAsset profile approach unused

No known blockers. Ready for main merge when requested.
