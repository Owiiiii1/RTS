# GP-S26 Combat Presentation

## Status
**GP-S26_ANALYSIS_READY_FOR_REVIEW**

Analysis / design only. No production C++. No Blueprint assets. No combat-semantics changes.

Review correction applied on top of analysis commit `d5e8b13fec5f4f21e7ed8ed7e8a51b1d73a83a5d`: transport locked to **Unreliable NetMulticast**; LastEvent / late-join replay removed from S26A; RPC ownership and payload tightened.

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
- Listen host is authority **and** local viewport: any future visual bound both to the gameplay hit path **and** a multicast presentation path will **double-play** unless presentation has a single receive policy.
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
| Presentation optional | Missing/late/dropped visuals must not change damage, cooldown, or FinishAttack |
| Dedicated server | Must not load/execute visual logic (meshes/Niagara/audio play / debug draw) |
| Listen server | Must not play the same presentation event twice |
| Clients | Must receive enough data to visualize a hit beat when the packet arrives |
| Transport | Transient cosmetics must not occupy the reliable channel |
| Extensibility | Melee, instant ranged, projectile ranged, future variants |
| Assets | No core runtime hard-dependency on specific Blueprint / montage / Niagara assets |
| Stack | No Lyra / CommonGame |
| Modules | Respect `GPUIRuntime → GPRuntime → GPGASRuntime` |
| S25 semantics | Do not alter immediate-first-hit, world-time cadence, hysteresis, TargetDied, range hierarchy |

---

## Options considered

### Option A — Explicit authoritative cosmetic event (accepted)

Server emits a cosmetic hit/release event after gameplay Apply. Viewports receive it via multicast and run local presentation.

| Criterion | Assessment |
| --- | --- |
| Reliability of gameplay | Unaffected — cosmetics are best-effort |
| Late join | Misses past beats by design (S26A) |
| Packet loss | Dropped beat is acceptable for cosmetics |
| Duplicate suppression | `PresentationSequence` per source |
| Listen server | Safe if visuals play **only** on multicast receive path |
| Bandwidth / scaling | Unreliable multicast scales for many concurrent RTS attackers |
| Coupling | Low — fire-and-forget side effect |
| Debugging | Sequence + serial + metadata |
| Extensibility | EventType / variant tag grows cleanly |

### Option B — Replicated Attack state + client observation (deferred)

Replicate AttackState / serial / target / timestamps; clients derive visuals from transitions and Health changes.

Useful later for Approaching/Ready chrome; weak for discrete hit beats; higher coupling. **Not S26A.**

### Recommendation

**Option A for S26A** with transport locked below. Light Option B chrome remains post-S26A only.

---

## Recommended architecture

```text
[Authority] AttemptAttackHit
    → AttackHitAttempt log
    → ApplyDamageFromUnit (gameplay; unchanged)
    → schedule NextHitTime (unchanged)
    → AttackHitApplied log
    → PresentationSequence++ (authority, first value 1)
    → Unreliable NetMulticast(payload)   // NEW side effect only
    // NO inline PlayPresentation here

[Net] Unreliable NetMulticast on presentation component
      (payload includes PresentationSequence; no separate replicated sequence state)

[Local worlds]
    UGP_CombatPresentationComponent receive
      → bookkeeping / dedupe by PresentationSequence
      → if NM_DedicatedServer: no visual/debug draw
      → else: debug draw / log (S26A)
      → future: montage / VFX / cosmetic projectile (assets optional)
```

### Ownership

| Layer | Module | Responsibility |
| --- | --- | --- |
| Emit after Apply | `GPRuntime` (`UGP_UnitCommandComponent`) | Resolve presentation component; ask it to multicast; **never** await / never Play inline |
| Presentation component | `GPRuntime` | Own Unreliable NetMulticast, Sequence counter (authority), dedupe, NetMode gate, debug viz |
| Future rich UI / widgets | `GPUIRuntime` | Optional consumers |
| GAS / damage formula | `GPGASRuntime` | Unchanged |

### Policies

1. **Single play path:** Visuals execute only inside the multicast receive handler (after dedupe / NetMode gate).
2. **Dedicated server:** Receive stub may update last-seen Sequence for diagnostics; **no** visual/debug draw / asset work.
3. **Listen server:** One play via multicast receive; **no** second inline Play after emit.
4. **No AnimNotify → damage:** Forbidden for GP combat cadence.
5. **Soft asset refs (later):** optional; missing asset = silent no-op.
6. **Dropped packets:** acceptable; Sequence is **not** a retransmission mechanism.

