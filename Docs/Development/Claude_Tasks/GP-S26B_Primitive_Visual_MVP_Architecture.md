# GP-S26B Primitive Visual MVP Architecture

## Status
**GP-S26B_ANALYSIS_READY_FOR_REVIEW**

Analysis / design only. No production C++. No Content assets created. No Blueprint packages. No gameplay changes.

Supersedes prior filename `GP-S26B_Combat_Assets_Analysis.md` (same branch; product direction revised to Primitive Visual MVP).

## Main baseline
`main` @ `80251125bbf03566edb4ec902f8770ee900d9bde`

Prior stage: **GP-S26A_DONE_PRESENTATION_ASSETS_DEFERRED**  
Source channel: `Docs/Development/Claude_Tasks/GP-S26_Combat_Presentation.md`  
Branch: `feature/gp-s26b-combat-assets-analysis`

---

## Product goal

MVP must become a **fully playable and visually readable RTS** using **Engine primitives and composite primitive meshes**, without waiting for authored art.

Every major entity class, attack, projectile, impact, death, and building needs a temporary visual that can later be swapped for real models **without changing gameplay code**.

### Visual coverage catalog (architecture scope)

| Domain | Must be readable |
| --- | --- |
| Units | infantry melee/ranged, heavy infantry, worker, tank, artillery, defensive turret |
| Creatures | melee monster, ranged monster, boss monster |
| Buildings | HQ, barracks, factory, defensive building, resource node |
| Projectiles | bullet, shell, energy |
| Impacts | normal, blocked, killing |
| Lifecycle | unit death, building destruction |
| State | movement direction, attack direction, team ownership, selection, health |

Not all items ship in one coding slice — roadmap slices below. Architecture must cover the full catalog.

---

## Verified foundations (locked)

| Fact | Evidence |
| --- | --- |
| Project combat art assets | **Absent** — Content has only 10 Enhanced Input `.uasset` packages |
| Skeletal / AnimBP / Montage / Niagara / Sound | **Absent** |
| Current unit visual | `AGP_Unit`: capsule root + `UStaticMeshComponent` + Engine `/Engine/BasicShapes/Cylinder` |
| Engine basic shapes | Available (Cylinder, Cube, Sphere, Cone, etc.) |
| GP-S26A presentation | Unreliable NetMulticast after Apply; listen/client validated |
| Gameplay vs visual | Damage/cadence/TargetDied independent of presentation |

### Explicit non-goals for Primitive MVP

- No third-party / authored mesh import as a dependency
- No SkeletalMesh migration as part of Primitive MVP
- No animation system (montage / AnimInstance) dependency
- No hard references from gameplay (command/damage/GAS) to visual assets

---

## Recommended architecture

### Primary: `UGP_UnitVisualComponent` + `UGP_PrimitiveVisualProfile`

```text
Gameplay (unchanged)
  → S26A CombatPresentationComponent (Unreliable multicast Impact)
  → UnitVisualComponent (local cosmetic only)
       → builds parts from PrimitiveVisualProfile
       → plays transform cosmetics (lunge / hit / death)
       → optional local projectile visuals (later slice / S26C)
       → keeps S26A debug fallback when profile/parts missing
```

| Owner | Responsibility |
| --- | --- |
| `UGP_UnitVisualComponent` | Create/own composite primitive parts; build from profile; presentation root; facing; transform animations; attack/hit/blocked/death cosmetics; consume S26A accepted events; debug fallback; dedicated suppression; **no** gameplay authority |
| `UGP_PrimitiveVisualProfile` | Data describing archetype parts + style params (soft Engine mesh paths OK; no gameplay hard refs) |
| Capsule / existing root | Gameplay collision + selection query authority |
| `UGP_CombatPresentationComponent` | Network cosmetic channel only (unchanged contract) |

**Fallback authoring:** BlueprintAssignable accept event (Option A) if unit BPs appear later — still no second RPC channel.

**Rejected for Primitive MVP:** GameplayCue-primary path; skeletal montage pipeline; damage-from-projectile.

---

## Profile storage choice (MVP)

| Option | Verdict |
| --- | --- |
| `UPrimaryDataAsset` profile | **Primary** — soft part mesh paths, style enums, timings; swappable later for real visual profiles |
| Native struct + enum defaults in C++ | Bootstrap defaults for first infantry prototype without Content DA instance |
| DataTable | Optional catalog index later; not required for B1 |
| Blueprint-authored profile | Optional operator step; not required for C++ foundation |
| Hard refs from Command/GAS | **Forbidden** |

