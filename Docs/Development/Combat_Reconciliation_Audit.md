# GP-SLICE7-AUDIT - Existing Combat Reconciliation (Refresh)

## Status
**GP_SLICE7_AUDIT_REFRESH_READY_FOR_REVIEW**

## Baseline
- Branch: `audit/gp-slice7-combat-reconciliation-refresh`
- Base / main: `d75fb426b043c80005c8363bef0f61ac37408fc5` (Merge GP-S28P4 planetary Ferronite HUD)
- Stage type: **documentation / audit only** - no gameplay C++ / Blueprint / map / config changes
- Slice 6: **complete** (GP-S23R through GP-S28 merged)
- Resource Playable Pass: **P1-P4 DONE / MERGED** on `main`
- Canonical Slice 7 map (`TDD/13`): GP-S29 through GP-S33

## Audit provenance
| Item | Value |
| --- | --- |
| Prior audit branch | `audit/gp-slice7-combat-reconciliation` |
| Prior audit commit | `2120b7d893428a4ad76cf440fa2b12a8e004afaf` |
| Prior audit base | `035c486758059032bb2551520834dd73f8667ef5` (Merge GP-S28 Storage + ThreatValue) |
| Prior status | `GP_SLICE7_AUDIT_READY_FOR_REVIEW` |
| This refresh | Content re-verified against current `main`; old branch **not** merged |

## Note on missing GDD path
`Docs/GDD/08_Combat_And_Damage.md` **does not exist**. File `Docs/GDD/08_Win_Lose_Conditions.md` is win/lose. Combat gameplay sources used instead: `GDD/02_Core_Gameplay_Loop.md`, `GDD/04_Units.md`, `GDD/09_UI_UX.md`, `GDD/11_Fog_of_War.md`, plus `TDD/04`, `TDD/05`, `TDD/02`, `TDD/13`, ADR-0003/0004/0006/0007.

## Equivalent-implementation policy (no duplicates)
Where production code already satisfies a Slice 7 requirement under another name, this audit records an **equivalent implementation**. Recommended action is **preserve + gap-fill**, not create a parallel `UGP_CombatComponent` / rewrite attack executor / duplicate damage path / invent projectile actors.

| Canonical name | Existing equivalent | Rename needed? |
| --- | --- | --- |
| `UGP_CombatComponent` fire/engage | `UGP_UnitCommandComponent` Attack Idle/Approaching/Ready + hit cadence | **No** (unless a later extraction proves necessary) |
| `Multicast_PlayAttackVFX` | `UGP_CombatPresentationComponent::Multicast_CombatPresentationEvent` | Optional cosmetic rename only |
| `GE_GP_Damage_Basic` | Native C++ `UGP_GE_Damage_Basic` | No |
| Cooldown via `GE_GP_Cooldown_Attack` | `NextAttackHitTime` + `AttackCooldown` attribute float | Yes for ADR-0003 compliance - deferred **GP-S31R** |

---

## Current main verification (2026-08-08)

### Files inspected
**GPRuntime**
- `Units/GPUnitCommandComponent.*` - Attack FSM + shared Mine/Haul host
- `Command/GPCommandComponent.*` - Attack validate / FriendlyAttackTarget
- `Units/GPUnitBase.*` - `ApplyDamageFromUnit`, death
- `Units/GPMobileUnit.*` - movement host (no AttackMove state)
- `Units/GPWorker.*` - no auto-attack
- `Combat/GPCombatPresentationComponent.*`, `GPCombatPresentationTypes.h`

**GPGASRuntime**
- `Combat/GPDamageApplication.*`
- `Effects/GPGE_DamageBasic.*`
- `Calculations/GPDamageCalculation.*`
- `AttributeSets/GPUnitAttributeSet.*`
- `Tags/GPGameplayTags.*`

### Resource pass impact (P1-P4)
`UGP_UnitCommandComponent` gained Mine/Haul / WaitingForDropOff / ResourceNode registry wake paths after the prior audit base. **Attack semantics were not rewritten:**
- Attack still uses `StartAttackExecutor` / `EvaluateAttack` / `AttemptAttackHit` / 2D range + hysteresis
- Damage still authority `ApplyDamageFromUnit` - Instant GE + MMC
- Presentation still unreliable multicast after applied hit
- No LOS / TargetingComponent / AttackMove executor / cooldown GE / projectile actors added
- Friendly same-team Attack still rejected at command + validate + damage

