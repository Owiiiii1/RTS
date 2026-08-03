# GP-S25 Attack Damage Execution

## Status
**ANALYSIS_READY_IMPLEMENTATION_PENDING**

## Baseline
`main` @ `f0bfb3b0bfa3b96015011cc6c4bd0375d0b6ef69` (Merge GP-S24 attack execution foundation)

Depends on: GP-S24 `DONE_WITH_DAMAGE_EXECUTION_DEFERRED`; GP-S03 Attribute Sets `DONE`; GPGASRuntime ASC + `UGP_DamageCalculation` stubs present.

Branch: `feature/gp-s25-attack-damage-analysis`

Recommended close status after full implementation + validation: **`DONE_WITH_VISUAL_COMBAT_DEFERRED`**

---

## Baseline inventory (actual code)

### Attack execution (GPRuntime)

| Symbol | State |
| --- | --- |
| `UGP_UnitCommandComponent` | Owns Attack Idle/Approaching/Ready; fixed `AttackRange=250` EditDefaultsOnly; no damage/hit cadence |
| `EGP_AttackTerminalReason` | CommandReplaced, InvalidTarget, TargetDestroyed, MovementRejected, MovementCancelled, EndPlay — **no TargetDied** |
| `AGP_UnitBase` | TeamId + UnitCommandComponent; comment: ASC/Definition/death **deferred**; **no** `IAbilitySystemInterface` |
| `AGP_MobileUnit` | MovementComponent only |
| `AGP_Unit` | Capsule + mesh; no ASC |

### GAS host / attributes (GPGASRuntime)

| Symbol | State |
| --- | --- |
| `UGP_AbilitySystemComponent` | Exists; `SetProjectReplicationMode` + `InitAbilityActorInfo` logging |
| ASC on units | **Absent** — no CreateDefaultSubobject ASC on UnitBase/MobileUnit/Unit |
| ASC on players | `AGP_PlayerState` owns ASC + `UGP_PlayerAttributeSet`; Mixed replication; `InitAbilityActorInfo(this,this)` |
| `UGP_UnitAttributeSet` | Health, MaxHealth, Armor, DamageResistance, AttackCooldown, Damage, AttackRange, AttackSpeed, MoveSpeed, CarriedFerronite — all default **0** |
| `PreAttributeChange` | Clamps Health `[0,MaxHealth]`, MaxHealth≥0, CarriedFerronite≥0 |
| `PostGameplayEffectExecute` | **Does not exist** |
| Death path | **None** (no `bIsDead`, `OnUnitDied`, HandleDeath) |
| `UGP_DamageCalculation` | **Exists** (MMC): `max(0, Damage-Armor)*(1-clamp(Resistance,0,1))` → returns **negative** Health magnitude |
| GameplayEffect assets | **None** in repo for `GE_GP_Damage_Basic` |
| Combat BP/DA | **None** required for current Attack path |

### Module graph

`GPRuntime` **already PublicDepends** on `GPGASRuntime` (`GPRuntime.Build.cs`). Unit actors may own `UGP_AbilitySystemComponent` / use `UGP_UnitAttributeSet` / `UGP_DamageCalculation` without a new module edge. **No cycle** if GPGASRuntime never depends on GPRuntime unit types (AttributeSet death notify must use weak actor interface or gameplay message, not `#include UnitBase` from AttributeSet if that creates cycle — see Death contract).

### Tags (native `FGPGameplayTags`)

| Tag | Exists |
| --- | --- |
| `GP.Command.Attack` | Yes |
| `GP.Unit.State.Dead` | Yes |
| `GP.Unit.State.AttackCooldown` | Yes (state tag; unused by Attack executor) |
| `GP.Data.Damage` / `GP.Event.UnitDied` / `GP.Effect.Damage` | **No** |

### Search summary

| Query | Result |
| --- | --- |
| Unit ASC / `IAbilitySystemInterface` on units | Missing |
| `PostGameplayEffectExecute` | Missing |
| `ApplyGameplayEffect` / `MakeOutgoingSpec` in project combat | Not used for units |
| `OnDeath` / `UnitDied` / death delegate | Missing |
| `UGP_DamageCalculation` | Present, formula locked in code |
| GP-S24 range | Component float 250; GAS AttackRange unused |

