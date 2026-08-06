# GP-SLICE7-AUDIT — Existing Combat Reconciliation

## Status
**GP_SLICE7_AUDIT_READY_FOR_REVIEW**

## Baseline
- Branch: `audit/gp-slice7-combat-reconciliation`
- Base / main: `035c486758059032bb2551520834dd73f8667ef5` (Merge GP-S28 Storage + ThreatValue)
- Stage type: **documentation / audit only** — no gameplay C++ changes
- Slice 6: **complete** (GP-S23R…GP-S28 merged)
- Canonical Slice 7 map (`TDD/13`): GP-S29…GP-S33

## Note on missing GDD path
`Docs/GDD/08_Combat_And_Damage.md` **does not exist**. File `Docs/GDD/08_Win_Lose_Conditions.md` is win/lose. Combat gameplay sources used instead: `GDD/02_Core_Gameplay_Loop.md`, `GDD/04_Units.md`, `GDD/09_UI_UX.md`, `GDD/11_Fog_of_War.md`, plus `TDD/04`, `TDD/05`, `TDD/02`, `TDD/13`, ADR-0003/0004/0006/0007.

## Equivalent-implementation policy (no duplicates)
Where production code already satisfies a Slice 7 requirement under another name, this audit records an **equivalent implementation**. Recommended action is **preserve + gap-fill**, not create a parallel `UGP_CombatComponent` / rewrite attack executor.

| Canonical name | Existing equivalent | Rename needed? |
| --- | --- | --- |
| `UGP_CombatComponent` fire/engage | `UGP_UnitCommandComponent` Attack Idle/Approaching/Ready + hit cadence | **No** (unless a later extraction proves necessary) |
| `Multicast_PlayAttackVFX` | `UGP_CombatPresentationComponent::Multicast_CombatPresentationEvent` | Optional cosmetic rename only |
| `GE_GP_Damage_Basic` | Native C++ `UGP_GE_Damage_Basic` | No |
| Cooldown via `GE_GP_Cooldown_Attack` | `NextAttackHitTime` + `AttackCooldown` attribute float | Yes for ADR-0003 compliance — see S31 |

---

## Matrix summary counts

| Status | Count (row-level across A–H) |
| --- | ---: |
| COMPLETE | 18 |
| PARTIAL | 9 |
| MISSING | 12 |
| CONFLICTING | 2 |
| OUTDATED | 1 |

**Blocking conflicts (must decide before rewrite):**
1. Explicit friendly-fire: TDD/04 allows ally Attack; production rejects at command + damage.
2. Attack cooldown: ADR-0003 / TDD require GE + tag; production uses server float schedule.

**Preserved systems (do not rewrite without proven gap):**
- Attack approach / Ready / hysteresis / TargetDied binding (`UGP_UnitCommandComponent`)
- Authority `ApplyDamageFromUnit` → `UGP_GE_Damage_Basic` + `UGP_DamageCalculation`
- Death path (`HandleDeathInternal` / `OnUnitDied`)
- Combat presentation multicast channel (S26A)
- Command routing for `GP.Command.Attack` (not AttackMove)
- No projectile actors (none present; not a Slice 7 correction target)

---