**Verdict:** P1-P4 did **not** change combat fire/damage semantics. Prior matrix conclusions remain valid; this refresh marks them **VERIFIED ON CURRENT MAIN**.

---

## Matrix summary counts

| Status | Count (row-level across A-H) |
| --- | ---: |
| COMPLETE | 19 |
| PARTIAL | 9 |
| MISSING | 12 |
| CONFLICTING | 1 |
| OUTDATED | 2 |

Count delta vs prior audit (18/9/12/2/1):
- Friendly-fire production behavior reclassified **COMPLETE** for intended MVP (hostile-only).
- TDD/04 "explicit ally Attack allowed" marked **OUTDATED** (docs correction debt; no code change).
- Cooldown GE remains the sole **CONFLICTING** ADR/TDD vs production float schedule (**GP-S31R**).

**Policy decisions (this refresh):**
1. **Friendly fire remains disabled** - production hostile-only is intended MVP; S29R/S30 must preserve it; future TDD wording correction needed.
2. **Attack cooldown GE deferred** - keep `NextAttackHitTime` + `AttackCooldown` until **GP-S31R**.

**Preserved systems (do not rewrite without proven gap):**
- Attack approach / Ready / hysteresis / TargetDied binding (`UGP_UnitCommandComponent`) - VERIFIED ON CURRENT MAIN
- Authority `ApplyDamageFromUnit` - `UGP_GE_Damage_Basic` + `UGP_DamageCalculation` - VERIFIED
- Death path (`HandleDeathInternal` / `OnUnitDied`) - VERIFIED
- Combat presentation multicast channel (S26A) - VERIFIED
- Command routing for `GP.Command.Attack` (not AttackMove) - VERIFIED
- No projectile actors - VERIFIED (preserve absence)

---

## A. GP-S29 CombatComponent

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Class `UGP_CombatComponent` | TDD/05, TDD/13 S29 | No class; fire logic in command component | - / `UGP_UnitCommandComponent` | OUTDATED (name) | Grep: zero `UGP_CombatComponent` | Canonical class name unused | Treat UnitCommand Attack as equivalent; do **not** add duplicate class | Document in S29R; optional later extract |
| Engage target / attack loop | TDD/05; GDD/02 | Idle->Approaching->Ready; `AttemptAttackHit` | `GPUnitCommandComponent.*` | COMPLETE - VERIFIED ON CURRENT MAIN | `EnterAttackApproaching` / `EnterAttackReady` / `AttemptAttackHit` | - | Preserve | - |
| Fire gate: range before each shot | TDD/04 CanFireAt | Distance vs EffectiveRange (+ hysteresis exit) before damage | `AttemptAttackHit` | COMPLETE - VERIFIED | Reject `OutOfRange`; hysteresis band no-damage | - | Preserve | - |
| LOS 3-trace (Eye/Chest/Feet) | TDD/04 LOS; GDD/11 | **None** in attack path | - | MISSING - VERIFIED | No `HasLineOfSight` / multi-trace in combat | Full 3-pair Visibility LOS | Add LOS into existing fire gate | **GP-S29R** |
| Cooldown before next fire | TDD/05 | World-time `NextAttackHitTime` from `AttackCooldown` attr | `ResolveSanitizedAttackCooldown` | PARTIAL - VERIFIED | Float schedule; no GE/tag | See S31 | Keep cadence; GE wrap in S31R | GP-S31R later |
| Server-only mutations | ADR-0004; TDD/04 | Authority guards on Attack + damage | `HasAuthority` checks | COMPLETE - VERIFIED | `AttemptAttackHit`, `ApplyDamageFromUnit` | - | Preserve | - |
| Tick model | TDD/05 | Component tick **enabled only while Attack active** | `SetAttackTickEnabled` | COMPLETE - VERIFIED | `bCanEverTick=true`, start disabled | Not permanent Tick | Preserve | - |
| Chase when OOR | TDD/04 | Approaching + movement approach | Attack Approaching | COMPLETE - VERIFIED | `EnterAttackApproaching` | - | Preserve | - |
| Target death clear | TDD/04 OnTargetDeath | `BindAttackTargetDeath` / `HandleAttackTargetDied` | same | COMPLETE - VERIFIED | FinishAttack TargetDied | No attack-move resume (S32) | Preserve for Attack | S32 for A-move resume |