---

## Problem

GP-S24 proves composite Attack approach/Ready without combat. Units cannot take or deal damage because they have no ASC/UnitAttributeSet wiring, no GE application path, no death contract, and no Ready hit cadence.

---

## Goals

Minimal complete vertical slice (split into S25A/S25B — see below):

```text
Unit ASC + attributes initialized
→ Attack Ready hits on cooldown
→ authoritative GE damage via existing MMC
→ Health reduction
→ single death transition
→ attackers FinishAttack(TargetDied)
```

## Non-Goals

Animation, projectile, VFX/SFX, crit/accuracy/elements, splash, aggro, AttackMove, LOS, Nav, prediction, UI health bars, score/rewards, respawn, corpse systems, GameMode win-condition wiring beyond optional log hook.

---

## Scope decisions (locked)

| Topic | In GP-S25? | Decision |
| --- | --- | --- |
| Repeated hits while Ready | **Yes** (S25B) | Server world-time cadence |
| One-shot only | No (as end state) | S25A may test single ApplyDamage; S25B adds cadence |
| Cooldown timer | **Yes** (S25B) | From `AttackCooldown` seconds |
| AttackSpeed | **Deferred** | Not used; cooldown is source of truth |
| Armor / DamageResistance | **Yes** (S25A) | Via existing MMC |
| Death | **Yes** (S25A) | Once-only on Health≤0 |
| Actor destruction | **Yes** (S25A) | Delayed Destroy after death |
| Death delegate | **Yes** (S25A) | Native multicast on UnitBase |
| GameMode notification | **Deferred** | Log only; no CurrentUnits decrement in S25 |
| Target invalidation | **Yes** | Existing Tick + TargetDied |
| Attribute defaults | **Yes** | EditDefaultsOnly bootstrap + authority init |
| Debug hooks | **Yes** | Non-shipping console |

---

## Selected ownership architecture

| Layer | Owner | Responsibility |
| --- | --- | --- |
| Attack state / Ready cadence / hit attempt | `UGP_UnitCommandComponent` (GPRuntime) | When to hit; serial guards; FinishAttack |
| ASC + UnitAttributeSet | `AGP_UnitBase` (GPRuntime host) | Owns components; `IAbilitySystemInterface` |
| Damage magnitude | `UGP_DamageCalculation` (GPGASRuntime) | Armor/resistance formula |
| GE class | New `UGP_GE_Damage_Basic` (GPGASRuntime) | Instant Health Additive via MMC; **no BP asset required** |
| Health→death signal | `UGP_UnitAttributeSet::PostGameplayEffectExecute` → owner callback | Detect Health≤0 after GE |
| Death lifecycle | `AGP_UnitBase` | `bIsDead`, `OnUnitDied`, stop move, clear commands, collision, Destroy |

**Dependency rule:** AttributeSet must not `#include` `GPUnitBase.h`. Use a thin GPGASRuntime interface (e.g. `IGP_CombatDamageReceiver` / `IGP_HealthDeathNotify` implemented by UnitBase) **or** `GetOwningActor()` + `FindComponent` / interface cast declared in GPGASRuntime. Preferred: `UINTERFACE`/`IGP_UnitDeathSink` in GPGASRuntime implemented by `AGP_UnitBase`.

---

## Damage mechanism (locked)

### Selection: **Option B — Instant GE + existing `UGP_DamageCalculation` MMC**

```text
Attacker ASC → MakeOutgoingSpec(UGP_GE_Damage_Basic) → ApplyGameplayEffectSpecToTarget(Target ASC)
→ MMC captures Source.Damage, Target.Armor, Target.DamageResistance
→ returns -EffectiveDamage as Health Additive magnitude
→ Target Health clamped in PreAttributeChange
→ PostGameplayEffectExecute may observe Health ≤ 0 → death
```

