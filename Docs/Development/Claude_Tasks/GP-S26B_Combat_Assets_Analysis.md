# GP-S26B Combat Assets Analysis

## Status
**GP-S26B_ANALYSIS_READY_FOR_REVIEW**

Analysis / design only. No production C++. No Blueprint or imported assets. No gameplay changes.

## Main baseline
`main` @ `80251125bbf03566edb4ec902f8770ee900d9bde`

Depends on:
- GP-S26A cosmetic presentation channel — **GP-S26A_DONE_PRESENTATION_ASSETS_DEFERRED**
- Source doc: `Docs/Development/Claude_Tasks/GP-S26_Combat_Presentation.md`

Branch: `feature/gp-s26b-combat-assets-analysis`

---

## 1. Existing GP-S26A architecture

### Channel ownership

| Piece | Location | Role |
| --- | --- | --- |
| `FGP_CombatPresentationEvent` | `GPRuntime/Public/Combat/GPCombatPresentationTypes.h` | Transient cosmetic payload |
| `EGP_CombatPresentationEventType` | Same | S26A: `MeleeImpact` only |
| `UGP_CombatPresentationComponent` | GPRuntime Combat | Replicated default subobject; Unreliable NetMulticast; Sequence dedupe |
| Emit | `UGP_UnitCommandComponent::AttemptAttackHit` | After successful `ApplyDamageFromUnit` (incl. blocked); snapshot for sync TargetDied |
| Receive | `Multicast_CombatPresentationEvent` → `HandleCombatPresentationEvent` | Sole local Play entry |
| Debug viz | `PlayCombatPresentationDebug` | Log + non-shipping Source→Target `DrawDebugLine` |
| Unit composition | `AGP_UnitBase` | Command + Presentation + ASC + UnitAttributeSet |
| Concrete mesh | `AGP_Unit` | Capsule root + `UStaticMeshComponent` VisualMesh (Engine Cylinder) |
| Death | `HandleDeathInternal` / `OnRep_IsDead` | Collision disable + LifeSpan; **no** death animation |
| Modules | `GPUIRuntime → GPRuntime → GPGASRuntime` | Presentation lives in GPRuntime; no GPGAS visual dependency |

### Payload available on accepted event

| Field | Available | Notes |
| --- | --- | --- |
| PresentationSequence | Yes | Dedupe / diagnostics |
| AttackSerial | Yes | Ties to Attack executor serial |
| Source | Yes (derived) | Component owner |
| Target | Yes | May be pending-kill / dead |
| EventType | Yes | `MeleeImpact` |
| AuthoritativeWorldTime | Yes (`float`) | Debug/order aid; not clock sync |
| AppliedDamage | Yes | Post-MMC applied magnitude |
| bBlocked | Yes | Applied with zero damage |
| bTargetDiedFromHit | Yes | Death observed for this hit |

### Data absent for richer presentation

| Need | Status |
| --- | --- |
| Impact world location / socket | Absent |
| Attack direction / facing | Absent (infer from actors) |
| Weapon / projectile id | Absent |
| Variant / unit visual profile id | Absent |
| Wind-up / AttackStarted cosmetic phase | Absent (impact-only channel) |
| Montage / Niagara / sound soft refs on units | Absent |
| Skeletal mesh / AnimInstance | Absent |
| Hit reaction vs death montage selection beyond bools | Only `bBlocked` / `bTargetDiedFromHit` |

Constraints preserved: unreliable multicast; no LastEvent; no late-join replay; dedicated suppresses visuals; gameplay cadence independent of presentation.

---

## 2. Verified asset inventory

### Tracked Content packages (complete list — 10 files)

All confirmed via `git ls-files` and filesystem under `GP/Content`. **All are Enhanced Input.** None are combat/visual presentation assets.

| Path | Type | Used by | Combat usable? |
| --- | --- | --- | --- |
| `GP/Content/GrimProtocol/Input/Camera/IA_Camera_Pan.uasset` | Input Action | `AGP_PlayerController` soft ref | No |
| `GP/Content/GrimProtocol/Input/Camera/IA_Camera_Zoom.uasset` | Input Action | same | No |
| `GP/Content/GrimProtocol/Input/Camera/IA_Camera_Rotate.uasset` | Input Action | same | No |
| `GP/Content/GrimProtocol/Input/Camera/IA_Camera_RotateToggle.uasset` | Input Action | same | No |
| `GP/Content/GrimProtocol/Input/Camera/IMC_GP_Camera.uasset` | Input Mapping Context | same | No |
| `GP/Content/GrimProtocol/Input/Commands/IA_Command.uasset` | Input Action | same | No |
| `GP/Content/GrimProtocol/Input/Commands/IMC_GP_Commands.uasset` | Input Mapping Context | same | No |
| `GP/Content/GrimProtocol/Input/Selection/IA_Select.uasset` | Input Action | same | No |
| `GP/Content/GrimProtocol/Input/Selection/IA_ControlGroup.uasset` | Input Action | same | No |
| `GP/Content/GrimProtocol/Input/Selection/IMC_GP_Selection.uasset` | Input Mapping Context | same | No |