**Section A verdict:** **PARTIAL** - fire loop production-valid; canonical LOS missing; class name outdated vs equivalent. **VERIFIED ON CURRENT MAIN.**

---

## B. GP-S30 TargetingComponent

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Class `UGP_TargetingComponent` | TDD/05, TDD/13 S30 | Absent | - | MISSING - VERIFIED | Grep: zero matches | Entire component | Implement new component (no equivalent) | GP-S30 after S29R |
| Idle auto-acquire | TDD/04; GDD/04 | Absent | - | MISSING - VERIFIED | No overlap scan / EngageTarget | Auto-acquire pipeline | New S30 | GP-S30 |
| Acquire filters (hostile, alive, inspectable) | TDD/04 | N/A | - | MISSING - VERIFIED | - | Filters; **hostile-only** (FF disabled) | S30 | GP-S30 |
| Deterministic priority | GDD/04 | Absent | - | MISSING - VERIFIED | - | Priority sort | Spec TBD in S30 task | GP-S30 |
| `bAutoAttacks` / Worker off | TDD/04; Worker docs | Worker comment only; no DA field wired for auto | `GPWorker.h` | PARTIAL - VERIFIED | No auto-attack / CombatComponent | Formal `bAutoAttacks` on definition | Add with S30 | GP-S30 |
| FoW visibility filter | GDD/11 | Absent (FoW not built) | - | MISSING - VERIFIED | Slice 9 FoW | Defer FoW filter until S48 | Note dependency | not Slice 7 blocker |

**Section B verdict:** **MISSING** (no equivalent). **VERIFIED ON CURRENT MAIN.**

---

## C. GP-S31 Damage / Cooldown GE

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `GE_GP_Damage_Basic` | TDD/02, TDD/13 S31 | Native C++ `UGP_GE_Damage_Basic` | `GPGE_DamageBasic.*` | COMPLETE - VERIFIED | Instant Health Additive + MMC | No Blueprint GE asset (by design) | Preserve C++ GE | - |
| MMC `UGP_DamageCalculation` | TDD/02 | Present | `GPDamageCalculation.*` | COMPLETE - VERIFIED | Damage/Armor/Resistance | - | Preserve | - |
| Apply via ASC (no direct Health write on attack path) | ADR-0003 | `GPDamageApplication::ApplyDamageEffect` | `GPDamageApplication.*`, `ApplyDamageFromUnit` | COMPLETE - VERIFIED | `ApplyGameplayEffectSpecToTarget` | - | Preserve | - |
| `GE_GP_Cooldown_Attack` | TDD/02, TDD/13 S31 | **Absent** | - | MISSING - VERIFIED / CONFLICTING vs ADR path | No class / asset | Duration GE + tag grant | Defer; keep float cadence | **GP-S31R** |
| Tag `GP.Unit.State.AttackCooldown` | TDD/02 | Registered; unused by executor | `GPGameplayTags` | PARTIAL - VERIFIED | Tag exists; fire gate uses float time | Gate on tag or keep float | Prefer GE+tag in S31R | GP-S31R |
| Cadence value from attrs | attr AttackCooldown | `AttackCooldown` - `NextAttackHitTime` | Unit attrs + command | COMPLETE (value path) - VERIFIED | `ResolveSanitizedAttackCooldown` | Duration GE not applied | Keep numeric source; wrap in GE later | GP-S31R |

**Section C verdict:** **PARTIAL** - damage COMPLETE; cooldown GE MISSING (float equivalent works; ADR conflict deferred to GP-S31R). **VERIFIED ON CURRENT MAIN.**

---

## D. GP-S32 Attack-Move

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Tag `GP.Command.AttackMove` | TDD/09, TDD/13 | Registered | `GPGameplayTags` | PARTIAL - VERIFIED | Tag only | No command handling | Wire in S32 | GP-S32 |
| Server validate AttackMove | TDD/04 | Not in `ValidateAndNormalizeCommand` | `GPCommandComponent` | MISSING - VERIFIED | Only Move/Attack/Mine branches | Destination snap + accept | Implement S32 | GP-S32 |
| `AttackMoveDestination` + `bAttackMoving` on MobileUnit | TDD/04, TDD/13 | Absent | `GPMobileUnit.*` | MISSING - VERIFIED | MobileUnit = movement only | Replicated state fields | Add on MobileUnit | GP-S32 |
| Pause move / engage / resume destination | TDD/04 | Absent | - | MISSING - VERIFIED | Depends on Targeting | Full A-move FSM | After S30 | GP-S32 |
| Input A-mode | GDD/09 | Not verified as wired to AttackMove | PC command input | MISSING / unverified | Smart RMB - Attack only | A+click AttackMove | S32 + input | GP-S32 |

