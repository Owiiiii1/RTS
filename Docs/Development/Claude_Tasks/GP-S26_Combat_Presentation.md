# GP-S26 Combat Presentation

## Status
**GP-S26_ANALYSIS_READY_FOR_REVIEW**

Analysis / design only. No production C++. No Blueprint assets. No combat-semantics changes.

## Main baseline
`main` @ `b511cf5008546cc421971cd4612cbd92c1a8b945` — Merge GP-S25B attack cadence integration

Depends on:
- GP-S24 Attack executor (`DONE_WITH_DAMAGE_EXECUTION_DEFERRED`)
- GP-S25A/B Health/Damage/Cadence (`DONE_WITH_VISUAL_COMBAT_DEFERRED`)

Branch: `feature/gp-s26-combat-presentation-analysis`

---

## Existing architecture findings

### Source of truth (gameplay — unchanged)

| System | Owner | Notes |
| --- | --- | --- |
| Attack Idle / Approaching / Ready | `UGP_UnitCommandComponent` | Plain C++ enums; **not replicated** |
| Immediate first hit + world-time cadence | Same | `bHasAttemptedFirstHit`, `NextAttackHitTime` — server-only |
| Damage | `AGP_UnitBase::ApplyDamageFromUnit` → native `UGP_GE_Damage_Basic` + MMC | Authority-only |
| Death | `HandleDeathInternal` + `bIsDead` OnRep | `OnUnitDied` native multicast, **authority-only** |
| Effective range | GAS AttackRange if >0 else component | Server resolver |
| Hysteresis / RangeUnreachable / SelfSupersede | Command component | Server-only |

Header contract today: *“No replication, RPC, queue execution, or Blueprint Attack API.”*

### Presentation layer inventory

| Item | State |
| --- | --- |
| SkeletalMesh / AnimInstance / Montage | **Absent** (`AGP_Unit` uses Engine cylinder `UStaticMeshComponent`) |
| Niagara / Sound / GameplayCue | **Absent** |
| NetMulticast / cosmetic RPC | **Absent** (only command `Server_RequestCommand` RPC) |
| Replicated Attack state / serial / target | **Absent** |
| Combat presentation component / subsystem | **Absent** |
| Client death hook | `ApplyClientDeadPresentation()` — **collision disable only** |
| Tags `GP.Unit.State.Attacking` / `AttackCooldown` | Registered; **unused** by executor |
| Content combat assets | **None** (Content has Enhanced Input only) |

### Replication surfaces available to clients today

| Data | Available remotely? |
| --- | --- |
| Actor transform / rotation | Yes (`SetReplicateMovement`) |
| `TeamId`, `bIsDead` | Yes + OnRep |
| Unit GAS attributes (Health, Damage, Cooldown, Range, …) | Yes (ASC Mixed) |
| Held command / AttackState / ActiveAttackSerial / AttackTarget | **No** |
| Movement serial / destination / bIsMoving | **No** (component non-replicated) |
| AttackHitAttempt / AttackHitApplied identity | **No** (server logs only) |
| Per-hit attacker identity / blocked flag / presentation sequence | **No** |

Clients can infer movement and Health loss / death, but **cannot** distinguish Attack approach vs idle, an authoritative hit vs other damage, or blocked damage as a combat beat.

### Module hierarchy

```text
GPUIRuntime → GPRuntime → GPGASRuntime
```

- `GPUIRuntime` is an empty module shell (also depends directly on GPGASRuntime).
- Presentation emit can live in `GPRuntime` next to the executor.
- Visual consumers / future UI may live in `GPUIRuntime` without cycles.
- Do **not** pull unit types into `GPGASRuntime`.

### Listen-server / dedicated implications (current)

- All hit/cadence/damage paths gate on `HasAuthority()` → no duplicate **gameplay** execution.
- Listen host is authority **and** local viewport: any future visual bound both to the gameplay hit path **and** a replicated/multicast presentation path will **double-play** unless presentation has a single receive policy.
- Dedicated server has no viewport; visual load/play must early-out (`NM_DedicatedServer`).

