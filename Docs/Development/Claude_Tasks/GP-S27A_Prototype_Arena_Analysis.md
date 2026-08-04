# GP-S27A Persistent Prototype Arena + One-Shot Editor Generator + Ore Node

## Status
**GP-S27A_ANALYSIS_READY_FOR_REVIEW**

Analysis / design only. No production C++. No `.umap`. No Blueprint/assets. No gameplay changes. Build not required.

## Main baseline
`main` @ `805756d12477b23bf7794f674e534fafba8ce360`

Prior stage: **GP-S26B1_DONE_PRIMITIVE_VISUAL_FOUNDATION**  
Branch: `feature/gp-s27a-prototype-arena-analysis`

### Product decisions (locked)

| Topic | Decision |
| --- | --- |
| Map generation | **One-shot** editor tool |
| Result | Persistent normal `.umap` |
| PIE | **No** automatic generation |
| Post-gen | Manually editable actors |
| Runtime scenario generator | **Not needed** |
| Cosmetic projectile policy | **Unchanged** (still S26C later) |

---

## 1. Current project state (verified)

### Modules / targets

| Item | Reality |
| --- | --- |
| Runtime modules | `GP`, `GPGASRuntime`, `GPRuntime`, `GPUIRuntime` — all `"Type": "Runtime"` in `GP/GP.uproject` |
| Editor **module** | **Absent** — no `GPEditor.Build.cs`, no Editor-type module in `.uproject` |
| `GPEditor.Target.cs` | `TargetType.Editor` loading the four Runtime modules only |
| Python editor plugin | **Not enabled** |

### Content / maps

| Item | Reality |
| --- | --- |
| Project `.umap` | **None** (`git ls-files` / Content scan) |
| Content packages | 10 Enhanced Input `.uasset` under `GP/Content/GrimProtocol/Input/...` |
| `GameDefaultMap` | `/Engine/Maps/Templates/OpenWorld` (`DefaultEngine.ini`) |
| `EditorStartupMap` / `ServerDefaultMap` | **Unset** |
| `GlobalDefaultGameMode` | `/Script/GPRuntime.GP_GameMode` (`DefaultGame.ini`) |

Engine OpenWorld template is **not** repo content. Landscape / PlayerStart / NavMesh / World Partition state of that template is **not** verified from project files. Repo only has a World Partition editor commandlet class setting in `DefaultEngine.ini`.

### Gameplay framework (C++)

| Class | Role |
| --- | --- |
| `AGP_GameMode` | GameState `AGP_GameState`, PC `AGP_PlayerController`, PS `AGP_PlayerState`, DefaultPawn `AGP_CameraPawn` |
| HUD | No `HUDClass` set |
| Units | `AGP_Unit` placeable C++ (`Blueprintable`); **no** spawn factory / level actors in repo |
| Placement today | Manual PIE / editor placement outside tracked content; code only iterates existing units |

### Navigation / resources

| Item | Reality |
| --- | --- |
| NavMesh / Recast / NavigationSystem usage in project C++ | **None found** |
| Resource node actor | **None** |
| Tags exist | `GP.Resource.Node`, `GP.Resource.Type.Ferronite` (GPGASRuntime) |
| Command path | Validates `AGP_UnitBase` + `GP.Resource.Node` today — **mismatch** with non-unit ore design (implementation must align later) |

### Editor-usable unit properties

| API | Status |
| --- | --- |
| `TeamId` | `EditInstanceOnly` + replicated — **usable on placed instances** |
| `SetTeamId` | C++ authority-only; not Blueprint/`CallInEditor` |
| Combat defaults (`DefaultHealth`, Armor, …) | **`EditDefaultsOnly`** — class CDO, **not** per-instance |
| ASC init | Authority `SetNumericAttributeBase` once in BeginPlay from Defaults |

**Implication:** test pairs with different Health/Armor need new instance override properties or editor helpers in implementation (see §8).

### Git

`.gitattributes` already LFS-tracks `*.uasset` and `*.umap`.

---

## 2. Persistent arena map format

| Option | Complexity | Editor automation | Nav | MP PIE | Save actors | Git | Manual edit | Risk |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **A. New non-WP `L_PrototypeArena`** | Low–Med | Straightforward | Classic Recast bounds | Simple | Excellent | One LFS umap + manifest | Excellent | Low |
| B. New World Partition map | High | Harder actor save/load | WP nuances | Harder | More failure modes | Heavier | Possible but awkward | High |
| C. Zone on existing map | N/A | No project map exists | N/A | Couples to Engine OpenWorld | Poor | Contaminates engine template workflow | Bad | High |
| D. Data Layer / Level Instance | Med–High | Extra indirection | OK | OK | OK | Extra assets | OK | Med |