**Section D verdict:** **MISSING** (tag stub only). **VERIFIED ON CURRENT MAIN.**

---

## E. GP-S33 Attack VFX

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Unreliable multicast cosmetic after hit | TDD/04, TDD/13 S33; ADR-0004 | `Multicast_CombatPresentationEvent` | `UGP_CombatPresentationComponent` | COMPLETE - VERIFIED | Authority emit after applied hit; Implementation-only play | Name differs from Multicast_PlayAttackVFX | Preserve; optional alias rename later | - |
| Cosmetic-only (no damage in RPC) | ADR-0004 | Payload is presentation event; damage already applied | same | COMPLETE - VERIFIED | Header contract S26A | - | Preserve | - |
| Art / Niagara / montage | TDD/04 list | Debug / primitive presentation only | S26A/B path | PARTIAL - VERIFIED | No production attack VFX assets | Authored VFX later | Out of Slice 7 scaffold scope | content later |
| Projectile actors | TDD/04 cosmetic note | **No** projectile classes/assets | - | MISSING (actors) - VERIFIED | Inventory empty | Not required as gameplay actors for S29?S33 | **Do not invent** projectile gameplay | out of Slice 7 correction |

**Section E verdict:** **COMPLETE** for scaffold intent (equivalent multicast); art PARTIAL; no projectile rewrite. **VERIFIED ON CURRENT MAIN.**

---

## F. Cross-cutting authority / replication

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Authority-only damage | ADR-0004 | Guarded | `ApplyDamageFromUnit` | COMPLETE - VERIFIED | NoAuthority reject | - | Preserve | - |
| Friendly-fire rejection (production / MVP) | Intended MVP policy | Reject same TeamId at command + damage | `GPCommandComponent`, `ValidateAttackTarget`, `ApplyDamageFromUnit` | COMPLETE - VERIFIED (MVP) | Hostile-only at three layers | TDD/04 ally-Attack wording outdated | **Keep disabled**; amend TDD later | docs correction (not S29R) |
| TDD/04 explicit ally Attack allowed | TDD/04 Server Validation | Conflicts with production | TDD only | OUTDATED | Docs say skip team check for explicit Attack | Docs correction debt | Do not implement FF | future docs pass |
| Alive/dead validation | GDD/02; TDD/04 | Source/Target dead checks | damage + ValidateAttackTarget | COMPLETE - VERIFIED | SourceDead/TargetDead/TargetDied | - | Preserve | - |
| Attack state replication | TDD (server sim) | Attack state **not** replicated (by design S24-S25) | UnitCommand | COMPLETE (intentional) - VERIFIED | Header: no replication | Client uses presentation/death only | Preserve | - |
| Health via GAS | ADR-0003 | Health via GE; death sink | UnitAttributeSet + Death | COMPLETE - VERIFIED | PostGE -> death | - | Preserve | - |
| Direct Health mutation on attack path | Forbidden | Not used on attack path | - | COMPLETE - VERIFIED | GE path only | Cheat console may set attrs for diagnostics | Keep attack path GE-only | - |

---

## G. Tests and diagnostics

| Requirement | Canonical source | Existing implementation | File/class | Status | Evidence | Exact gap | Recommended action | Proposed next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Attack inspect / invalidate diagnostics | Ops practice | `gp.Attack.Inspect`, `DestroyTarget`, `MoveTarget`, `TestInvalid` | `GPUnitCommandComponent.cpp` | COMPLETE - VERIFIED | Console commands | No formal Failures=0 combat suite like S28 | Optional contract suite with S29R | **GP-S29R** |
| Combat damage diagnostics | S25A | `gp.Combat.Resolve/Inspect/SetStats/ApplyDamage/KillTarget` | `GPUnitBase.cpp` | COMPLETE - VERIFIED | Console | Same | Preserve | - |
| Presentation inspect | S26A | `gp.CombatPresentation.Inspect` | Presentation cpp | COMPLETE - VERIFIED | Console | - | Preserve | - |
| Sequential combat regression suite | S28 pattern | **Absent** for combat | - | MISSING - VERIFIED | Only resource suite | Optional | Not blocking audit | later |
| PIE contract Failures=0 suite for LOS/A-move | - | Absent | - | MISSING - VERIFIED | - | Add with each gap task | per-task | **GP-S29R** for LOS |