**MVP choice:** C++ `UGP_PrimitiveVisualProfile` DataAsset type + **native default struct/enums** for the first `AGP_Unit` infantry prototype so B1 can run with **zero new Content packages**. Optional DA instances added when catalog expands (B3), preferably by editor/operator.

---

## Primitive part model

### `FGP_PrimitiveVisualPart`

| Field | Purpose |
| --- | --- |
| `PartName` | Stable id (`Body`, `Forward`, `Barrel`, …) |
| `PrimitiveShape` | `Cube`, `Sphere`, `Cylinder`, `Cone`, `Capsule` (`Plane` only if required) |
| `RelativeLocation` / `RelativeRotation` / `RelativeScale` | Local transform |
| `ParentPartName` | Hierarchy (empty = attach to presentation root) |
| `bPresentationRoot` | Marks attach root under capsule |
| `bFacingIndicator` | Reads as forward arrow / nose |
| `bWeaponPart` | Attack thrust / recoil target |
| `bTurretPart` | Independent yaw (tank/turret) |
| `bAnimatedPart` | Eligible for bob/lunge/recoil |
| `TeamColorMode` | `None` / `Primary` / `Accent` |
| `CollisionMode` | Always **NoCollision** for pure visuals in MVP |
| Visibility rules | Optional hide-when-dead / hide-on-DS |

### Construction rules

- One `UStaticMeshComponent` per part (or pooled equivalent)
- Mesh = Engine basic shape via **soft** path resolve (e.g. `/Engine/BasicShapes/Cube`)
- Components **Movable**; collision **disabled**
- Gameplay collision remains on existing capsule/root
- Visual parts **never** participate in authoritative hit detection or attack range
- Bounded part count per archetype (guideline: ≤ 8–12 parts)

---

## Archetype catalog

Legend for actor column:
- **Existing:** current C++ actor can host visual now
- **Future:** entity type not present as dedicated class yet — visual archetype reserved

| Archetype | Parts (hierarchy) | Move style | Attack style | Death style | Actor |
| --- | --- | --- | --- | --- | --- |
| Infantry_Melee | Body(cyl) ← Forward marker ← short weapon cube | InfantryBob | MeleeLunge + WeaponThrust | FallOver / CollapseScale | **Existing** `AGP_Unit` prototype |
| Infantry_Ranged | Body ← barrel cyl | InfantryBob | WeaponThrust / BarrelRecoil | FallOver | Future specialization / class defaults |
| Heavy_Infantry | Larger body + shoulder cubes + heavy weapon | InfantryBob | BodyRecoil + WeaponThrust | CollapseScale | Future |
| Worker | Smaller body + tool marker | InfantryBob | WeaponThrust (light) | CollapseScale | Future |
| Tank | Hull cuboid ← Turret ← Barrel | VehicleStable | TurretFirePulse + BarrelRecoil | Sink / CollapseScale | Future |
| Artillery | Low base + long barrel + supports | VehicleStable | BarrelRecoil (strong) | CollapseScale | Future |
| Turret | Fixed base ← rotating head ← barrel | None | TurretFirePulse + BarrelRecoil | CollapseScale | Future |
| Monster_Melee | Sphere core + limb capsules/cones | MonsterBounce | MonsterBite/Pulse + Lunge | BreakApart-lite / CollapseScale | Future |
| Monster_Ranged | Body + emitter/spike marker | MonsterBounce | BodyRecoil / Pulse | CollapseScale | Future |
| Monster_Boss | Multi-part larger core + horns/limbs | HoverBob / MonsterBounce | Multi-part Pulse | Staged CollapseScale | Future |
| HQ | Large central block + tower/core | None | (building hit pulse) | Building crush/Sink | Future |
| Barracks | Long rect + entrance marker | None | Hit pulse | CollapseScale | Future |
| Factory | Large block + stack cylinders | None | Hit pulse | CollapseScale | Future |
| Defense_Building | Base + turret structure | None | TurretFirePulse | CollapseScale | Future |
| Resource_Node | Crystal: cones / rotated cubes | None / HoverBob | N/A (gather later) | Shrink/Collapse | Future |

Visual archetype ≠ gameplay class. Combat rules stay on command/GAS; profile only changes cosmetics.

---

## Primitive animation model

No Timeline assets, montages, or skeletal animation.

### Driver

- Component-owned **transient** cosmetic state
- Enable tick **only while** an animation is active; disable when idle
- Timer or tick interpolation with math easing (ease-out / smoothstep)
- **Not** replicated; each viewport runs the same deterministic recipe from event + profile