### Choice: **A — new compact non–World-Partition map**

Path proposal: `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`

Reasons: no project map today; OpenWorld template unsuitable as owned base; non-WP is simplest for one-shot generation, Recast, MP PIE, and hand-editing.

After acceptance: set `GameDefaultMap` / optional `EditorStartupMap` to `/Game/GrimProtocol/Maps/L_PrototypeArena` (implementation slice).

---

## 3. One-shot editor generator

### Options

| Mechanism | Fit |
| --- | --- |
| Editor console command + menu | **Primary** — explicit, scriptable, no PIE |
| Editor Subsystem owning generation | Holds shared logic / paths |
| Commandlet | Good CI rebuild later; heavier UX for operators |
| `CallInEditor` on editor-only actor | **Fallback** — simple but cluttery |
| Editor Utility Widget | Needs EUW asset; avoid for MVP |
| Unreal Python | **Rejected** — plugin not enabled |

### Primary: Editor module command

`gp.Editor.GeneratePrototypeArena` (and menu **GP → Generate Prototype Arena**)

- Manual only; never BeginPlay/PIE
- Lives in Editor module → excluded from packaged game / Shipping game target
- Create or open `L_PrototypeArena`, spawn actors, build nav, save package
- Default: **abort if map/package already exists**
- Separate: `gp.Editor.RebuildPrototypeArena` (explicit wipe of tagged generated actors or recreate package)

### Fallback

Transient editor-only `AActor` with `CallInEditor` “Generate” that calls the same subsystem.

---

## 4. Editor module proposal (not implemented here)

Name: **`GPEditor`** (Unreal convention; distinct from missing module today; `GPEditor.Target.cs` already names the Editor target)

| File | Purpose |
| --- | --- |
| `GP/Source/GPEditor/GPEditor.Build.cs` | `Type = ModuleType` Editor; deps: `GPRuntime`, `UnrealEd`, `LevelEditor`, `ToolMenus`, `EditorSubsystem`, `Engine`, etc. |
| `GPEditor.h` / `.cpp` | `IMPLEMENT_MODULE`, register menus/commands |
| `GPPrototypeArenaGenerator.h` / `.cpp` | Generation + save logic |
| `GPPrototypeArenaCommands.h` / `.cpp` | UI commands |
| Update `GP.uproject` | Module `"Type": "Editor"`, `"LoadingPhase": "Default"` |
| Update `GPEditor.Target.cs` | `ExtraModuleNames.Add("GPEditor")` |

Operator launch:
1. Open Editor (GPEditor target)
2. Console: `gp.Editor.GeneratePrototypeArena`
3. Or menu **GP → Prototype Arena → Generate**

Packaged game: Editor module not linked into `GP` Game target → commands absent.

---

## 5. `AGP_ResourceNode` architecture

| Topic | Decision |
| --- | --- |
| Base | `AActor` (replicated) — **not** `AGP_UnitBase` |
| Why not unit | Avoid command/ASC/attack/death coupling; ore is not a combat unit |
| Components | Root `USceneComponent` or `UBoxComponent` / `UCapsuleComponent` for **gameplay collision** |
| Visual | Prefer **shared** `UGP_PrimitiveVisualComponent` (refactor rename from unit visual) **or** thin `UGP_ResourceNodeVisualComponent` in A1 if refactor scope too large |
| Must not | Inherit `AGP_Unit` only for visuals |
| State | `ResourceType` (Ore only), `MaxAmount`, `CurrentAmount` (replicated, authority) |
| Gather / deplete | **Out of S27A** |
| Selection | Optional: add `GP.Capability.Selectable` / inspect tags only if selection layer can target non-`AGP_UnitBase` without large rewrite — **default S27A1: inspect via console; selection deferred** if architecture still unit-only |
| Tags | `GP.Resource.Node` + `GP.Resource.Type.Ferronite` (or Ore alias) on actor |
| Command mismatch | Today harvest validation expects `AGP_UnitBase` — **document follow-up** when gather lands; not in S27A |

### Replication

- `bReplicates = true`
- `CurrentAmount` / `MaxAmount` / `ResourceType` replicated
- No visual part replication (build locally like units)

---

## 6. Primitive ore composition (proposal)

Readable crystal pile; ≤6 parts; Engine shapes only; root collision only.