### Can presentation subscribe to AttackHitAttempt / AttackHitApplied?

| Hook | Usable as emit trigger? |
| --- | --- |
| `AttackHitAttempt` (pre-GE) | Risky — emit before Apply; incomplete metadata; may diverge if Apply aborts |
| `AttackHitApplied` (post-GE) | **Yes** — has Applied/Blocked/Health/Death; cadence already decided |
| Separate `AttackRelease` before damage | Requires wind-up redesign; **not** for S26A |

Safe approach: **side-effect emit after successful hit processing**, never gating damage/cadence on presentation.

---

## Current gaps

1. No cosmetic combat event channel to clients.
2. No AttackSerial / presentation sequence visible remotely.
3. No melee / ranged / projectile presentation taxonomy.
4. No wind-up / release visual model (and none should gate gameplay).
5. No client-side duplicate suppression infrastructure.
6. No dedicated-server visual skip policy beyond implicit absence of assets.
7. Death presentation is collision-only; no hit reaction hook.
8. Observing Health alone is ambiguous for attack VFX (multi-attacker, blocked, external damage).

---

## Constraints

| Constraint | Requirement |
| --- | --- |
| Authority | Gameplay remains server-authoritative |
| Cadence independence | Damage timing must **not** depend on AnimNotify / montage length |
| Presentation optional | Missing/late visuals must not change damage, cooldown, or FinishAttack |
| Dedicated server | Must not load/execute visual logic (meshes/Niagara/audio play) |
| Listen server | Must not play the same presentation event twice |
| Clients | Must receive enough data to visualize a hit beat |
| Extensibility | Melee, instant ranged, projectile ranged, future variants |
| Assets | No core runtime hard-dependency on specific Blueprint / montage / Niagara assets |
| Stack | No Lyra / CommonGame |
| Modules | Respect `GPUIRuntime → GPRuntime → GPGASRuntime` |
| S25 semantics | Do not alter immediate-first-hit, world-time cadence, hysteresis, TargetDied, range hierarchy |

---

## Options considered

### Option A — Replicated combat presentation event

Server records an authoritative **cosmetic** attack-release/hit event after gameplay Apply. Clients receive a multicast and/or replicated last-event payload and run local presentation.

| Criterion | Assessment |
| --- | --- |
| Reliability | High if reliable multicast or replicated+OnRep; event is explicit |
| Late join | Multicast-only misses history; last-event OnRep recovers “most recent” only |
| Packet loss | Reliable channel recovers; unreliable may drop cosmetic beats (acceptable) |
| Duplicate suppression | Strong via monotonic `PresentationSequence` per source |
| Listen server | Safe **if** visuals play only on presentation receive path, never also inline in `AttemptAttackHit` |
| Bandwidth | Low — compact struct per hit |
| Coupling | Low — emit is fire-and-forget side effect; gameplay does not wait |
| Debugging | Excellent — sequence + serial + metadata in logs |
| Extensibility | EventType / variant tag grows cleanly |

### Option B — Replicated Attack state + client observation

Replicate AttackState / serial / target / timestamps; clients derive visuals from transitions and Health changes.

| Criterion | Assessment |
| --- | --- |
| Reliability | Medium — state converges, but “hit beat” is inferred |
| Late join | Better for continuous state (Approaching/Ready) |
| Packet loss | State eventually consistent; transient hits may be missed without event |
| Duplicate suppression | Harder for discrete VFX (must synthesize edge triggers) |
| Listen server | OnRep + local authority mutation can double-fire without careful gating |
| Bandwidth | Higher if ticking state; moderate if OnRep-only fields |
| Coupling | Higher — presentation tied to executor state shape |
| Debugging | Ambiguous hit vs blocked vs external damage |
| Extensibility | Weak for projectile / multi-hit variants without extra events |

### Recommendation — **Hybrid (A primary, B deferred)**

**S26A implements Option A** (authoritative cosmetic presentation event).

Optionally later (S26B+): light replicated Attack intent flags (e.g. Approaching/Ready) for UI/selection feedback — **not** required for hit VFX and **not** allowed to drive damage.