---

## RPC ownership placement

### Requirements if NetMulticast is on `UGP_CombatPresentationComponent`

| Requirement | Detail |
| --- | --- |
| Owning actor replicated | `AGP_UnitBase` already `bReplicates = true` |
| Default subobject | Component created in UnitBase constructor as default subobject |
| Component replicated | `SetIsReplicatedByDefault(true)` (or equivalent) so component RPCs are valid on server and clients |
| Authority-only call | Multicast invoked only with authority after AttackHitApplied |
| Exists server + clients | Same subobject path as other unit components |
| Dedicated receive | Stub / sequence bookkeeping only; no visual/debug draw |
| No double Play | Emit path must not call PlayPresentation locally before/after multicast |

### Alternative: multicast on `AGP_UnitBase`

- UnitBase declares Unreliable NetMulticast and forwards payload into the local presentation component.
- Pros: actor-level RPC always tied to replicated pawn.
- Cons: pollutes UnitBase with presentation RPC surface; UnitBase already owns command/ASC/death concerns; weaker separation than CommandComponent-style ownership.

### Choice (locked for S26A)

**NetMulticast on `UGP_CombatPresentationComponent`.**

Rationale:
- Minimizes UnitBase API pollution (UnitBase only gains a default subobject pointer, matching `UnitCommandComponent` / ASC pattern).
- Presentation owns its channel, sequence counter, dedupe, and NetMode policy.
- Matches current architecture: gameplay emit from CommandComponent → sibling presentation component.

UnitBase remains responsible only for creating/replicating the subobject, not for presentation RPC signatures.

---

## Data model

```text
FGP_CombatPresentationEvent  (or mirrored multicast args)
  PresentationSequence     : uint32  // 0 invalid; first emit = 1; monotonic per source unit
  AttackSerial             : uint32  // matches executor ActiveAttackSerial
  Target                   : AGP_UnitBase*  // explicit; may be pending-kill / dead
  EventType                : EGP_CombatPresentationEventType
  AuthoritativeWorldTime   : float   // server GetTimeSeconds at emit (debug/ordering aid only)
  AppliedDamage            : float
  bBlocked                 : bool    // Apply ran; FinalDamage == 0
  bTargetDiedFromHit       : bool
  // Source NOT in payload — derived from component owner (source unit)
  // Future: VariantTag, ImpactLocation, ProjectileId, WindUpSeconds
```

```text
EGP_CombatPresentationEventType (S26A minimal)
  MeleeImpact
  // Deferred: RangedInstant, ProjectileLaunch, ProjectileImpact, HitReaction, AttackCancel
```

### Source field

Omit from wire payload when RPC lives on the source unit’s presentation component. Receivers use `GetOwner()` / `Cast<AGP_UnitBase>(GetOwner())`. Reduces payload without losing meaning.

### AuthoritativeWorldTime

- **float is sufficient** for S26A debug / transient cosmetics.
- Not a clock-sync protocol; no client-server time synchronization feature in S26A.
- Do not introduce double solely for “precision”; float matches cosmetic needs and shrinks payload.

### PresentationSequence semantics

| Rule | Detail |
| --- | --- |
| Scope | Monotonic per source unit (component authority counter) |
| Increment | Authority increments **before** multicast emit |
| First valid value | `1` (`0` = invalid / unset) |
| Wire presence | Carried **inside** multicast payload only |
| Not replicated as standing state | No separate replicated Sequence / LastEvent property in S26A |
| Used for | Duplicate suppression, gap diagnostics, listen-server safety |
| Not used for | Retransmit / late-join replay / relevancy catch-up |

**Wraparound:** For S26A, `uint32` wrap is **practically irrelevant**. If implementation uses “ignore when `Sequence <= LastProcessed`”, that comparison **must** carry an explicit comment that wraparound is ignored for S26A. Prefer also treating exact `Sequence == LastProcessed` as the primary duplicate case. Sequence must **not** be treated as a delivery guarantee.

Duplicate key: `(SourceOwner, PresentationSequence)`.

---

## Replication model

### S26A transport (locked)