---

## H. Assets / content inventory

| Asset / area | Path | Status | Notes |
| --- | --- | --- | --- |
| `GE_GP_Damage_Basic` Blueprint | - | MISSING (OK) | Native C++ GE used |
| `GE_GP_Cooldown_Attack` asset/C++ | - | MISSING | Gap S31R |
| Projectile BP/C++ | - | MISSING | Not in Slice 7 correction scope |
| Combat Unit DA (SalvageWalker) | - | MISSING | Content later |
| Existing Content | `GP/Content/GrimProtocol/...` | N/A | No combat GE/projectile authored as Slice 7 deliverable |
| Authored unit example | `Units/BP_Unit_AuthoredExample.uasset` | Present | Visual example; not combat GE |

---

## Critical validation answers

| Question | Answer |
| --- | --- |
| Exists `UGP_CombatComponent`? | **No.** Equivalent: `UGP_UnitCommandComponent` Attack FSM. VERIFIED. |
| Where is current target? | `UGP_UnitCommandComponent::AttackTarget` (`TWeakObjectPtr<AGP_UnitBase>`), server-only. |
| Who starts/stops attack loop? | `HandleCommand` / Attack enter - `SetAttackTickEnabled(true)`; `FinishAttack` / death / replace - disable tick + clear cadence. |
| Timer or permanent Tick? | **Gated component Tick** while Attack active + world-time `NextAttackHitTime`. Not permanent Tick. |
| Damage GAS/MMC or direct? | **GAS:** `UGP_GE_Damage_Basic` + `UGP_DamageCalculation` MMC via `GPDamageApplication`. |
| Authority guard on every damage path? | **Yes** on `ApplyDamageFromUnit` (and presentation emit). |
| Friendly-fire rejection? | **Yes** (command + validate + damage). **MVP policy: keep disabled.** TDD ally-Attack wording outdated. |
| Alive/dead validation? | **Yes.** |
| Range validation before each shot? | **Yes** in `AttemptAttackHit`. |
| Canonical LOS three-trace? | **No.** -> **GP-S29R** |
| Cooldown GE or float/timer? | **Float/world-time** from `AttackCooldown` attribute. Tag registered unused. No `GE_GP_Cooldown_Attack`. -> **GP-S31R** |
| Exists `UGP_TargetingComponent`? | **No.** |
| Automatic target acquisition? | **No.** |
| Deterministic target priority? | **No.** |
| `AttackMoveDestination`? | **No.** |
| Attack-move resume after target loss? | **No** (Attack ends Idle). |
| VFX multicast cosmetic-only? | **Yes** - `Multicast_CombatPresentationEvent` after authority damage. |
| Projectile actors in Slice 7? | **None exist.** Preserve absence. |
| What combat tests exist? | Console diagnostics (`gp.Attack.*`, `gp.Combat.*`, presentation inspect). No Failures=0 sequential combat suite. |
| Production-valid / do not rewrite? | Attack FSM, approach/hysteresis, GE damage+MMC, death, presentation channel, Attack command validation (hostile-only). |

---

## Recommended NEXT - GP-S29R Combat LOS Fire Gate

**Spec-ready recommendation only. Do not implement in this audit.**

### Task / branch (future)
- Branch: `feature/gp-s29r-combat-los-fire-gate`
- Task file (materialize after approval): `Docs/Development/Claude_Tasks/GP-S29R_Combat_LOS_Fire_Gate.md`
- Code Allowed: YES only after explicit approval of this refresh

### Goal
Add canonical multi-point LOS fire gating into the **existing** Attack FSM before applied hit. Preserve approach / Ready / hysteresis / damage GE / presentation. No duplicate CombatComponent.

### Classes / functions expected to change (implementation later)
| Area | Likely touch |
| --- | --- |
| `UGP_UnitCommandComponent` | `AttemptAttackHit` (and/or shared `CanFireAt` helper used by Ready cadence) - add LOS gate after range checks, before `ApplyDamageFromUnit` |
| `AGP_UnitBase` (optional helper) | `GetAttackOriginPoints` / `GetHitPoints` / `HasLineOfSightTo` resolving sockets + bounds fallbacks per TDD/04 |
| Diagnostics | Extend `gp.Attack.*` or add LOS-specific inspect; deterministic contract runner |
| Docs | Task file + index when S29R starts |