Rationale:
- Hit identity and blocked/death metadata need an explicit event (A).
- Continuous Attack state replication (B) is useful for chrome, not for reliable hit beats.
- Keeps S25 cadence untouched: emit after Apply, never wait on presentation.

---

## Recommended architecture

```text
[Authority] AttemptAttackHit
    → AttackHitAttempt log
    → ApplyDamageFromUnit (gameplay; unchanged)
    → schedule NextHitTime (unchanged)
    → AttackHitApplied log
    → EmitCombatPresentationEvent(payload)   // NEW side effect only

[Net] Multicast and/or Replicated LastEvent + PresentationSequence

[Local worlds with viewport]
    UGP_CombatPresentationComponent / subsystem
      → dedupe by PresentationSequence
      → if NM_DedicatedServer: return
      → debug draw / log (S26A)
      → future: montage / VFX / cosmetic projectile (assets optional)
```

### Ownership

| Layer | Module | Responsibility |
| --- | --- | --- |
| Emit after Apply | `GPRuntime` (`UGP_UnitCommandComponent`) | Build payload; call presentation sink; **never** await it |
| Presentation sink / component | `GPRuntime` (generic) | Replication receive, dedupe, NetMode gate, debug viz |
| Future rich UI / widgets | `GPUIRuntime` | Optional consumers of the same event API |
| GAS / damage formula | `GPGASRuntime` | Unchanged; no presentation includes |

### Policies

1. **Single play path:** Visuals execute only inside presentation receive handlers.
2. **Dedicated server:** Early-out before any mesh/Niagara/audio/debug-draw that implies a viewport (logs may remain Verbose/category-gated).
3. **Listen server:** Receives the same multicast/OnRep once; must not also call Play from `AttemptAttackHit`.
4. **No AnimNotify → damage:** Forbidden forever for GP combat cadence.
5. **Soft asset refs:** SoftObjectPath / TSoftObjectPtr / DataAsset optional; missing asset = silent no-op visual.

---

## Data model

```text
FGP_CombatPresentationEvent
  PresentationSequence : uint32   // monotonic per source unit; 0 = invalid
  AttackSerial         : uint32   // matches executor ActiveAttackSerial
  Source               : AGP_UnitBase* (owner implied if component-owned)
  Target               : AGP_UnitBase* (may be pending-kill / dead)
  EventType            : EGP_CombatPresentationEventType
  AuthoritativeWorldTime : double // server GetTimeSeconds at emit
  AppliedDamage        : float
  bBlocked             : bool     // Applied path ran but FinalDamage == 0
  bTargetDiedFromHit   : bool     // death observed for this hit apply
  // Future (not S26A required):
  // VariantTag, ImpactLocation, ProjectileId, WindUpSeconds
```

```text
EGP_CombatPresentationEventType (S26A minimal)
  MeleeImpact      // default for current Attack executor hits
  // Deferred:
  // RangedInstant, ProjectileLaunch, ProjectileImpact, HitReaction, AttackCancel
```

Duplicate key: `(SourceUnitNetId or Object*, PresentationSequence)`.

---

## Replication model

### S26A recommended transport

**NetMulticast reliable** on `UGP_CombatPresentationComponent` (or UnitBase):

- Payload = `FGP_CombatPresentationEvent` (or mirrored args).
- Client/listen handler: dedupe → NetMode gate → debug presentation.
- Dedicated server: multicast stub returns immediately (no visual work).

**Why not only OnRep last-event for S26A:**
- OnRep is fine for late-join “last hit” and bandwidth, but multicast matches RTS hit-beat immediacy with less inference.

**Optional hybrid add-on (same slice if cheap):**
- Also store `LastPresentationEvent` + `PresentationSequence` as replicated properties for late join / relevancy catch-up of the **latest** beat only.
- Not required to close S26A if operator accepts “late join misses past cosmetics.”

### Suppression rules