**Unreliable NetMulticast** on `UGP_CombatPresentationComponent`.

Reasons:
- Event is transient and cosmetic.
- Losing an individual presentation beat is acceptable.
- Reliable per-hit multicast scales poorly with many concurrent RTS attackers.
- Reliable backlog can deliver visually stale hits.
- Presentation must not occupy the reliable channel or harm gameplay/network responsiveness.

### Explicitly out of S26A transport

| Item | Status |
| --- | --- |
| Reliable NetMulticast | **Rejected** for S26A baseline |
| Replicated `LastPresentationEvent` | **Out of scope** |
| Late-join replay of last hit | **Out of scope** |
| Relevancy catch-up of transient hit | **Out of scope** |

### Late join / relevancy semantics (expected)

- Late join does **not** receive past cosmetic events.
- Actor becoming relevant does **not** replay an old hit.
- This is **correct and intentional** S26A semantics.

### Suppression rules

| Case | Behavior |
| --- | --- |
| Same Sequence already processed | Ignore (duplicate suppression) |
| Older Sequence after gaps (optional `<=` with wrap comment) | Ignore stale beat |
| Packet loss | Missed beat; no retransmission |
| Dedicated server | No visual/debug draw |
| Listen host | Exactly one play via multicast receive path |
| Emit path | Never calls PlayPresentation inline |

### Bandwidth

Unreliable compact payload per hit (no Source actor ref). Suitable for many simultaneous attackers; drops under pressure rather than queueing stale reliable cosmetics.

---

## Temporal model

### Authoritative cosmetic moment (locked for S26A)

**Post-`AttackHitApplied` emit** — after GE Apply returns and AttackHitApplied metadata is known; emit must not alter NextHitTime / cadence.

| Moment | Role |
| --- | --- |
| `AttackHitAttempt` | Gameplay-only log; **do not** drive presentation emit |
| ApplyDamageFromUnit | Authoritative damage / death |
| `AttackHitApplied` + unreliable multicast | Authoritative **cosmetic release / impact** marker |
| Separate pre-damage `AttackRelease` | Deferred (wind-up era) |

### Wind-up / animation start

| Topic | S26A | Later |
| --- | --- | --- |
| Gameplay wind-up delaying damage | **Forbidden** | Still forbidden |
| Visual wind-up before cosmetic impact | Not required | Optional client-only; must not move NextHitTime |
| When animation starts | N/A (no montage) | On presentation receive; may finish after damage already applied |

### Immediate-first-hit

- Gameplay remains immediate on first Ready (S25).
- Presentation multicast fires in the same authoritative hit processing step.
- Clients may miss or see late visuals due to unreliable net; **acceptable**; no server wait.

### Blocked damage

- Still emit with `bBlocked=true`, `AppliedDamage=0`.
- Cadence unchanged.
- S26A: distinct debug log/draw optional.

### TargetDied during presentation

- Emit may set `bTargetDiedFromHit=true`.
- Gameplay FinishAttack(TargetDied) unchanged.
- Clients still see `bIsDead` OnRep for death presentation (collision today).

### Command replacement / attacker death / OOR

| Case | Presentation |
| --- | --- |
| Attack → Move / retarget | New AttackSerial; old cosmetics may finish if already received |
| Attacker death | No further emits |
| Target left range (no hit) | **No** event |
| Hysteresis band (Ready, no damage) | **No** event |

### Projectile visual vs authoritative damage

- Authoritative damage remains instant (S25) for current units.
- Future cosmetic projectile is post-damage visual only.
- **No** projectile collision gameplay in S26.
- True travel-time damage would be a separate gameplay stage.

### Cosmetic delay

- S26A: **no** artificial server delay.
- Must not insert server `Delay` before Apply.

### Cadence preservation

Emit is a pure side effect. No presentation TimerManager on the authority cadence path. No AnimNotify callbacks into CommandComponent.

---

## GP-S26A scope

Minimal vertical slice (review-corrected):