Reference style for Input: `TSoftObjectPtr` + `FSoftObjectPath` in `GPPlayerController.cpp` (soft). Multiplayer: local input only.

### Engine (non-repo) mesh used by units

| Path | Type | Used by | Notes |
| --- | --- | --- | --- |
| `/Engine/BasicShapes/Cylinder.Cylinder` | Engine Static Mesh | `AGP_Unit` via `ConstructorHelpers::FObjectFinder` | **Hard** constructor load; placeholder only; **not** melee/impact/death content |

### Confirmed absent in repository (evidence)

| Category | Evidence |
| --- | --- |
| Skeletal Mesh / Skeleton | 0 Content packages; 0 `USkeletalMeshComponent` in production C++ |
| Anim Blueprint / Sequence / Montage / Blend Space | 0 packages; 0 Anim* types in production C++ |
| Niagara / Cascade | 0 packages; 0 `UNiagaraSystem` / particle types in production C++ |
| Sound Cue / Wave / MetaSound | 0 packages; 0 audio play APIs in combat path |
| Projectile BP/C++ presentation actors | 0 |
| Impact / death / hit-reaction assets | 0 |
| Weapon meshes / sockets authored in project | 0 (no skeletal) |
| Unit Blueprint classes | 0 `.uasset` BP; native `Blueprintable` types only |
| Visual DataAssets / DataTables for units | 0 instances (`UGP_CameraConfigDataAsset` class exists; no `.uasset` instances) |
| Maps | 0 `.umap`; default map is Engine OpenWorld template |
| Imported media (fbx/png/wav/…) | 0 tracked |

### Rejected assumptions

- Do **not** assume montages/Niagara exist because docs mention them as deferred.
- Do **not** treat Engine Cylinder as a combat presentation asset.
- Do **not** treat `Blueprintable` on `AGP_Unit` as proof a unit Blueprint asset exists.
- Do **not** invent placeholder art for S26B scope.

**Conclusion:** There are **no** repository assets suitable for melee attack / impact / death presentation in GP-S26B. Real visuals require a **separate asset/import stage**.

---

## 3. Current unit visual architecture

| Question | Answer |
| --- | --- |
| Mesh on `AGP_UnitBase`? | **Neither** — Base has no mesh |
| Mesh on concrete unit? | `AGP_Unit`: **`UStaticMeshComponent`** `VisualMesh` |
| Mesh created where? | **C++** constructor (`CreateDefaultSubobject`) |
| AnimInstance? | **No** |
| Base unit Blueprint? | **No** asset |
| Play montage without architecture change? | **No** — requires `USkeletalMeshComponent` + skeleton + AnimInstance/montage |
| Static → skeletal migration required for real anim? | **Yes**, for montage-based attack/death |
| Risk to selection/collision/health/replication? | Controllable if skeletal replaces/attaches as non-collision visual under existing capsule root; keep capsule as selection/collision authority; do **not** move ASC/command/replication onto mesh |
| Presentation root today | Capsule (`RootComponent`) for transforms; `VisualMesh` for placeholder look; combat debug draws actor locations |
| Recommended future presentation root | Capsule remains gameplay root; skeletal (or visual) component attached as presentation child; sockets live on skeletal mesh |

Death lifecycle today is presentation-agnostic (collision + LifeSpan). Adding death montage later must remain cosmetic and must not delay `bIsDead` / Destroy policy.

---

## 4. Options compared

### A — Delegates on `UGP_CombatPresentationComponent`; unit BP subscribes

| Criterion | Assessment |
| --- | --- |
| Modules | GPRuntime only |
| BP coupling | High when unit BPs exist; **no unit BPs today** |
| Replication | Reuses existing Unreliable multicast; no new RPCs |
| RTS scale | Good (one event → local play) |
| Dedicated | Gate before Play / in subscribers |
| Asset loading | Deferred to BP graphs |
| Data-driven | Weak unless BPs encode profiles |
| Risks | Logic scattered across BPs; hard to enforce dedicated/missing-asset policy |
| Cost | Low C++ if BPs exist; **blocked on missing BPs/assets now** |
| Fit | Good later; weak as sole S26B given empty Content |

### B — Data-driven C++ component with soft refs (montage/Niagara/sound)