## A. GP-S29 CombatComponent

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Class `UGP_CombatComponent` | TDD/05, TDD/13 S29 | No class; fire logic in command component | — / `UGP_UnitCommandComponent` | OUTDATED (name) | Grep: zero `UGP_CombatComponent` | Canonical class name unused | Treat UnitCommand Attack as equivalent; do **not** add duplicate class | Document in S29R; optional later extract |
| Engage target / attack loop | TDD/05 §CombatComponent; GDD/02 Combat Loop | Idle→Approaching→Ready; `AttemptAttackHit` | `GPUnitCommandComponent.*` | COMPLETE | `EnterAttackApproaching` / `EnterAttackReady` / `AttemptAttackHit` | — | Preserve | — |
| Fire gate: range before each shot | TDD/04 CanFireAt | Distance vs EffectiveRange (+ hysteresis exit) before damage | `AttemptAttackHit` | COMPLETE | Reject `OutOfRange`; hysteresis band no-damage | — | Preserve | — |
| LOS 3-trace (Eye/Chest/Feet) | TDD/04 §LOS; GDD/11 | **None** in attack path | — | MISSING | No `HasLineOfSight` / multi-trace in combat | Full 3-pair Visibility LOS | Add LOS into existing fire gate | **GP-S29R** |
| Cooldown before next fire | TDD/05 | World-time `NextAttackHitTime` from `AttackCooldown` attr | `ResolveSanitizedAttackCooldown` | PARTIAL | Float schedule; no GE/tag | See S31 | Keep cadence; decide GE wrap in S31 | GP-S31R later |
| Server-only mutations | ADR-0004; TDD/04 | Authority guards on Attack + damage | `HasAuthority` checks | COMPLETE | `AttemptAttackHit`, `ApplyDamageFromUnit` | — | Preserve | — |
| Tick model | TDD/05 “server tick” | Component tick **enabled only while Attack active** | `SetAttackTickEnabled` | COMPLETE | `bCanEverTick=true`, start disabled | Not permanent Tick | Preserve | — |
| Chase when OOR | TDD/04 | Approaching + movement approach | Attack Approaching | COMPLETE | `EnterAttackApproaching` | — | Preserve | — |
| Target death clear | TDD/04 OnTargetDeath | `BindAttackTargetDeath` / `HandleAttackTargetDied` | same | COMPLETE | FinishAttack TargetDied | No attack-move resume (S32) | Preserve for Attack | S32 for A-move resume |

**Section A verdict:** **PARTIAL** — fire loop production-valid; canonical LOS missing; class name outdated vs equivalent.

---

## B. GP-S30 TargetingComponent

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Class `UGP_TargetingComponent` | TDD/05, TDD/13 S30 | Absent | — | MISSING | Grep: zero matches | Entire component | Implement new component (no equivalent) | GP-S30 after S29R |
| Idle auto-acquire | TDD/04; GDD/04 | Absent | — | MISSING | No overlap scan / EngageTarget | Auto-acquire pipeline | New S30 | GP-S30 |
| Acquire filters (hostile, alive, inspectable) | TDD/04 | N/A | — | MISSING | — | Filters | S30 | GP-S30 |
| Deterministic priority | GDD/04 (SWARM > unit > building TBD) | Absent | — | MISSING | — | Priority sort | Spec TBD in S30 task | GP-S30 |
| `bAutoAttacks` / Worker off | TDD/04; Worker docs | Worker comment only; no DA field wired for auto | `GPWorker.h` | PARTIAL | “No auto-attack / CombatComponent” | Formal `bAutoAttacks` on definition | Add with S30 | GP-S30 |
| FoW visibility filter | GDD/11 | Absent (FoW not built) | — | MISSING | Slice 9 FoW | Defer FoW filter until S48 | Note dependency | not Slice 7 blocker |

**Section B verdict:** **MISSING** (no equivalent).

---

## C. GP-S31 Damage / Cooldown GE

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `GE_GP_Damage_Basic` | TDD/02, TDD/13 S31 | Native C++ `UGP_GE_Damage_Basic` | `GPGE_DamageBasic.*` | COMPLETE | Instant Health Additive + MMC | No Blueprint GE asset (by design) | Preserve C++ GE | — |
| MMC `UGP_DamageCalculation` | TDD/02 | Present | `GPDamageCalculation.*` | COMPLETE | Damage/Armor/Resistance | — | Preserve | — |
| Apply via ASC (no direct Health write on attack path) | ADR-0003 | `GPDamageApplication::ApplyDamageEffect` | `GPDamageApplication.*`, `ApplyDamageFromUnit` | COMPLETE | `ApplyGameplayEffectSpecToTarget` | — | Preserve | — |
| `GE_GP_Cooldown_Attack` | TDD/02, TDD/13 S31 | **Absent** | — | MISSING | No class / asset | Duration GE + tag grant | Add native GE; wire after hit | GP-S31R |
| Tag `GP.Unit.State.AttackCooldown` | TDD/02 | Registered; unused by executor | `GPGameplayTags` | PARTIAL | Tag exists; fire gate uses float time | Gate on tag or keep float | Prefer GE+tag for ADR-0003 | GP-S31R |
| Cadence value from attrs | GDD/04 AttackSpeed / attr AttackCooldown | `AttackCooldown` attribute → `NextAttackHitTime` | Unit attrs + command | COMPLETE (value path) | `ResolveSanitizedAttackCooldown` | Duration GE not applied | Keep numeric source; wrap in GE | GP-S31R |