| Option | Verdict |
| --- | --- |
| A. SetByCaller meta Damage | Rejected for MVP — would dual-use/confuse `Damage` attribute; MMC already captures source Damage |
| B. Existing MMC on Instant GE | **Selected** — matches TDD/02 + implemented formula; single Health write |
| C. Direct `SetNumericAttributeBase` Health | Rejected — bypasses mitigation, replication discipline, GE pipeline |

### MMC vs ExecutionCalculation

MMC is **sufficient**: it produces one Health delta. Armor/Resistance are capture inputs, not separate attribute writes. `UGameplayEffectExecutionCalculation` deferred until multi-output combat (e.g. lifesteal, threat).

### GE without Blueprint asset

**Locked:** C++ `UCLASS() UGP_GE_Damage_Basic : public UGameplayEffect` constructed in module startup / CDO with:

- Duration: Instant
- Modifier: Health Additive
- Magnitude: Custom Calculation Class = `UGP_DamageCalculation`

Application uses `UGP_AbilitySystemComponent::MakeOutgoingSpec(UGP_GE_Damage_Basic::StaticClass(), …)` (or equivalent level 1 spec). No Content Browser GE required for acceptance.

---

## Attribute semantics (locked)

| Attribute | Meaning |
| --- | --- |
| `Damage` | **Source base damage** (attacker stat). Not a target meta attribute. |
| `Armor` | Flat reduction before resistance (`max(0, Damage - Armor)`) |
| `DamageResistance` | Fraction **0..1** (1 = full immune). MMC clamps to `[0,1]`. Analysis recommends future clamp to `[0,0.95]` in MMC if full immune unwanted — **keep MMC `[0,1]` for S25** (matches existing code); document |
| `AttackCooldown` | Seconds between Ready hits |
| `AttackSpeed` | **Unused in S25**; deferred |
| `AttackRange` | GAS attribute becomes preferred when >0 after init; component float remains fallback |
| `Health` / `MaxHealth` | Current / max; Health clamped to MaxHealth |

**No new IncomingDamage attribute** in S25.

---

## Damage formula (locked — matches `UGP_DamageCalculation`)

```text
RawDamage = max(0, SourceDamage)
AfterArmor = max(0, RawDamage - max(0, TargetArmor))
Resistance = clamp(TargetDamageResistance, 0, 1)
FinalDamage = AfterArmor * (1 - Resistance)
HealthDelta = -FinalDamage
```

| Rule | Decision |
| --- | --- |
| Minimum damage | **0** allowed (full block) |
| Armor > Damage | AfterArmor = 0 |
| Non-finite stats | Treat capture failure as 0 (existing MMC warnings) |
| Source == target | Reject hit attempt (command layer) |
| Friendly fire | Reject (same TeamId) — align Attack validation |
| Neutral targets | Allowed (TeamId 0/-1) — same as GP-S24 |

---

## Attack cadence (locked — S25B)

### Selection: **Immediate-first-hit**

```text
Enter Ready
→ AttemptHit now (if eligible)
→ NextHitTime = Now + CooldownSeconds
while Ready:
  if Now >= NextHitTime → AttemptHit → NextHitTime = Now + CooldownSeconds
```

| Case | Behavior |
| --- | --- |
| Target leaves range before NextHit | Pause hits; Approaching; **preserve NextHitTime** (no reset) |
| Target dies during Apply | Death callback FinishAttack TargetDied; no further hits |
| Command replaced during cooldown | ResetAttackExecutor; no hit |
| Retarget | New Attack serial; new Ready → immediate first hit |
| Attacker dies | Owner death shutdown; Tick off |
| Cooldown ≤ 0 or non-finite | Clamp to minimum **0.05s** for safety (log Warning); do not hit every frame |
| AttackCooldown attribute changes | Next schedule uses **current** attribute at hit time |

`AttackSpeed` not converted in S25.

---

## Range source-of-truth (locked)

**Fallback hierarchy (no silent zero-range):**

```text
EffectiveRange =
  if UnitASC AttackRange is finite AND > 0 → GAS AttackRange
  else → UnitCommandComponent.AttackRange (default 250)
```

Do **not** remove component property in S25. Migration complete only after init guarantees nonzero GAS values on production units.

---

## Attribute initialization (locked)