| Part | Shape | Parent | Approx loc / scale | Role |
| --- | --- | --- | --- | --- |
| Base | Cylinder | Root | (0,0,−20), scale (1.2, 1.2, 0.25) | Pedestal |
| Core | Cone | Base | (0,0,40), rot tip-up, scale (0.55, 0.55, 0.9) | Main crystal |
| CrystalA | Cone | Base | (35, 10, 30), tilted, scale (0.35, 0.35, 0.55) | Accent |
| CrystalB | Cone | Base | (−30, 20, 28), tilted, scale (0.30, 0.30, 0.50) | Accent |
| CrystalC | Cone | Base | (10, −35, 32), tilted, scale (0.32, 0.32, 0.52) | Accent |
| CrystalD (optional) | Cube rotated | Base | (0, 0, 70), scale (0.2, 0.2, 0.35) | Tip marker |

Gameplay: `UBoxComponent` ~120×120×80 around cluster, blocks pawns/nav as needed. Visual parts: **NoCollision**, no tick, no custom material.

---

## 7. `L_PrototypeArena` layout proposal

Assumptions: AttackRange ≈ **250**; RTS top-down camera; compact prototype.

### Arena metrics

| Metric | Value |
| --- | --- |
| Playable floor | **4000 × 4000** uu (origin-centered: X,Y ∈ [−2000, 2000]) |
| Wall height | ~300 uu |
| Floor | Scaled Engine Plane/Cube or BSP-less static mesh floor actor |
| Lighting | Single DirectionalLight + SkyAtmosphere/SkyLight minimal |
| GameMode | `AGP_GameMode` (world override or project default update) |
| PlayerStart | Center-back of Team1 side: `(−1200, 0, 100)` facing +X |
| NavMeshBounds | Cover full floor + margin |

### Zones

| Zone | Region (center) | Purpose |
| --- | --- | --- |
| Team1 Staging | (−1400, 0) | T1 starters |
| Team2 Staging | (+1400, 0) | T2 starters |
| Melee Pair | (0, −800) | In-range duel (~200 apart) |
| Blocked Pair | (0, −400) | High-armor/res target |
| Kill Pair | (0, 0) | Low-health target |
| OOR Pair | (0, +900) | Attacker vs target Dist ≈ **400–500** (>250) |
| Ore Field | (±600, ±1400) | 4–6 nodes |
| Obstacle belt | mid lanes | Cubes blocking paths |
| Future pad | (+0, +1600) empty | tanks/buildings later |

### Population (approx counts)

| Actor | Count | Notes |
| --- | --- | --- |
| Floor + 4 walls | 5 | Static, NoNav or wall block |
| DirectionalLight + Sky | 2 | |
| PlayerStart | 1–2 | Prefer 1 for listen host |
| NavMeshBoundsVolume | 1 | |
| RecastNavMesh | 1 | Built in editor |
| AGP_Unit Team1 | 3 | Staging |
| AGP_Unit Team2 | 3 | Staging |
| Melee pair | 2 | T1 attacker + T2 target, Dist≈180 |
| Blocked pair | 2 | Target needs high Armor/Res via instance overrides |
| Kill pair | 2 | Target low Health override |
| OOR pair | 2 | Dist≈450 along Y |
| Ore nodes | 5 | Spread in Ore Field |
| Obstacles | 6–8 | Cubes |
| **Total order** | **~40–50** actors | Keep lean |

### Labels / tags

- Folder or actor label prefix: `GP_Arena_`
- Actor tag: `GP.GeneratedPrototypeArena`
- Deterministic names: `GP_Arena_T1_Unit_01`, `GP_Arena_Ore_03`, `GP_Arena_Test_Melee_Attacker`, …

### Team IDs

- Team 1 = `1`, Team 2 = `2`, Ore/obstacles = neutral `0` or unassigned `-1` (prefer **0** for nodes if “neutral” is meaningful)

---

## 8. Generated actor ownership / API gaps

| Need | Current | Implementation gap |
| --- | --- | --- |
| Spawn `AGP_Unit` | Supported via `SpawnActor` in editor world | OK |
| Set `TeamId` | `EditInstanceOnly` property | Generator can set property before finish; or `SetTeamId` if editor world has authority |
| Per-instance Health/Armor | Defaults only `EditDefaultsOnly` | **Need** `EditInstanceOnly` combat overrides consumed by `InitializeCombatAttributesIfNeeded`, or editor helper writing overrides |
| Ore amounts | N/A | New UPROPERTY on ResourceNode |
| Obstacles | No class | Simple `AStaticMeshActor` / thin `AGP_BlockingVolume` with Cube mesh |
| PlayerStart / Nav bounds | Engine classes | Spawn standard engine actors |
| Visual archetype | UnitVisualComponent default InfantryMelee | OK for all units in A3 |