**Section C verdict:** **PARTIAL** — damage COMPLETE; cooldown GE MISSING (float equivalent works but conflicts ADR-0003).

---

## D. GP-S32 Attack-Move

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Tag `GP.Command.AttackMove` | TDD/09, TDD/13 | Registered | `GPGameplayTags` | PARTIAL | Tag only | No command handling | Wire in S32 | GP-S32 |
| Server validate AttackMove | TDD/04 | Not in `ValidateAndNormalizeCommand` | `GPCommandComponent` | MISSING | Only Move/Attack/Mine branches | Destination snap + accept | Implement S32 | GP-S32 |
| `AttackMoveDestination` + `bAttackMoving` on MobileUnit | TDD/04, TDD/13 | Absent | `GPMobileUnit.*` | MISSING | MobileUnit = movement only | Replicated state fields | Add on MobileUnit | GP-S32 |
| Pause move / engage / resume destination | TDD/04 | Absent | — | MISSING | Depends on Targeting | Full A-move FSM | After S30 | GP-S32 |
| Input A-mode | GDD/09 | Not verified as wired to AttackMove | PC command input | MISSING / unverified | Smart RMB → Attack only | A+click AttackMove | S32 + input | GP-S32 |

**Section D verdict:** **MISSING** (tag stub only).

---

## E. GP-S33 Attack VFX

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Unreliable multicast cosmetic after hit | TDD/04, TDD/13 S33; ADR-0004 | `Multicast_CombatPresentationEvent` | `UGP_CombatPresentationComponent` | COMPLETE | Authority emit after applied hit; Implementation-only play | Name ≠ `Multicast_PlayAttackVFX` | Preserve; optional alias rename later | — |
| Cosmetic-only (no damage in RPC) | ADR-0004 | Payload is presentation event; damage already applied | same | COMPLETE | Header contract S26A | — | Preserve | — |
| Art / Niagara / montage | TDD/04 list | Debug / primitive presentation only | S26A/B path | PARTIAL | No production attack VFX assets | Authored VFX later | Out of Slice 7 scaffold scope | content later |
| Projectile actors | TDD/04 “Projectile spawn” cosmetic note | **No** projectile classes/assets | — | MISSING (actors) | Inventory empty | Not required as gameplay actors for S29–S33 | **Do not invent** projectile gameplay | out of Slice 7 correction |

**Section E verdict:** **COMPLETE** for scaffold intent (equivalent multicast); art PARTIAL; no projectile rewrite.

---