| Path | Mechanism |
| --- | --- |
| Production bootstrap | `EditDefaultsOnly` combat defaults on `AGP_UnitBase` (MaxHealth, Health, Damage, Armor, DamageResistance, AttackCooldown, AttackRange) — **not** hardcoded in AttributeSet.cpp |
| Authority init | After ASC `InitAbilityActorInfo` + AttributeSet granted: `SetNumericAttributeBase` for each default (or one Instant init GE C++ class) |
| Test override | Non-shipping `gp.Combat.SetStats` sets bases on selected authority unit |
| Assets/DA | Deferred |

Recommended CDO test defaults (overridable in BP): MaxHealth=100, Health=100, Damage=20, Armor=0, DamageResistance=0, AttackCooldown=1.0, AttackRange=250.

---

## Death contract (locked — S25A)

```text
PostGameplayEffectExecute (Health affected)
→ if Health <= 0 && owner implements death sink && !already dead
→ AGP_UnitBase::HandleDeath (authority)
→ bIsDead = true (replicated)
→ add loose tag GP.Unit.State.Dead (optional but preferred)
→ StopMove(EndPlay-equivalent Manual/Command path safe)
→ UnitCommandComponent shutdown (Finish/clear Held, disable Attack tick)
→ disable collision (capsule)
→ Broadcast OnUnitDied(this)
→ SetLifeSpan / delayed Destroy (e.g. 0.5–2.0s EditDefaultsOnly)
```

| Question | Decision |
| --- | --- |
| Health≤0 detection | AttributeSet PostGEExecute + MaxHealth/Health check |
| Once-only | `bIsDead` guard before broadcast |
| Delegate | `DECLARE_MULTICAST_DELEGATE_OneParam(FGP_OnUnitDied, AGP_UnitBase*)` on UnitBase |
| Dead reject commands | Yes — `ReceiveCommand` / `HandleCommand` early-out |
| Movement stop | Yes |
| Held clear | Yes |
| Collision disable | Yes |
| Destroy | Delayed, not immediate inside GE execute stack if avoidable — schedule Destroy next tick / LifeSpan |
| GameMode | Not required in S25 |
| Dead UObject validity | Remains valid until Destroy; attackers must tolerate weak ptr |

Add `EGP_AttackTerminalReason::TargetDied`.

---

## Target death integration (locked — S25B binds; S25A provides delegate)

```text
StartAttackExecutor:
  bind AttackTarget->OnUnitDied
End/Replace/Finish:
  unbind

OnTargetDied:
  if Serial matches ActiveAttackSerial → FinishAttack(Cancelled or Failed, TargetDied)
```

**Preferred result:** `Failed` + `TargetDied` (target loss) — or `Cancelled` + `TargetDied`. **Locked: `Failed` + `TargetDied`** (Attack objective failed).

Do not rely solely on Tick invalidation for death (Destroy delay / weak race); bind delegate.

### Reentrancy during hit

```text
AttemptHit
→ ApplyGameplayEffectSpecToTarget
→ PostGEExecute → HandleDeath → OnUnitDied
→ attacker FinishAttack (unbind, clear Held)
→ return to AttemptHit (must no-op further work; bFinishingAttack / serial guards)
```

No Destroy inside PostGEExecute synchronous path — use LifeSpan/next-tick.

---

## Attacker death (locked)

`AGP_UnitBase::HandleDeath` calls command-component `NotifyOwnerDied()`:

- Finish/reset Attack without clearing unrelated newer Held incorrectly
- Disable Attack tick
- Clear Held if any
- Stop movement

Works for Approaching, Ready, mid-hit, mid-target-death callback (`bFinishingAttack` / `bIsDead` guards).

---

## Replication (locked)

| Item | Policy |
| --- | --- |
| Damage apply | Authority only |
| Attributes | GAS Mixed on unit ASC (match PlayerState) |
| `bIsDead` | Replicated bool on UnitBase |
| Client visuals | OnRep Health / OnRep bIsDead later; **no** damage RPC |
| Prediction | None |
| Client hit requests | Forbidden |

---

## Tags (locked proposal — implement in code stage)

