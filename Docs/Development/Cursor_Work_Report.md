# Cursor Work Report

## Status

**VOXEL_SPIKE_BLOCKED_PLUGIN_NOT_INSTALLED**

**INTERMEDIATE / NOT MERGE READY**

Stage 3A tech spike only. No production terrain service. Voxel Plugin is not installed on this machine. Do not start Worker leveling / Foundation / placement migration.

## Branch / base / head

| Item | Value |
| --- | --- |
| Path | `D:\Progects\RTS` |
| Branch | `terrain/gp-voxel-foundation` |
| Remote | `origin/terrain/gp-voxel-foundation` |
| Base `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Merge-base with `origin/main` | `569777625b8a4718289ad4809efa5ba5da09df7c` |
| Spike commit | recorded in the SHA follow-up |

Created from synced `main` with `git switch -c`. No rebase, reset, stash, or clean. Operator dirty/untracked preserved.

## Local plugin discovery

**Not installed.**

| Location | Result |
| --- | --- |
| `D:\Progects\RTS\GP\Plugins` | directory does not exist |
| `GP.uproject` Plugins[] | no Voxel entry (GameplayAbilities, EnhancedInput, CommonUI, ModelViewViewModel, ModelingToolsEditorMode) |
| `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins` | no `*Voxel*.uplugin` (Fab/Runtime/Experimental/Enterprise searched) |
| `Engine\Plugins\Marketplace` | directory does not exist |
| UE 5.5 / 5.6 / `C:\epic57\UE_5.7` | no Voxel uplugin |
| `%LOCALAPPDATA%\UnrealEngine`, Documents Unreal Projects, `D:\Progects`, Downloads | no Voxel uplugin |

Edition (Free / Pro / Legacy) is not claimed.

## Exact plugin version / path

**None.** Blocker: `VOXEL_PLUGIN_NOT_INSTALLED`.

## UE compatibility

| Item | Proven |
| --- | --- |
| Engine | UE **5.8.1** changelist `56057345` at `C:\Program Files\Epic Games\UE_5.8` |
| On-disk EngineAssociation | `"5.8"` in operator-dirty `GP.uproject` (not committed) |
| Plugin EngineVersion / binaries / Build.cs | **N/A — plugin absent** |
| Load under this editor | **not attempted** (nothing to load) |

No random plugin was downloaded. Engine was not modified. Compile probe skipped because enabling a plugin would require changing protected `GP.uproject`.

## Proven API symbols

**None.** Required capabilities (world own, height/density query, crater edit, collision/mesh update, serialize, net helpers, chunk dirtying) have no exact header/class/function until source exists. See `Docs/Development/Voxel_Plugin_Technical_Spike.md`.

## Authority recommendation

Gameplay must not call Voxel APIs. Future GPRuntime `UWorldSubsystem` (name not locked; same layer as `UGP_BuildGridSubsystem`) should own a generic deformation request. Only an adapter includes Voxel headers. Server accepts gameplay deformation; clients never decide craters.

## Reconstruction / replication recommendation

**Option B is the preferred candidate, not proven:** server compact event log (location / radius / depth / shape / revision) + local apply on each machine.

- Option A (plugin-native replication): cannot select
- Option C (chunk/delta payload): cannot select

## Determinism risks

Unmeasured. Do not promise bit-identical geometry. Event reconstruction is only valid if the installed plugin’s local apply is close enough for collision, BuildGrid queries, and later FoW surface work. Late join: replay vs snapshot vs out-of-scope — unproven; not implemented.

## Nav recommendation

Current units use Recast when coverage exists: `UGP_MovementComponent::TryBuildNavigationPath` → `UNavigationSystemV1::ProjectPointToNavigation` + `FindPathSync`, else straight-line fallback. Buildings use `NavigationObstacle`. Do not full-rebuild NavMesh per crater. Stage 3E: bounded test of one crater vs Recast locality; optional separate traversability overlay. No nav rewrite in 3A.

## BuildGrid seam

`UGP_BuildGridSubsystem`: **200 cm** cells (`DefaultCellSizeCm`). Occupancy independent of voxels. `CellToWorld` takes explicit `GroundZ`. Foundation/leveling queries are future adapter methods, not implemented. BuildGrid source was **not** changed.

## Files changed (this checkpoint)

- `Docs/Development/Voxel_Plugin_Technical_Spike.md` (created)
- `Docs/Development/Cursor_Work_Report.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/TDD/16_Voxel_Terrain_And_Foundations.md`
- `Docs/Architecture_Decisions/ADR_0010_Voxel_Terrain_And_Foundation_System.md`

No `GP/Source` production code. No Content / Config / maps / `GP.uproject` / Tools.

## Build result

GPEditor Win64 Development + UHT: **PASS** (`Result: Succeeded`, 2026-09-03). Docs/spike only; no production source change. GP Dev/Shipping not required.

## Protected before / after

**Before branch + after commit (same operator set, not committed):**

```
 M GP/Config/DefaultEngine.ini
 M GP/Config/DefaultGame.ini
 M GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap
 M GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset
 M GP/GP.uproject
?? GP/Content/Basic_VFX/
?? GP/Content/GrimProtocol/Blueprint/
?? GP/Content/GrimProtocol/DataAssets/Buildings/
?? GP/Content/GrimProtocol/DataAssets/Game/
?? GP/Content/GrimProtocol/DataAssets/Units/
?? GP/Content/GrimProtocol/Materials/
?? GP/Content/Mixed_Magic_VFX_Pack/
?? GP/Content/RocketThrusterExhaustFX/
?? Tools/
```

No `git reset --hard`, `git clean`, stash, or broad restore.

## Exact next action

1. Operator installs a **vendor Voxel Plugin listed for Unreal Engine 5.8.1** (project-local `GP/Plugins/` preferred, or Fab/engine path if that is the supported install).
2. Operator enables it in protected `GP.uproject` when ready (not this checkpoint).
3. Follow-up 3A checkpoint: source API audit with real symbols, optional compile probe, revisit Options A/B/C.
4. **Do not start 3B** until that follow-up is unblocked.