### Styles

| Category | Styles | Notes |
| --- | --- | --- |
| Movement | None, InfantryBob, MonsterBounce, HoverBob, VehicleStable | Optional low-amp; prefer event/timer over permanent tick; VehicleStable = no bob |
| Attack | MeleeLunge, WeaponThrust, BarrelRecoil, BodyRecoil, TurretFirePulse, MonsterBite/Pulse | Triggered by S26A Impact (B2) or future Fire event (S26C) |
| Hit | ScalePulse, PositionKick, RotationKick; blocked vs killing variants | Flash only if material strategy supports safe runtime tint |
| Death | FallOver, CollapseScale, Sink; BreakApart-lite (≤N detach parts); Fade only with safe material | Must respect `DeadActorLifeSpan`; cancel attack cosmetics |

### Policies

| Topic | Rule |
| --- | --- |
| Duration | Profile timings (e.g. lunge 0.12–0.25s, hit 0.1–0.2s, death ≤ LifeSpan) |
| Affected parts | Named parts / flags from profile |
| Local-only | Yes |
| Interrupt | Newer attack overrides in-flight attack; death cancels attack/hit; Attack→Move does not require stop of already-playing short cosmetic |
| Stacking | One attack + one hit channel; death wins |
| Target death mid-anim | Finish or shorten; never touch gameplay FinishAttack |
| Actor Destroy | Visual component tears down with actor; no orphan ticks |

---

## Projectile architecture

Cosmetic only. **Never** applies damage. **No** replication. **No** gameplay collision.

### Type

`AGP_PrimitiveProjectileVisual` (pooled actor) **or** pooled component spawned under a local subsystem — prefer **pooled short-lived actors** with no collision for clear lifetime.

### Rules

- Spawned locally from Fire/Impact presentation (see timing)
- Path Source → Target (or snapshot impact location)
- If Target dies: continue to last known location then impact cosmetic
- Bounded lifetime; pool return
- Dedicated server: **do not** spawn
- Types: Bullet (small fast sphere/cyl), Shell (larger slower), Energy (bright team-tint primitive), optional mathematical Arc

### Timing decision (locked)

S26A Impact arrives **after** authoritative hit → a projectile spawned on Impact looks late.

| Slice | Behavior |
| --- | --- |
| **GP-S26B1 / B2** | Melee + reactive Impact cosmetics only (lunge/hit/death). **No** travel projectile demo |
| **GP-S26C** | Two-phase: `AttackStarted` / `AttackFired` cosmetic emit + Impact event; local projectile flies on Fire; damage remains cadence-driven; projectile does **not** define damage time; AnimNotify never damages |

**Why S26C (not B2):** B2 must validate melee visual on existing Impact channel without expanding authority emit surface. Ranged travel needs a new cosmetic Fire event and pooling/scale validation — separate controlled slice.

---

## Team-color strategy

| Approach | Notes |
| --- | --- |
| Dynamic Material Instance on Engine basic material | Preferred **if** Engine shape materials accept scalar/vector params at runtime |
| Custom Primitive Data | Good if base material supports CPD; fewer DMI instances |
| Per-component material override | Works; watch DMI count |
| Emissive | Only if base material exposes it safely |
| Selection | Separate from team color (existing selection UX / future outline) |

### Split of work

1. **C++ infrastructure:** team id → color map; apply to parts with `TeamColorMode`; selection distinct.
2. **Editor/operator step (optional):** one minimal project material (soft-ref) if Engine defaults cannot recolor reliably — **not** claimed as auto-created by C++ alone.
3. **Fallback without material:** distinct silhouette / facing marker scale / part layout per team is insufficient alone; prefer simple vertex-color or unlit param if available, else **debug-draw team glyph** / log until material asset exists.

Do not block B1 on perfect materials — ship silhouette + facing + Inspect; escalate material as operator step.

---

## Selection, collision, gameplay boundaries

| Rule | Detail |
| --- | --- |
| Collision | Capsule/root remains gameplay; visual parts NoCollision |
| Selection | Must not depend on visual part collision (Visibility on capsule stays) |
| Attack range | Independent of visual scale |
| Facing | Visual facing/turret yaw is cosmetic unless a later task explicitly syncs gameplay rotation |
| Tick | No permanent tick on all units; enable only for active cosmetics / optional move bob with strict budget |
| Replication | Do **not** replicate transient animation transforms |
| Client build | Same archetype identity → same part recipe |
| Dedicated | Do not spawn render parts / suppress play; respect UE component lifecycle (component may exist stubbed without mesh create) |

