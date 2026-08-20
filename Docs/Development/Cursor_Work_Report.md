# Cursor Work Report — Fog of War Runtime Foundation

## Status

**FOW_RUNTIME_FOUNDATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / baseline / head

- Branch: `feature/gp-fow-runtime-foundation`
- Exact base: `origin/main` @ `de718725115ddd636b56092bd6197cf0f7a65950`
- Candidate head: the implementation commit containing this report (`git rev-parse HEAD` after commit)

## Factual pre-change architecture

- `AGP_GameState` owned match state and authority registries but no FoW service.
- `UGP_UnitDefinition::SightRangeCm` was combat auto-acquire tuning; no FoW vision owner existed.
- Auto-acquire scanned valid hostile units without FoW filtering.
- Building authority validated MainBase radius, grid, navigation, and environment but not visibility.
- Unit drops use a fixed MainBase UnitDropZone, not a free-world landing selection.
- BuildGrid has 200 cm cells but no bounded playable-area authority.

## Runtime owner and semantics

- Chosen owner: non-replicated `UGP_FogOfWarComponent` default subobject on `AGP_GameState`.
- States: `Unexplored`, `Explored`, `Visible`.
- `Visible` always accumulates into `Explored`; current visibility may clear to `Explored`; explored bits
  never clear during the match.
- Each playable TeamId has an independent grid. There is no allied sharing.
- Invalid teams, client-side authority queries, non-finite locations, and out-of-bounds locations safely
  return `Unexplored`.
- Raw `TBitArray` grids are authority-only and are not replicated.

## Grid/world-bounds ownership

- Cell size: 200 cm.
- No canonical playable-area/map-bounds owner was found.
- Temporary foundation owner: FoW component origin `(-100000, -100000)`, dimensions `1000 x 1000`.
- FoW coordinates remain separate from deferred BuildGrid footprint/geometry ownership.

## Sight-source ownership and authored safety

- Canonical owner: `UGP_UnitDefinition::FogOfWarSightRadiusCm` and
  `UGP_UnitDefinition::bGrantsFogOfWarVision`.
- Building flow: `UGP_BuildingDefinition -> UnitDefinition -> AGP_UnitBase`; no duplicate building sight
  fields were added.
- Combat `SightRangeCm` remains independent.
- Native/fallback values: MainBase 3000, Worker 600, Salvage Walker 900, Logistics Hub 900, Defensive
  Turret 900 cm; all grant vision.
- Read-only Unreal inspection:
  - `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_MainBase`: 900 / true
  - `/Game/GrimProtocol/DataAssets/Units/DA_GP_Unit_Worker`: 900 / true
  - `/Game/GrimProtocol/DataAssets/Units/DA_GP_Unit_SalvageWalker`: 900 / true
  - `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_LogisticsHUB`: 900 / true
  - `/Game/GrimProtocol/DataAssets/Buildings/DA_Units/DA_GP_Unit_DefensiveTurret`: 900 / true
- Result: no authored-asset migration required; no DataAsset was edited.

## Update model

- Authority recomputes at 0.2 seconds / 5 Hz.
- Definition-ready live units/buildings self-register.
- Death and EndPlay unregister; team changes refresh immediately.
- Recompute scans only the bounded weak registry. It does not perform per-frame all-world discovery.
- Circle coverage only; no LOS occlusion, height, cones, stealth, or allied vision.

## Gameplay consumers integrated

### Auto-acquire

`UGP_UnitCommandComponent::FindNearestAutoAcquireTarget` rejects a hostile candidate unless its current
location is `Visible` to the owner team. Existing range, priority, LOS-at-fire, cooldown, damage,
retaliation, and Attack-Move behavior remain unchanged.

### Orbital building placement

Authority confirmation rejects a non-Visible snapped landing location with `NotVisible`. Remote-client
preview remains optimistic because a trusted client FoW mirror is out of scope; server confirmation is
authoritative.

### Unit drop

Unchanged. Unit drops land at the owning MainBase UnitDropZone and expose no free-placement target.
Temporary in-flight DropPod vision is deferred.

## Explicit Attack / last-known verdict

Audited, not implemented in this foundation. Full hidden-target pursuit, last-known location, fire
transition, and re-engage behavior require one coherent last-known model. Explicit Attack and retaliation
semantics were not partially rewritten.

## Replication and local selection verdict