| Tag | Action |
| --- | --- |
| `GP.Unit.State.Dead` | **Use existing** on death |
| `GP.Data.Damage` | **Not required** (MMC path) |
| `GP.Event.UnitDied` | Optional; native delegate preferred — **defer tag event** |
| `GP.Effect.Damage` | Optional GE asset tag — **defer** |

No tag additions on this analysis branch.

---

## Proposed implementation files

### GP-S25A — Health & Damage Foundation

| File | Change |
| --- | --- |
| `GPUnitBase.h/.cpp` | ASC + UnitAttributeSet subobjects; `IAbilitySystemInterface`; init; `bIsDead`; `OnUnitDied`; `HandleDeath`; command/move shutdown; combat EditDefaultsOnly defaults |
| `GPUnitAttributeSet.h/.cpp` | `PostGameplayEffectExecute`; death notify via GPGAS interface |
| New `IGP_UnitDeathSink` (GPGASRuntime) | Thin notify interface |
| New `GPGE_Damage_Basic.h/.cpp` | C++ Instant GE using MMC |
| `GPGameplayTags` | Optionally grant Dead tag (existing) |
| Non-shipping `gp.Combat.*` | Inspect / SetStats / ApplyDamage / KillTarget |
| Docs | S25A record |

**NO Build.cs change** expected (dependency exists).

### GP-S25B — Attack Cadence Integration

| File | Change |
| --- | --- |
| `GPUnitCommandComponent.h/.cpp` | Ready hit loop; NextHitTime; Apply damage GE; bind OnUnitDied; `TargetDied` reason; EffectiveRange from GAS fallback; attacker death notify |
| Docs | S25B record |

**MovementComponent:** NO changes.

---

## Reentrancy analysis

| Chain | Guard |
| --- | --- |
| Death during hit | `bIsDead` + `bFinishingAttack`; FinishAttack idempotent; no recursive Apply |
| Self-target | Reject before Apply |
| Replacement during callback | Exact Attack serial; Reset before new Start |
| Destroy in death callback | Forbidden sync Destroy; LifeSpan only |
| Multi-attacker kill | Single HandleDeath; each attacker OnUnitDied independently |

---

## Logging contract

| Event | When |
| --- | --- |
| `AttackHitAttempt` | Before Apply |
| `AttackHitApplied` | After successful Apply (FinalDamage, HealthBefore/After) |
| `AttackHitRejected` | Missing ASC / dead / OOR / cooldown |
| `DamageCalculated` | Optional Verbose from MMC (already Verbose) |
| `UnitDeathStarted` / `UnitDied` | HandleDeath |
| `AttackFinished Reason=TargetDied` | Attacker reaction |

Fields: Attacker, Target, AttackSerial, RawDamage, Armor, Resistance, FinalDamage, HealthBefore, HealthAfter, Cooldown, HitIndex, Role, NetMode. No Tick spam.

---

## Debug hooks (non-shipping)

| Command | Behavior |
| --- | --- |
| `gp.Combat.Inspect` | Prefer authority unit with active Attack; else first authority unit — dump Health/Damage/Cooldown/Range/ASC/Dead |
| `gp.Combat.SetStats H D CD [Armor] [Res] [Range]` | Authority SetNumericAttributeBase |
| `gp.Combat.ApplyDamage Amount` | Apply C++ damage GE from selected attacker ASC to its Attack target (or first other unit) |
| `gp.Combat.KillTarget` | ApplyDamage with large Amount through GE path — **not** fake `bIsDead` |

---

## Operator validation plan

### S25A (foundation)

| ID | Case |
| --- | --- |
| A1 | SetStats + ApplyDamage reduces Health with Armor/Resistance |
| A2 | Overkill → single UnitDied; bIsDead; no double death |
| A3 | KillTarget via GE path |
| A4 | Dead unit rejects commands |
| A5 | Listen Server host authority |
| A6 | GPEditor + GP Dev + Shipping |

### S25B (cadence)