Document required minimal APIs for S27A1/A3 without implementing here.

---

## 9. Navigation policy (MVP)

| Topic | Choice |
| --- | --- |
| System | **Recast** (`NavMeshBoundsVolume` + `RecastNavMesh`) |
| Project today | Navigation **not** wired — must enable/add in arena map |
| Build | **Editor static build** after generation; save with map |
| Runtime regen | **Off** for MVP (no dynamic rebuild required) |
| Obstacles | Block navigation (collision + nav relevant) |
| Ore nodes | **Block** navigation (static deposit) |
| Units | Dynamic agents; standard pawn nav if Move uses it — verify Move path still point-move; do not change Move semantics in S27A |

If current movement is direct `SetActorLocation` without nav pathing, NavMesh still valuable for future and for operator “walkable” validation; generation should still place bounds + build.

---

## 10. Multiplayer PIE

| Requirement | How |
| --- | --- |
| Listen + client | Load same persistent map; no runtime generator |
| Authority | Server owns units/ore state; clients receive replication |
| No duplicate gen | Generator editor-only |
| Teams | Instance `TeamId` set at save time |
| PlayerStart | Single clear start; camera pawn from GameMode |
| Ore/units visible | Replicated actors + local primitive build on BeginPlay |
| Editor code in package | Not in Game target |

---

## 11. Idempotency / rebuild

| Mode | Behavior |
| --- | --- |
| **Generate (default)** | If `L_PrototypeArena` package exists → **abort** with clear log |
| **Rebuild (explicit)** | Separate command; deletes actors with tag `GP.GeneratedPrototypeArena` **or** recreates map from scratch after confirmation flag |
| Manual edits | Untagged / non-generated actors preserved on rebuild-if-tag-wipe; full recreate wipes all — document clearly |

**MVP default: Abort if exists** (safest).

Deterministic seed constant `GP_PROTOTYPE_ARENA_LAYOUT_VERSION = 1` + fixed transforms (no RNG), or seeded RNG with fixed seed recorded in manifest.

---

## 12. Git / binary map policy

| Topic | Policy |
| --- | --- |
| `.umap` | Tracked via **existing Git LFS** rules |
| Diff review | Binary — not human-reviewable; rely on companion manifest |
| Companion file | `GP/Content/GrimProtocol/Maps/L_PrototypeArena.layout.md` (or `.json`) — version, seed, actor list, transforms, teams |
| Generator | Deterministic; updates manifest alongside map |
| Do not introduce new LFS stack | Already present |

---

## 13. Implementation slices

### GP-S27A1 — Resource Node Foundation
- `AGP_ResourceNode` + Ore primitive visual
- Replication of amounts/type
- Inspect console
- **No map**

### GP-S27A2 — Editor Generator Foundation
- Add `GPEditor` module + command/menu
- Create empty non-WP `L_PrototypeArena` (floor/light/PlayerStart/nav bounds)
- Save/open/abort-if-exists
- No full population yet

### GP-S27A3 — Populate and Persist Arena
- Units, test pairs (needs combat instance overrides), ore, obstacles
- Nav build + save
- Manifest
- Optional default map switch
- Operator MP validation

**Order rationale:** Ore class usable without editor; editor module before map population; population last once placement APIs exist.

---

## 14. Validation matrix (future)

### Resource node
Host/client visible; amount replicates; root collision; nav block; inspect; no visual tick.

### Generator
Manual only; no PIE gen; create map; abort duplicate; save/reopen; deterministic labels; rebuild safety.

### Arena
Direct load; listen+client; units/teams/ore/pairs present; nav works; Move/Attack/selection/death unchanged; no manual placement required for matrix; Shipping/game build has no editor command.

---

## 15. Rejected options

| Option | Why |
| --- | --- |
| Runtime scenario generator | Product forbids |
| Generate every PIE | Product forbids |
| World Partition arena for MVP | Unnecessary complexity |
| Reuse Engine OpenWorld as owned map | Not in repo; WP; unstable ownership |
| Python EUW-first | Plugin absent; extra assets |
| Ore as `AGP_Unit` subclass | Wrong domain coupling |
| Silent regenerate overwrite | Destroys manual edits |

---

## Stop condition

- Docs committed on `feature/gp-s27a-prototype-arena-analysis`
- C++ diff: **none**; assets/umap diff: **none**; build: **not required**
- Do **not** implement ResourceNode, editor module, or map without explicit task