**Do not change for S29R:** TargetingComponent, AttackMove, cooldown GE, projectile actors, FF policy, Mining/Haul, presentation art pipeline.

### Exact LOS policy (from `TDD/04_RTS_Selection_And_Commands.md` Range Check and Line-of-Sight)
- Channel: `ECC_Visibility`
- Forgiving: **ANY** of 3 pairs clear => LOS valid
- Clear rule: `!bBlockingHit` **OR** `HitActor == Target`
- Source sockets (attacker): `AttackOrigin_Eye` / `AttackOrigin_Chest` / `AttackOrigin_Feet`
  Fallbacks: bounds Top / Center / (Bottom + 10 cm)
- Target sockets: `Hit_Head` / `Hit_Chest` / `Hit_Feet`
  Fallbacks: bounds Top / Center / (Bottom + 10 cm)
- Pairs: Eye->Head, Chest->Chest, Feet->Feet
- Authority: server-only evaluation (same as current fire gate)

### Behavior while LOS blocked
- **Do not** apply damage / do not emit combat presentation for that attempt
- **Do not** finish Attack solely due to LOS block
- Remain in Attack executor; prefer stay Ready if still in range hysteresis band, else Approaching / chase via existing movement approach (same as out-of-range chase model)
- Cooldown: do **not** advance `NextAttackHitTime` on a blocked attempt (no spent shot without hit) - align with fail-closed fire gate

### Behavior when LOS restored
- Next Ready cadence evaluation that passes range + LOS applies hit normally
- No PIE restart / no command re-issue required

### Relation to approach / range / hysteresis
| Gate | Order in fire path |
| --- | --- |
| Target validity / dead / team | Existing `ValidateAttackTarget` |
| Distance vs EffectiveRange (+ exit hysteresis) | Existing `AttemptAttackHit` |
| **LOS 3-trace** | **New - after range OK, before damage** |
| Apply damage + presentation + schedule cooldown | Existing |

LOS does not replace range. Approach/hysteresis remain unchanged.

### Deterministic contract tests (S29R acceptance)
Minimum Failures=0 cases:
1. Clear LOS in range -> damage applies
2. Full cover (all 3 blocked) in range -> no damage, Attack remains active
3. Low cover (only Eye->Head clear) -> damage applies (forgiving ANY)
4. LOS blocked then blocker removed -> subsequent cadence hits without re-command
5. Same-team still rejected (FF regression)
6. No permanent Tick introduced for LOS (evaluate on existing Attack tick / hit attempt only)

### Operator PIE tests (S29R)
1. Two units Attack with clear LOS - damage + presentation
2. Place blocking geometry between - hits stop while Attack continues / chase
3. Remove blocker - hits resume
4. Low cover allowing head line - still fires
5. Confirm Worker/Mine haul unaffected

### Out of S29R
- New `UGP_CombatComponent`
- `UGP_TargetingComponent` / auto-acquire (GP-S30)
- `GE_GP_Cooldown_Attack` (GP-S31R)
- Attack-move (GP-S32)
- Projectile actors / combat art
- Friendly-fire enablement
- FoW targeting filter
- Resource / Storage / HUD work

### After S29R (preview, not started)
1. GP-S30 TargetingComponent (auto-acquire; hostile-only; no FF via acquire)
2. GP-S31R Cooldown GE (ADR-0003 alignment)
3. GP-S32 Attack-move state
4. S33 treated as satisfied by S26A equivalent unless rename desired

---

## Sources reviewed
- Prior audit @ `2120b7d893428a4ad76cf440fa2b12a8e004afaf`
- `Docs/README.md`, `DOCUMENTATION_INDEX.md`, `AI_Project_Log.md`
- `TDD/02_GAS_Architecture.md`, `04_RTS_Selection_And_Commands.md`, `05_Unit_Architecture.md`, `13_Architecture_Proposal.md`
- `GDD/02`, `04_Units.md`, `09_UI_UX.md`, `11_Fog_of_War.md`
- ADR-0003, 0004, 0006, 0007
- Current `main` combat sources listed above

## Implementation status
**Not started** - audit documentation refresh only. **Do not implement GP-S29R on this branch.**