| ID | Case |
| --- | --- |
| B1 | OOR Attack → Ready → repeated hits on cooldown (not per-frame) |
| B2 | Leave range pauses; return resumes with preserved NextHitTime |
| B3 | Target death → AttackFinished TargetDied |
| B4 | Two attackers vs one target; one death |
| B5 | Attack→Move during cooldown |
| B6 | Retarget during cooldown |
| B7 | Attacker dies Approaching/Ready |
| B8 | QueueDeferred unchanged |
| B9 | Remote Team 2 (may be NOT_RUN_ACCEPTED_BY_USER) |
| B10 | Final builds |

---

## Recommended slice split (locked)

**Split GP-S25 into two implementation stages:**

| Stage | Name | Why |
| --- | --- | --- |
| **GP-S25A** | Health & Damage Foundation | Units lack ASC today — must land host, init, GE, death before cadence |
| **GP-S25B** | Attack Cadence Integration | Clear operator matrix on top of stable Health/death |

Do **not** implement S25B until S25A operator-accepted.

---

## Decision table

| Topic | Decision | Rationale | Deferred |
| --- | --- | --- | --- |
| Damage mechanism | Instant C++ GE + existing MMC | Matches code/TDD; no BP asset | ExecutionCalculation |
| Formula | Existing MMC (0 floor) | Already implemented | Crit/pen/types |
| Cadence | Immediate-first-hit; AttackCooldown seconds | Responsive RTS; testable | AttackSpeed |
| First-hit timing | Immediate on Ready | Better feel; clear logs | Wind-up |
| Range source | GAS if >0 else component 250 | Avoid zero-range trap | Remove component prop |
| Stat init | UnitBase EditDefaultsOnly + authority SetNumericAttributeBase | No AttributeSet hardcode; no DA required | Definition DA |
| Death ownership | UnitBase HandleDeath + AttributeSet notify via interface | No GPGAS→GPRuntime include cycle | GameMode units count |
| Death delegate | Native `OnUnitDied` | Sync Attack bind | GameplayEvent tag |
| Actor destruction | Delayed LifeSpan | Reentrancy-safe | Corpse/ragdoll |
| Attack reaction | Bind OnUnitDied → TargetDied | Exact; not Destroy-timing | — |
| Attacker death | Owner HandleDeath → command shutdown | Stops Tick/hits | — |
| Replication | Auth apply; Mixed ASC; replicate bIsDead | Project GAS discipline | Prediction/UI |
| Tags | Use `Unit_State_Dead` | Exists | Data.Damage / Event tags |
| Assets | C++ GE class | No Content dependency | GE_GP_Damage_Basic uasset |
| Debug | gp.Combat.* through GE | Real path | — |

---

## Risks

1. **ASC on Pawn replication** — must set Mixed before InitAbilityActorInfo; verify AttributeSet registration order (subobject before BeginPlay init).
2. **AttributeSet → UnitBase cycle** — must use GPGAS interface, not UnitBase include.
3. **Zero defaults** — without init, Damage=0 → no Health change; SetStats/init mandatory for tests.
4. **MMC Resistance=1** — full immune possible; acceptable for S25.
5. **Sync death during Apply** — FinishAttack reentrancy must be bulletproof.
6. **Historical TDD numbering** (S31 GE) — do not renumber TDD; GP-S25 is continuation slice.

---

## Implementation task outline

### S25A
1. Add death sink interface in GPGASRuntime.
2. UnitAttributeSet PostGameplayEffectExecute → notify sink on Health≤0.
3. C++ `UGP_GE_Damage_Basic` + wire MMC.
4. UnitBase: ASC, UnitAttributeSet, ISI, init, defaults, HandleDeath, OnUnitDied, command/move shutdown.
5. Debug combat console.
6. Docs + GPEditor/UHT candidate; operator A1–A6; finalize Dev/Shipping.

### S25B
1. Add `TargetDied` reason; Ready cadence + NextHitTime; EffectiveRange helper.
2. AttemptHit Apply GE; bind/unbind OnUnitDied; owner death notify.
3. Logs AttackHit*.
4. Operator B1–B10; finalize.

---

## Stop condition (analysis)

Docs only. Commit/push `feature/gp-s25-attack-damage-analysis`. Do **not** merge to main. Do **not** start S25A implementation without explicit task. Do **not** add damage C++ on this branch.