| Criterion | Assessment |
| --- | --- |
| Modules | GPRuntime (+ optional soft Niagara/Audio module deps when used) |
| BP coupling | Low |
| Replication | Reuse S26A channel |
| RTS scale | Good if soft + async load discipline |
| Dedicated | Early-out before load/play |
| Asset loading | Soft refs; missing = no-op |
| Data-driven | Strong via DataAsset profiles |
| Risks | Empty refs today; tempting hard refs; skeletal still required for montages |
| Cost | Medium |
| Fit | Strong **profile shell** even before art lands |

### C — GameplayCue via GAS

| Criterion | Assessment |
| --- | --- |
| Modules | Pulls presentation toward GPGASRuntime / cue infrastructure |
| BP coupling | Cue BPs (none exist) |
| Replication | Cue replication model differs from current Unreliable cosmetic channel |
| RTS scale | Cue spam / filtering needs care for many units |
| Dedicated | Cues can be configured off on DS; still extra surface |
| Asset loading | Cue assets required |
| Data-driven | Good in mature GAS games |
| Risks | Parallel presentation path; duplicates S26A channel; no cue assets |
| Cost | High relative to benefit now |
| Fit | **Rejected for S26B** |

### D — `UGP_UnitVisualComponent` consumes presentation events

| Criterion | Assessment |
| --- | --- |
| Modules | GPRuntime |
| BP coupling | Optional BP subclass/overrides; C++ owns policy |
| Replication | No new presentation RPCs; listens to S26A accept |
| RTS scale | Good; per-unit profile |
| Dedicated | Component early-out |
| Asset loading | Soft refs / profile DA |
| Data-driven | Strong when combined with B-style profile |
| Risks | Extra component on Base; must not become second emit path |
| Cost | Medium |
| Fit | **Best ownership split** with current native unit architecture |

---

## 5. Recommended architecture

### Primary: **D + B (shell)** — `UGP_UnitVisualComponent` + soft-ref presentation profile

```text
[S26A unchanged]
  Authority Apply → Unreliable Multicast → Handle (dedupe / DS gate)

[S26B add]
  After accept (non-DS): notify visual sink
    → UGP_UnitVisualComponent::HandleAcceptedPresentation(Event)
        → resolve UGP_UnitCombatPresentationProfile (soft)
        → try play montage / Niagara / sound soft refs
        → on missing: no-op + keep PlayCombatPresentationDebug fallback
```

Policies:
- No new per-asset RPCs.
- No AnimNotify → damage.
- No damage delay for animation.
- Soft refs only from visual/profile layer (not CommandComponent / GAS damage path).
- Different unit types → different profile DataAssets (or component defaults).
- Blocked / normal / killing hit branch on existing bools + EventType.
- Melee now; EventType enum already reserved for ranged/spell later.

### Fallback: **A**

If/when concrete unit Blueprints become the authoring surface, expose `BlueprintAssignable` on the presentation or visual component so BP graphs can play assets without a second network channel. Still keep C++ dedicated/missing-asset guards.

### Rejected for S26B
- **C (GameplayCue)** as primary path.

---

## 6. Minimal GP-S26B implementation slice (design only)

**Nature:** architecture-first + hooks. **Not** a polished art slice — verified inventory has zero combat assets.

### C++ (proposed future implementation)

| Change | Purpose |
| --- | --- |
| `UGP_UnitVisualComponent` (new, GPRuntime) | Default subobject on `AGP_UnitBase` or `AGP_Unit`; consumes accepted events; NetMode gate; soft profile resolve |
| `UGP_UnitCombatPresentationProfile` (`UPrimaryDataAsset`, new) | Soft slots: AttackImpactMontage, BlockedFX, KillFX, ImpactNiagara, ImpactSound (all optional/empty) |
| `UGP_CombatPresentationComponent` | After accept (non-DS): broadcast native multicast delegate / call visual component; keep debug fallback |
| `FGP_CombatPresentationEvent` / enum | Mark `BlueprintType` for future BP hooks (optional in same slice) |
| `AGP_Unit` / Base | Attach visual component; **do not** migrate to skeletal in S26B unless assets arrive |

### Delegates / interfaces

- Native `DECLARE_MULTICAST_DELEGATE_OneParam(FGP_OnCombatPresentationAccepted, const FGP_CombatPresentationEvent&)` on presentation component (and/or BlueprintAssignable wrapper).
- Optional lightweight interface `IGP_CombatPresentationSink` if multiple consumers needed — not required if visual component is sole sink.

### Blueprint / Content assets usable now

**None** for combat presentation.

### DataAsset

Yes — **empty profile class + optional empty instance** only if implementation stage explicitly creates it. Analysis forbids creating assets now. Implementation may ship C++ DA type with null soft refs without importing art.

### Demo presentation for S26B implementation (realistic)