---

## Identity / profile assignment (MVP)

| Option | MVP use |
| --- | --- |
| Replicated `EGP_VisualArchetype` enum | **Primary minimal** — small, identical on server/client, no Content dependency |
| Soft DataAsset profile override | Optional on class defaults / future UnitDefinition |
| Class defaults | `AGP_Unit` defaults → Infantry_Melee |
| Unit definition / DataTable | Future when entity catalog exists |
| Blueprint defaults | Optional later |

**Chosen MVP:** replicated (or CDO-identical) **`EGP_VisualArchetype`** + resolve to built-in default profile struct / soft DA. Gameplay combat code never reads visual archetype for damage/range. Buildings/monsters share `UGP_UnitVisualComponent` (or thin subclass) with different archetypes.

Future real art: swap profile resolution to skeletal/mesh profile **without** changing Attack/GAS code.

---

## Roadmap slices

### GP-S26B1 — Primitive visual foundation
- `UGP_UnitVisualComponent` + part schema + profile types/enums
- Build infantry prototype on current `AGP_Unit`
- Facing marker; team differentiation fallback
- No combat animation yet
- Debug/Inspect; listen + client composition parity
- Collision/selection unchanged

### GP-S26B2 — Primitive combat presentation
- Forward S26A accepted Impact → visual component
- Melee lunge, hit pulse, blocked reaction, killing/death reaction
- Barrel recoil where profile marks weapon/turret (if parts exist)
- Keep debug fallback
- **No** gameplay / cadence / payload required changes (unless BlueprintType-only)

### GP-S26B3 — Archetype catalog
- Profiles for ranged/heavy/worker/tank/artillery/turret/monsters/buildings/resource node
- Assignment/preview matrix; silhouette readability from RTS camera
- Still Engine primitives only

### GP-S26C — Two-phase ranged presentation
- AttackFired/Started cosmetic event
- Cosmetic projectile + Impact event
- No damage-from-projectile; pooling; scale validation

---

## Scalability

| Concern | Rule |
| --- | --- |
| Parts | Cap per archetype (≤12); disable collision |
| Materials | Prefer shared DMI/CPD per team over per-part unique materials |
| Tick | Idle = no tick; active cosmetic only |
| Projectiles | Pooled; bounded live count; no replication |
| RPC | Unreliable cosmetic only (S26A/S26C); no reliable spam |
| Anim state | Local only |
| Server | Dedicated: no mesh build / no projectile spawn |
| Listen | One Play path (existing S26A rule) |

### Rough targets (design budgets, not benchmarks yet)

| Visible units | Expectation |
| --- | --- |
| 100 | Comfortable with bob optional |
| 500 | Disable move bob; keep short attack/hit cosmetics; pool projectiles |
| 1000 | Strict tick budget; minimal parts profiles; no per-frame logs |

Operator scale validation in later slices (100 / 500 + Inspect tick counts).

---

## Validation matrices (future implementation)

### Foundation (B1)
Host/client identical composition; team distinction; facing; movement; selection; collision unchanged; death cleanup of parts.

### Combat (B2)
Melee lunge; normal/blocked/killing; host single play; remote client; Attack→Move; target destroyed mid-presentation.

### Catalog (B3)
≥1 instance per archetype; silhouettes distinct from RTS camera; turret/barrel orientation; buildings non-mobile; monsters distinct; resource node readable.

### Scale
100 / 500 units; Inspect component/tick counts; no per-frame log spam; Shipping build.

---

## Rejected approaches

| Approach | Why |
| --- | --- |
| Wait for authored art before MVP viz | Blocks playable readable RTS |
| Skeletal migration in Primitive MVP | No skeletons/anims; high risk |
| GameplayCue as primary | Parallel channel; no cue assets; RTS spam risk |
| Projectile-driven damage | Breaks S25 authority |
| Hard refs from Attack/GAS to meshes | Couples gameplay to viz |
| Permanent tick on all units | RTS scale failure |
| Replicated transform animation | Bandwidth + listen double complexity |
| Reliable cosmetic RPC | Stale backlog (S26A lesson) |

---

## Stop condition (this analysis revision)

- Document renamed/rewritten to Primitive Visual MVP Architecture
- AI log + Cursor report updated
- Push `feature/gp-s26b-combat-assets-analysis`
- **C++ diff: none. Assets diff: none. Build: not required.**
- Do **not** start implementation or create assets without explicit task.
