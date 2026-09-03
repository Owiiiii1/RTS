# Voxel Plugin Technical Spike (Stage 3A)

**Date:** 2026-09-03
**Branch:** `terrain/gp-voxel-foundation`
**Base:** `origin/main` @ `569777625b8a4718289ad4809efa5ba5da09df7c`
**Status:** `NEEDS_PLUGIN_INSTALL`

This spike is discovery-only. No production terrain service, no Content, no `GP.uproject` change, no Worker leveling, no Foundation, no placement migration.

Canonical constraints (unchanged):

- Voxel terrain = continuous physical terrain
- BuildGrid stays discrete planning / occupancy / foundation
- FoW grid stays separate
- deformation is server-authoritative
- clients reconstruct; clients do not author gameplay craters
- do not assume a full NavMesh rebuild per deformation
- exact plugin version / API / replication must be proven, not guessed

---

## Environment

| Item | Proven value |
| --- | --- |
| Unreal Engine | **5.8.1** (`MajorVersion=5`, `MinorVersion=8`, `PatchVersion=1`, changelist `56057345`, branch `++UE5+Release-5.8`) |
| Engine path | `C:\Program Files\Epic Games\UE_5.8` |
| `GP.uproject` EngineAssociation (on-disk, operator-dirty, not committed here) | `"5.8"` |
| Project plugins dir | `D:\Progects\RTS\GP\Plugins` — **does not exist** |
| Voxel Plugin installed | **No** |

Searched and found **zero** `*Voxel*.uplugin` / VoxelPlugin / VoxelCore modules:

- `D:\Progects\RTS\GP\Plugins` (missing)
- `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins` (including `Fab`, `Runtime`, `Experimental`, `Enterprise`)
- `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Marketplace` (directory does not exist)
- `C:\Program Files\Epic Games\UE_5.5` and `UE_5.6` Engine trees
- `C:\epic57\UE_5.7`
- `C:\Users\owlni\AppData\Local\UnrealEngine` (including `5.8` and `Common`)
- `C:\Users\owlni\Documents\Unreal Projects`
- `D:\Progects`
- `C:\Users\owlni\Downloads`
- committed `GP.uproject` plugin list (GameplayAbilities, EnhancedInput, CommonUI, ModelViewViewModel, ModelingToolsEditorMode only)

No Voxel binaries, source, or metadata were available to inspect. Edition (Free / Pro / Legacy) is **not claimed**.

### Operator install requirement

Cursor must not download a random Voxel Plugin drop or modify the engine.

Operator needs to install a **Voxel Plugin package that the vendor lists as compatible with Unreal Engine 5.8.1**, then make it visible to this project **without Cursor guessing the marketplace SKU**.

Preferred later layout (not done in this checkpoint):

1. Install into `GP/Plugins/<VendorPluginFolder>/` so the plugin is project-local, **or** install via Fab into the associated UE 5.8 engine plugins if that is the vendor’s supported path.
2. Enable it in `GP.uproject` `Plugins[]`. That file is operator-protected; enabling is an **operator step** for the next checkpoint.
3. Re-run this spike’s API / compile probe against the installed source.

Until that exists, no production adapter and no `.uproject` compile integration.

---

## Plugin Modules

**None proven.** No `.uplugin`, no module names, no loading phases, no plugin dependencies.

---

## Proven Runtime APIs

No Voxel Plugin headers were present. The table records required capabilities and the **absence of a proven symbol**.

| Capability | Exact API | Header | Module | Proven status |
| --- | --- | --- | --- | --- |
| Own / spawn runtime voxel world | — | — | — | **NOT PROVEN** (plugin not installed) |
| Query surface / height at XY | — | — | — | **NOT PROVEN** |
| Query density / value | — | — | — | **NOT PROVEN** |
| Query material | — | — | — | **NOT PROVEN** |
| Sphere / crater / cut edit | — | — | — | **NOT PROVEN** |
| Sculpt add / remove | — | — | — | **NOT PROVEN** |
| Edit layers / stamps | — | — | — | **NOT PROVEN** |
| Async edit path | — | — | — | **NOT PROVEN** |
| Collision update after edit | — | — | — | **NOT PROVEN** |
| Mesh / render update after edit | — | — | — | **NOT PROVEN** |
| Runtime serialize / save edits | — | — | — | **NOT PROVEN** |
| Plugin networking / replication helpers | — | — | — | **NOT PROVEN** |
| Chunk dirty bounds / local update granularity | — | — | — | **NOT PROVEN** |

Do not treat any Voxel class name from memory, marketplace copy, or older engine drops as this project’s API.

---

## Runtime deformation

**No proven crater/edit call.**

Intended GP contract (already in TDD/16; not implemented): gameplay emits a generic request (`WorldLocation`, `Radius`, `Depth/Strength`, `Shape`, optional `SourceIdentity`). Only the future terrain service applies it, and only on authority.

Next experiment after install: one authority-only sphere/crater against a runtime voxel world, then observe collision + mesh update without claiming replication.

---

## Terrain query

**No proven query API.**

BuildGrid later needs equivalents of:

- `QuerySurfaceHeight(WorldXY)`
- slope / flatness over a cell or bounds
- `ApplyLeveling(...)` as progressive Worker edits (Stage 3B, not now)

Those names are **GP-facing abstractions**, not Voxel Plugin symbols.

---

## Collision/render update behavior

**Not observed.** Plugin not loaded. No compile probe (would require enabling the plugin in protected `GP.uproject`).

---

## Multiplayer findings

### What the plugin does