## F. Cross-cutting authority / replication

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Authority-only damage | ADR-0004 | Guarded | `ApplyDamageFromUnit` | COMPLETE | `NoAuthority` reject | — | Preserve | — |
| Friendly-fire rejection (production) | Code / CONTRIBUTING safety | Reject same TeamId at command + damage | `GPCommandComponent`, `ValidateAttackTarget`, `ApplyDamageFromUnit` | CONFLICTING vs TDD/04 | TDD allows explicit ally Attack | Policy mismatch | **Decide:** keep reject (safer) and amend TDD, or allow FF | policy note before S30 |
| Alive/dead validation | GDD/02; TDD/04 | Source/Target dead checks | damage + ValidateAttackTarget | COMPLETE | `SourceDead`/`TargetDead`/`TargetDied` | — | Preserve | — |
| Attack state replication | TDD (server sim) | Attack state **not** replicated (by design S24–S25) | UnitCommand | COMPLETE (intentional) | Header: no replication | Client uses presentation/death only | Preserve | — |
| Threat / health via GAS | ADR-0003 | Health via GE; death sink | UnitAttributeSet + Death | COMPLETE | PostGE → death | — | Preserve | — |
| Direct Health mutation on attack path | Forbidden | Not used on attack path | — | COMPLETE | GE path only | Cheat console may set attrs for diagnostics | Keep attack path GE-only | — |

---

## G. Tests and diagnostics

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Attack inspect / invalidate diagnostics | Ops practice | `gp.Attack.Inspect`, `DestroyTarget`, `MoveTarget`, `TestInvalid` | `GPUnitCommandComponent.cpp` | COMPLETE | Console commands | No formal Failures=0 contract suite like S28 | Optional contract suite later | after S29R |
| Combat damage diagnostics | S25A | `gp.Combat.Resolve/Inspect/SetStats/ApplyDamage/KillTarget` | `GPUnitBase.cpp` | COMPLETE | Console | Same | Preserve | — |
| Presentation inspect | S26A | `gp.CombatPresentation.Inspect` | Presentation cpp | COMPLETE | Console | — | Preserve | — |
| Sequential combat regression suite | S28 pattern | **Absent** for combat | — | MISSING | Only resource suite | Optional | Not blocking audit | later |
| PIE contract Failures=0 suite for LOS/A-move | — | Absent | — | MISSING | — | Add with each gap task | per-task | — |

---

## H. Assets / content inventory

| Asset / area | Path | Status | Notes |
| --- | --- | --- | --- |
| `GE_GP_Damage_Basic` Blueprint | — | MISSING (OK) | Native C++ GE used |
| `GE_GP_Cooldown_Attack` asset/C++ | — | MISSING | Gap S31 |
| Projectile BP/C++ | — | MISSING | Not in Slice 7 correction scope |
| Combat Unit DA (SalvageWalker) | — | MISSING | Content later |
| Existing Content | `GP/Content/GrimProtocol/...` | N/A | Input IMCs, Ferronite DA, PrototypeArena, authored example BPs only — **no combat GE/projectile** |
| Authored unit example | `Units/BP_Unit_AuthoredExample.uasset` | Present | Visual example; not combat GE |

---

## Critical validation answers

| Question | Answer |
| --- | --- |
| Exists `UGP_CombatComponent`? | **No.** Equivalent: `UGP_UnitCommandComponent` Attack FSM. |
| Where is current target? | `UGP_UnitCommandComponent::AttackTarget` (`TWeakObjectPtr<AGP_UnitBase>`), server-only. |
| Who starts/stops attack loop? | `HandleCommand` / Attack enter → `SetAttackTickEnabled(true)`; `FinishAttack` / death / replace → disable tick + clear cadence. |
| Timer or permanent Tick? | **Gated component Tick** while Attack active + world-time `NextAttackHitTime`. Not permanent Tick. |
| Damage GAS/MMC or direct? | **GAS:** `UGP_GE_Damage_Basic` + `UGP_DamageCalculation` MMC via `GPDamageApplication`. |
| Authority guard on every damage path? | **Yes** on `ApplyDamageFromUnit` (and presentation emit). |
| Friendly-fire rejection? | **Yes** in production (command + validate + damage). **Conflicts** TDD/04 explicit-ally-allowed. |
| Alive/dead validation? | **Yes.** |
| Range validation before each shot? | **Yes** in `AttemptAttackHit`. |
| Canonical LOS three-trace? | **No.** |
| Cooldown GE or float/timer? | **Float/world-time** from `AttackCooldown` attribute. Tag registered unused. No `GE_GP_Cooldown_Attack`. |
| Exists `UGP_TargetingComponent`? | **No.** |
| Automatic target acquisition? | **No.** |
| Deterministic target priority? | **No.** |
| `AttackMoveDestination`? | **No.** |
| Attack-move resume after target loss? | **No** (Attack ends Idle). |
| VFX multicast cosmetic-only? | **Yes** — `Multicast_CombatPresentationEvent` after authority damage. |
| Projectile actors in Slice 7? | **None exist.** Canonical Slice 7 does not require gameplay projectiles; cosmetic note only — **preserve absence**. |
| What combat tests exist? | Console diagnostics (`gp.Attack.*`, `gp.Combat.*`, presentation inspect). No Failures=0 sequential combat suite. |
| Production-valid / do not rewrite? | Attack FSM, approach/hysteresis, GE damage+MMC, death, presentation channel, Attack command validation (hostile-only). |