1. Keep debug line + logs as guaranteed fallback.
2. Prove visual component receives accepted events for normal / blocked / kill.
3. Prove soft-ref resolve path logs “missing asset” without crash.
4. Prove dedicated does not enter play path.
5. **Do not** claim montage/Niagara demo until import stage provides files.

### Asset resolution / missing asset

- Soft load at play time (or match preload via existing `UGP_MatchAssetLoader` later).
- Null / failed load → skip that channel; do not block others; retain debug fallback.
- Never hard-ref montages from CommandComponent.

### Payload changes for S26B

| Change | Recommendation |
| --- | --- |
| Required for S26B architecture-first | **None** — current payload sufficient |
| Optional later (S26C / art stage) | ImpactLocation, optional ProfileTag, AttackStarted/Windup EventTypes |
| BlueprintType on struct/enum | Recommended when adding BP hooks |

### Debug fallback

- Keep `PlayCombatPresentationDebug` (log + non-shipping line) until real assets exist and optionally behind cvar afterward.

### Explicit non-goals of S26B slice

- Importing skeletal/anim/Niagara/audio.
- Migrating `AGP_Unit` to skeletal mesh (unless assets ready in a later task).
- Two-phase AttackStarted emit (see Timing → S26C).
- GameplayCue path.
- Changing S25 cadence / damage / TargetDied.

### Follow-on asset/import stage (separate task)

Required before “real” melee viz:
- Unit skeletal mesh + skeleton + AnimBP (or montage player).
- Attack / blocked / death montages or Niagara.
- Optional unit Blueprints or profile DataAsset instances.
- Capsule-root migration plan for visual component swap.

---

## 7. Timing model

### Current (S26A — keep for S26B)

```text
Authoritative hit (Apply) → cosmetic Impact event → reactive presentation
```

- Gameplay hit already occurred.
- Presentation may look slightly “late” vs fantasy wind-up.
- Acceptable for RTS with placeholder/debug and for architecture-first S26B.
- **Must not** delay Apply to wait for animation.

### Perceived lateness

If attack montage starts only on Impact, the swing appears after damage. Mitigations without breaking authority:

| Approach | When |
| --- | --- |
| Short client-only anticipation (cosmetic lerp) | Optional later; does not move NextHitTime |
| Two-phase cosmetic channel | **GP-S26C** |

### Two-phase model (recommended deferred to **GP-S26C**)

```text
Phase 1: AttackStarted / Windup cosmetic (optional emit when entering Ready or on cadence arm)
Phase 2: AttackImpact cosmetic (existing post-Apply event)
Damage remains authoritative + cadence-driven
AnimNotify never applies damage
```

**Do not expand S26B** to two-phase now: no anim assets, would require new authority emit points, and increases validation surface. S26B stays Impact-reactive + visual sink architecture.

---

## 8. Operator validation plan (future S26B implementation)

| ID | Case | Expect |
| --- | --- | --- |
| B1 | Listen host normal hit | One accept → visual sink + debug fallback; no double play |
| B2 | Remote client normal hit | Same Sequence/Serial; local play once |
| B3 | Blocked hit | Blocked branch / missing FX no-op; cooldown unchanged |
| B4 | Killing hit | Kill branch; TargetDied gameplay unchanged |
| B5 | Missing asset soft refs | No crash; debug fallback remains |
| B6 | Two unit visual profiles | Different profile soft refs resolve independently (when instances exist) |
| B7 | Attack→Move | No further events; no orphan hard loads |
| B8 | Target death during presentation | Safe Target ptr; no gameplay change |
| B9 | Actor relevancy | No LastEvent replay (S26A semantics) |
| B10 | Dedicated server | No visual/audio/Niagara play |
| B11 | Late join | No past cosmetics |
| B12 | Shipping | Debug draw compiled out; no shipping asset hard deps |

Until assets exist, B6 may be **NOT RUN** or validated via two empty profiles with distinct log tags only.

---

## 9. Risks

| Risk | Mitigation |
| --- | --- |
| Implementing montage play without meshes | Forbid; architecture-first only |
| Hard refs in core gameplay | Soft refs only on visual/profile |
| Second RPC channel | Forbidden; reuse S26A multicast |
| AnimNotify damage creep | Explicit non-goal |
| Skeletal migration breaks selection | Keep capsule root + query collision |
| Scope creep into S26C two-phase | Documented deferral |

---

## 10. Build / stop condition (this analysis branch)

| Item | Status |
| --- | --- |
| C++ diff | **none** |
| Assets diff | **none** |
| Build | **not required** |
| Commit/push | `feature/gp-s26b-combat-assets-analysis` |
| Merge / PR / implementation | **Do not** |

**Do not** start GP-S26B implementation or asset import without an explicit task.