Unknown. No source, binaries, or replication types were present.

### What the plugin does NOT prove

- No evidence of deterministic/network replication
- No evidence of chunk/delta send
- No evidence of save-blob / snapshot for late join
- No listen-server host+client behavior

### Recommended GP authority model (candidate, not proven)

ADR-0010 already requires: **server authors deformation; clients reconstruct.**

Until plugin replication is proven, the only model that matches GP without inventing plugin APIs is:

**Option B (candidate):** server keeps a compact deformation **event log** (`location`, `radius`, `depth/strength`, `shape`, `revision`) and each machine applies the same local edit API.

| Option | Meaning | Spike verdict |
| --- | --- | --- |
| A — plugin-native replication | Use vendor net code if it is suitable and deterministic enough | **Cannot select.** Not installed. |
| B — compact server event log + local apply | GP authority = event sequence | **Preferred candidate** if the installed plugin exposes a runtime local-edit API that clients can call. **Not proven.** |
| C — server chunk / delta payload | Replicate voxel bytes | **Cannot select.** No serialization API proven. Likely heavier; only if B fails. |

Gameplay producers must not include Voxel Plugin types. Exact service class name is **not locked** this checkpoint. Existing GP pattern to follow later: `UWorldSubsystem` in `GPRuntime` (same layer as `UGP_BuildGridSubsystem`), with a private adapter that is the only code allowed to include Voxel headers.

---

## Determinism risk

**Not measured.**

Bit-identical geometry/collision across server and client must not be promised. Event reconstruction is only viable if the installed plugin applies the same operation parameters to the same initial voxel world with results close enough for:

- unit walking / collision
- BuildGrid height / slope queries
- later FoW surface projection (Stage 3E)

If local apply diverges, Option B is invalid and Option C / snapshot becomes the next experiment.

---

## Late join options

Not implemented. Candidates after plugin install:

1. Replay compact deformation history (only if history is small and apply is cheap/deterministic enough)
2. Authoritative terrain snapshot / save blob (only if plugin actually serializes runtime edits)
3. Out of spike scope until 1 or 2 is proven

---

## Performance risks

No benchmark numbers. Two future edit patterns that the installed plugin must support locally:

| Pattern | Expected shape | Risk if naive |
| --- | --- | --- |
| Combat crater | occasional localized sphere-ish destructive edit, many over a match | global mesh/collision rebuild; per-shot full world dirty |
| Worker leveling | many small rectangular edits over time | one edit every frame; unbounded dirty radius |

Recommended cadence **until measured:** never apply a gameplay deformation every Tick. Craters = discrete authority events. Leveling = batched / intervalled Worker contributions. Exact Hz is **to benchmark after install**. Local dirty bounds, async edits, and collision/mesh locality are **unknown** until source exists.

---

## BuildGrid integration seam

Proven in `UGP_BuildGridSubsystem` (`GPRuntime`, `UWorldSubsystem`):

- `DefaultCellSizeCm = 200.0f` (200 cm cells)
- occupancy is server-only (`TMap<FIntPoint, FGP_GridCellRecord>`), not replicated as a subsystem
- cell XY is independent of voxel representation; Z is not encoded in cell coordinates
- `CellToWorld(..., GroundZ)` currently takes an explicit ground Z (default `0`)
- placement nav check: `IsFootprintNavigable` → `UNavigationSystemV1::ProjectPointToNavigation`
- foundation / leveled-terrain placement is **not implemented** (TDD/06)

Voxel must not replace occupancy. Later adapter queries (height / slope / leveling apply) feed BuildGrid; they do not become the occupancy map. **BuildGrid was not modified in this checkpoint.**

---

## Navigation integration seam

Units **do** depend on Recast NavMesh when coverage exists:

- `UGP_MovementComponent::TryBuildNavigationPath` uses `UNavigationSystemV1::ProjectPointToNavigation` then `FindPathSync`
- if nav system / nav data is missing, or the unit is off-mesh, it **straight-line fallbacks**
- `bRequireNavigationWhenAvailable` rejects off-nav destinations only when the unit is already on mesh
- buildings keep `NavigationObstacle`; mobile units do not carve NavMesh (`AGP_MobileUnit`)
- PrototypeArena generator / contracts spawn `ANavMeshBoundsVolume` and expect `ARecastNavMesh`

A separate traversability grid **can** later sit above voxel geometry as GP source-of-truth for “is this crater walkable,” without merging into BuildGrid or FoW. That is Stage **3E**, not 3A.

Bounded next nav test (after a voxel world exists, still not a rewrite): one authority crater → observe whether Recast remains valid locally → confirm `RequestMove` around the crater **without** `RebuildAll`. Do not assume full rebuild per pit.

SWARM corpse obstacles (TDD/17) already reject mandatory NavMesh rebuild; same spirit.

---

## Stage 3A Recommendation

**NEEDS_PLUGIN_INSTALL**

| Question | Answer |
| --- | --- |
| Can we lock Voxel API symbols? | No |
| Can we implement the production deformation service? | No |
| Can we compile-probe inside GP modules? | No — plugin absent; enabling it requires protected `GP.uproject` |
| Should Stage 3B (Worker leveling) start? | **No** |
| Next action | Operator installs UE 5.8.1-compatible Voxel Plugin; then a follow-up 3A checkpoint audits source, optionally compile-probes, and revisits Options A/B/C |

### GO / BLOCKED

`NEEDS_PLUGIN_INSTALL` (blocker code: `VOXEL_PLUGIN_NOT_INSTALLED`)