| Case | Behavior |
| --- | --- |
| Sequence ≤ last processed | Ignore |
| Dedicated server | No visual execution |
| Listen host | Exactly one play via multicast/OnRep path |
| Replay / re-possess | Sequence still monotonic — no replay of old sequences |

### Bandwidth

One compact event per authoritative hit (~tens of bytes + actor refs). Compatible with current RTS scale for S26A.

---

## Temporal model

### Authoritative cosmetic moment (locked for S26A)

**Post-`AttackHitApplied` emit** — after GE Apply returns and AttackHitApplied metadata is known; **before or after** NextHitTime schedule is irrelevant as long as emit does not alter schedule.

| Moment | Role |
| --- | --- |
| `AttackHitAttempt` | Gameplay-only log; **do not** drive presentation emit |
| ApplyDamageFromUnit | Authoritative damage / death |
| `AttackHitApplied` + emit | Authoritative **cosmetic release / impact** marker |
| Separate pre-damage `AttackRelease` | Deferred (wind-up era); would require cosmetic-only timeline |

### Wind-up / animation start

| Topic | S26A | Later |
| --- | --- | --- |
| Gameplay wind-up delaying damage | **Forbidden** | Still forbidden |
| Visual wind-up before cosmetic impact | Not required | Optional client-only lead-in that **does not** move NextHitTime |
| When animation starts | N/A (no montage) | On presentation event; may play a short anticipatory montage that finishes **after** damage already applied |

### Immediate-first-hit

- Gameplay remains immediate on first Ready (S25).
- Presentation event fires in the same authoritative hit processing step.
- Clients may see visual slightly late due to net; **acceptable**; no server wait.

### Blocked damage

- Still emit event with `bBlocked=true`, `AppliedDamage=0`.
- Cadence already schedules cooldown (unchanged).
- Presentation may use a “blocked / clank” debug color later; S26A can log + debug draw differently.

### TargetDied during presentation

- Emit may set `bTargetDiedFromHit=true`.
- Ongoing cosmetic may complete or shorten; gameplay already finished via TargetDied bind.
- Clients also see `bIsDead` OnRep for death presentation (collision today).

### Command replacement / attacker death / OOR

| Case | Presentation |
| --- | --- |
| Attack → Move / retarget | New AttackSerial; old cosmetics may finish; new events use new serial |
| Attacker death | No further emits; in-flight cosmetic optional cancel by observing `bIsDead` on source |
| Target left range (no hit) | **No** presentation event |
| Hysteresis band (Ready, no damage) | **No** event |

### Projectile visual vs authoritative damage

- Authoritative damage remains instant (S25 path) for current units.
- Future cosmetic projectile: spawn on event (`ProjectileLaunch`) and interpolate to target **after** damage already applied; impact VFX is cosmetic only.
- **No** projectile collision gameplay in S26.
- Optional later: true travel-time damage is a **different gameplay stage**, not presentation.

### Cosmetic delay

- S26A: **no** artificial server delay; optional client-only lerp for debug projectile stub only.
- Must not insert server `Delay` before Apply.

### Cadence preservation

Emit is a pure side effect. No TimerManager for presentation on the authority cadence path. No AnimNotify callbacks into CommandComponent.

---

## GP-S26A scope

Minimal vertical slice (recommended after code analysis):

| Include | Detail |
| --- | --- |
| Cosmetic presentation event emit | After AttackHitApplied (incl. blocked) |
| Payload | PresentationSequence, AttackSerial, Source, Target, EventType, AuthoritativeWorldTime, AppliedDamage, bBlocked, bTargetDiedFromHit |
| Transport | NetMulticast reliable (+ optional LastEvent replicate) |
| Consumer | One `UGP_CombatPresentationComponent` on `AGP_UnitBase` (or equivalent single sink) |
| Dedupe | Client/listen Sequence suppression |
| Dedicated | No visual execution |
| Assets | **None required** — debug draw + logs only |
| Debug | `gp.CombatPresentation.*` inspect/log verbosity (non-shipping) |
| Validation | Listen server + remote client |

### Explicitly deferred (not GP-S26A)