| Include | Detail |
| --- | --- |
| Cosmetic emit | After AttackHitApplied (incl. blocked) |
| Payload | PresentationSequence, AttackSerial, Target, EventType, AuthoritativeWorldTime (`float`), AppliedDamage, bBlocked, bTargetDiedFromHit |
| Source | Derived from component owner (not wired) |
| Transport | **Unreliable NetMulticast** on `UGP_CombatPresentationComponent` |
| Component | Default subobject on `AGP_UnitBase`; replicated; authority emit; single receive Play path |
| Sequence | Authority increment before emit; first value `1`; payload-only (not standing replicated state) |
| Dedupe | Client/listen Sequence suppression; not a redelivery mechanism |
| Late join | No past cosmetics; no relevancy hit replay |
| Dedicated | Receive stub / bookkeeping only; no visual/debug draw |
| Assets | **None** — debug draw + logs only |
| Debug | `gp.CombatPresentation.*` (non-shipping) |
| Validation | Listen server + remote client |

### Explicitly deferred (not GP-S26A)

- Reliable multicast / LastPresentationEvent / late-join or relevancy catch-up
- Real animation montages / AnimInstance / AnimNotify
- Niagara systems, materials, sounds
- Projectile actors with collision gameplay
- Attack prediction / lag compensation
- LOS, nav combat, aggro presentation
- Ability-specific visuals / GameplayCue asset pipeline
- Polished UI (health bars, damage numbers)
- Full replicated AttackState machine (Option B)
- Wind-up that offsets gameplay hit time
- Server-time synchronization protocols
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
| Listen-server double play | Single multicast receive Play path; never inline Play on emit |
| Presentation gates damage | Emit after Apply only; no waits |
| Reliable channel congestion | Unreliable multicast only for S26A cosmetics |
| Stale reliable backlog | Eliminated by rejecting reliable transport |
| Dropped beats | Accepted cosmetic loss; Sequence for diagnostics, not retry |
| Late join empty cosmetics | Expected S26A semantics |
| Dedicated visual work | NetMode gate; no debug draw on DS |
| UnitBase pollution | Multicast on presentation component, not UnitBase |
| Sequence wrap `<=` bugs | Comment wrap as practically irrelevant for S26A |
| Coupling to BP assets | Debug-only default |

---

## Implementation plan

Docs-only until explicit S26A implementation task.

Suggested order (future):

1. Define payload struct/args + EventType (GPRuntime); Source omitted; time as float.
2. Add replicated `UGP_CombatPresentationComponent` default subobject on UnitBase.
3. Authority Sequence++ + Unreliable NetMulticast; receive = dedupe + NetMode gate + debug viz.
4. Emit from CommandComponent after AttackHitApplied; no inline Play.
5. Non-shipping debug commands / log category.
6. Operator matrix (listen + client + dedicated no-visual).
7. Finalization builds; no required Content assets.

---

## Operator validation plan

| ID | Case | Expect |
| --- | --- | --- |
| P1 | Listen server host attacks | One presentation log/debug viz per received hit |
| P2 | Remote client observes attack | Sequence/Serial/metadata; one viz when packet arrives |
| P3 | Melee-like immediate Attack in range | Event on first hit without gameplay delay |
| P4 | Blocked damage | Event with bBlocked; cooldown still schedules |
| P5 | Repeated cadence | Sequences increase; duplicates ignored |
| P6 | TargetDied on hit | Optional death flag; AttackFinished TargetDied unchanged |
| P7 | Attacker dies mid-Attack | No crash; no further events |
| P8 | Attack → Move replacement | Gameplay unchanged; cosmetics may finish if already received |
| P9 | Duplicate suppression | Same Sequence ignored |
| P10 | Dedicated server | No visual/debug draw / no asset load errors |
| P11 | Late join / become relevant | **No** replay of past cosmetic hits (expected) |
| P12 | Artificial packet drop (if testable) | Missed beat does not affect damage/cadence |

Gameplay regression checks (must remain PASS): immediate hit, cadence, hysteresis, RangeUnreachable, SelfSupersede, TargetDied.

---

## Build/test plan

| When | Build |
| --- | --- |
| This analysis branch | **None** (docs only) |
| S26A implementation | GPEditor Dev + UHT; operator PIE listen+client; finalization GP Dev + Shipping |

---

## Stop condition

- Document `GP-S26_Combat_Presentation.md` updated with review corrections.
- AI Project Log + Cursor Work Report updated.
- Branch `feature/gp-s26-combat-presentation-analysis` pushed.
- **No** C++ diff. **No** Blueprint assets. **No** merge to main. **No** PR.
- **Do not** start GP-S26A implementation without an explicit task.