Broad `IsNetRelevantFor` filtering, persistent last-known static actor state, local FoW mirror, and hidden
enemy local selection/inspect gating are deferred. No client-computed state is used for gameplay.

## Deferred FoW pieces

- local/client FoW mirror and explored deltas
- fog mask/terrain rendering
- last-known visual snapshots and unit blip fading
- production minimap and FoW layers
- CommonUI/MVVM FoW UI
- selection/inspect gating that depends on trusted client state
- explicit-Attack last-known integration
- broad replication relevance hiding
- in-flight DropPod vision
- LOS/height/cone/stealth/allied vision

## Changed production/test files

- `GP/Source/GPRuntime/Public/FogOfWar/GPFogOfWarComponent.h`
- `GP/Source/GPRuntime/Private/FogOfWar/GPFogOfWarComponent.cpp`
- `GP/Source/GPRuntime/Public/FogOfWar/GPFoWRuntimeFoundationContractTest.h`
- `GP/Source/GPRuntime/Private/Debug/GPFoWRuntimeFoundationContractTest.cpp`
- `GP/Source/GPRuntime/Public/Game/GPGameState.h`
- `GP/Source/GPRuntime/Private/Game/GPGameState.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitDefinition.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitDefinitionCatalog.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Private/Units/GPWorker.cpp`
- `GP/Source/GPRuntime/Private/Units/GPSalvageWalker.cpp`
- `GP/Source/GPRuntime/Private/Buildings/GPMainBase.cpp`
- `GP/Source/GPRuntime/Private/Buildings/GPLogisticsHub.cpp`
- `GP/Source/GPRuntime/Private/Buildings/GPDefensiveTurret.cpp`
- `GP/Source/GPRuntime/Private/Units/GPUnitCommandComponent.cpp`
- `GP/Source/GPRuntime/Public/Orbital/GPBuildingDropAuthority.h`
- `GP/Source/GPRuntime/Private/Orbital/GPBuildingDropAuthority.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPBuildGridContractTest.cpp`

## Changed documentation

- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/Claude_Tasks/GP-FoW-Runtime-Foundation.md`
- `Docs/Development/Claude_Tasks/README.md`
- `Docs/Development/DOCUMENTATION_INDEX.md`
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/TDD/15_Fog_of_War.md`

## Contract and regression results

- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunAutoAcquireContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunAttackMoveContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunRetaliationPursuitContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunOrbitalBuildingDropContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildGridContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunDefensiveTurretContractTest` — **PASS**, `Failures=0`
- `gp.Units.RunUnitDefinitionContractTest` — **PASS**, `Failures=0`
- `gp.Building.RunBuildingVitalsOwnershipContractTest` — **PASS**, `Failures=0`
- `gp.Resource.RunOrbitalUnitDropContractTest` — **PASS**, `Failures=0`

## Risk/escalation decision

The shared `AGP_GameState`, `AGP_UnitBase`, definition, combat, and placement surface triggered broader
affected regression coverage. Ten focused contracts cover the changed invariants. The historical full
resource suite was not run because it adds unrelated scope and has known authored-map contamination;
no selected regression exposed an unresolved cross-system failure.

## Build

- GPEditor Win64 Development + UHT — **PASS**
- GP Development / Shipping — intentionally deferred until operator PASS finalization

## Protected-content confirmation

No `GP/Config/DefaultEngine.ini`, `GP/Config/DefaultGame.ini`, map, Blueprint, DataAsset, material, VFX,
untracked Content, or `Tools/` file was modified, staged, reverted, stashed, reset, restored, or cleaned
by this slice. Existing local protected changes remain local and untouched.

## Exact operator test

1. PIE as Team 1 and run `gp.FoW.DebugDump`; record MainBase/Worker coordinates.
2. Query a Worker coordinate with `gp.FoW.QueryState 1 X Y`: expect `Visible`.
3. Move that Worker outward; query the new coordinate: expect `Visible`.
4. After all friendly sight leaves the old coordinate, query it: expect `Explored`.
5. Query an untouched in-bounds coordinate: expect `Unexplored`.
6. Place an enemy inside combat scan range but outside all friendly sight: no auto-acquire.
7. Move friendly sight onto the enemy: auto-acquire works.
8. Confirm an orbital building at a queried non-Visible location: authority rejects.
9. Confirm at an otherwise-valid queried Visible location: authority accepts.

## Merge/finalization state

**NOT MERGED. NOT FINALIZED.**