- Real animation montages / AnimInstance / AnimNotify
- Niagara systems, materials, sounds
- Projectile actors with collision gameplay
- Attack prediction / lag compensation
- LOS, nav combat, aggro presentation
- Ability-specific visuals / GameplayCue asset pipeline
- Polished UI (health bars, damage numbers) — may be S26B/UI track
- Full replicated AttackState machine (Option B)
- Wind-up that offsets gameplay hit time
- Changing S25 cadence, hysteresis, range, TargetDied, damage formula

---

## Deferred scope (post-S26A)

| Stage | Candidate |
| --- | --- |
| S26B | Soft-ref montage / hit react hooks; optional Approaching/Ready chrome (light B) |
| S26C | Cosmetic projectile path; instant ranged variant EventTypes |
| Later | Niagara/SFX DataAssets; UI damage numbers; death flair beyond collision |

---

## Risks

| Risk | Mitigation |
| --- | --- |
| Listen-server double play | Single receive path; never Play from AttemptAttackHit body |
| Presentation starts gating damage | Code review + tests: emit after Apply only; no waits |
| Clients infer hits from Health | Prefer event channel; Health remains secondary |
| Multicast spam / bandwidth | Compact payload; one event per hit |
| Late join misses cosmetics | Accept in S26A; optional LastEvent OnRep |
| Dedicated loads assets | Soft refs + NetMode gate; no hard constructor loads |
| Coupling to BP assets | Debug-only default; soft optional paths |
| Event before FinishAttack reentrancy | Emit after Apply with same serial guards as AttackHitApplied |
| Confusion with unused Attacking tags | Do not require tags for S26A; optional later |

---

## Implementation plan

Docs-only until explicit S26A implementation task.

Suggested implementation order (future):

1. Define `FGP_CombatPresentationEvent` + EventType enum (GPRuntime).
2. Add `UGP_CombatPresentationComponent` (replicate/multicast + dedupe + NetMode gate + debug draw).
3. Attach to `AGP_UnitBase`; no mesh/anim dependency.
4. Emit from CommandComponent **after** AttackHitApplied metadata is finalized (incl. blocked).
5. Non-shipping debug commands / log category.
6. Operator matrix (listen + client).
7. Finalization builds; still no required Content assets.

---

## Operator validation plan

| ID | Case | Expect |
| --- | --- | --- |
| P1 | Listen server host attacks | One presentation log/debug viz per hit |
| P2 | Remote client observes attack | Same Sequence/Serial/metadata; one viz |
| P3 | Melee-like immediate Attack in range | Event on first hit without gameplay delay |
| P4 | Blocked damage (Armor/Res) | Event with bBlocked; cooldown still schedules |
| P5 | Repeated cadence | Sequences increase; no dupes |
| P6 | TargetDied on hit | Event may flag death; AttackFinished TargetDied unchanged |
| P7 | Attacker dies mid-Attack | No crash; no further events |
| P8 | Attack → Move replacement | Old Attack stops; no gameplay change; cosmetics may finish |
| P9 | Duplicate suppression | Inject/replay same Sequence → ignored |
| P10 | Dedicated server | No visual execution / no asset load errors |
| P11 | Late join / relevancy (if LastEvent shipped) | Sees last event or acceptable miss if multicast-only |

Gameplay regression checks (must remain PASS): immediate hit, cadence, hysteresis, RangeUnreachable, SelfSupersede, TargetDied.

---

## Build/test plan

| When | Build |
| --- | --- |
| This analysis branch | **None** (docs only) |
| S26A implementation | GPEditor Dev + UHT; operator PIE listen+client; finalization GP Dev + Shipping |

Automated tests: none required for analysis; S26A may add a lightweight authority emit unit test if harness exists (currently no Attack automation).

---

## Stop condition

- Document `GP-S26_Combat_Presentation.md` committed.
- AI Project Log + Cursor Work Report updated.
- Branch `feature/gp-s26-combat-presentation-analysis` pushed.
- **No** C++ diff. **No** Blueprint assets. **No** merge to main. **No** PR.
- **Do not** start GP-S26A implementation without an explicit task.