---

## Recommended next task (audit decision)

**Option B — GP-S29 partial → narrow correction.**

### Recommended branch / task
- Branch: `feature/gp-s29r-combat-los-fire-gate`
- Task (to materialize later, not in this audit): `GP-S29R_Combat_LOS_Fire_Gate`

### Boundaries (for that future task)
**In:**
- Add canonical 3-trace LOS into existing `AttemptAttackHit` / fire gate (or shared helper on UnitBase)
- Fail closed when LOS blocked; chase/reposition already exists via Approaching
- Diagnostics for LOS pass/fail
- Docs update only for the correction

**Out:**
- New `UGP_CombatComponent` class / rewrite of Attack FSM
- `UGP_TargetingComponent` / auto-acquire (GP-S30)
- `GE_GP_Cooldown_Attack` (GP-S31R)
- Attack-move / `AttackMoveDestination` (GP-S32)
- Rename presentation multicast (optional, later)
- Projectile actors
- FoW targeting filter
- Friendly-fire policy flip without explicit product decision
- Storage / Slice 8+

### Why not A or C
- **Not A:** S29 incomplete without LOS.
- **Not C:** Remaining Slice 7 gaps (S30–S32, cooldown GE) are separable; smallest safe step is LOS on the preserved executor.

### After S29R (preview, not started)
1. GP-S30 TargetingComponent (auto-acquire; no FF via acquire)
2. GP-S31R Cooldown GE (ADR-0003 alignment) — or accept documented exception
3. GP-S32 Attack-move state
4. S33 treated as satisfied by S26A equivalent unless rename desired

---

## Sources reviewed
- `Docs/README.md`, `DOCUMENTATION_INDEX.md`, `AI_Project_Log.md`
- `TDD/02_GAS_Architecture.md`, `04_RTS_Selection_And_Commands.md`, `05_Unit_Architecture.md`, `13_Architecture_Proposal.md`
- `GDD/02`, `04_Units.md`, `09_UI_UX.md`, `11_Fog_of_War.md` (combat-relevant); `08_Combat_And_Damage.md` N/A
- `CONTRIBUTING.md`, `STYLE.md`
- ADR-0003, 0004, 0006, 0007
- Historical task headers: GP-S24, GP-S25, GP-S26 (combat presentation)

## Source files inspected (combat-related)
- `GPRuntime/Units/GPUnitCommandComponent.*`, `GPUnitBase.*`, `GPMobileUnit.*`, `GPWorker.*`, `GPUnit.*`
- `GPRuntime/Command/GPCommandComponent.*`
- `GPRuntime/Combat/GPCombatPresentationComponent.*`, `GPCombatPresentationTypes.h`
- `GPGASRuntime/Combat/GPDamageApplication.*`, `GPDeathSink.h`
- `GPGASRuntime/Effects/GPGE_DamageBasic.*`, `Calculations/GPDamageCalculation.*`
- `GPGASRuntime/AttributeSets/GPUnitAttributeSet.*`, `Tags/GPGameplayTags.*`

## Implementation status
**Not started** — audit documentation only.
