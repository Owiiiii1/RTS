# Post-S29R Next Slice Audit

> **Historical audit.** Numbered-wave rows below are **superseded**. Canonical SWARM concept: [`../GDD/14_SWARM.md`](../GDD/14_SWARM.md). Runtime still not started.

## Baseline

| Fact | Value |
| --- | --- |
| Branch (audit) | `audit/post-s29r-next-slice` |
| `main` HEAD | `3673a6891b3638592da115887d95e373d2475b1e` |
| Last closed stage | **GP-S29R** — MERGED / CLOSED |
| Operator validation | **PASS** (Team Colors, Health Bar, Salvage Walker, Combat, LOS, LOS log spam fix) |
| Final builds | GPEditor Dev+UHT / GP Win64 Development / GP Win64 Shipping — **PASS** |
| Production code in this audit | **None** (docs/planning only) |

GP-S29R closed Slice 7 reconciliation combat presentation: LOS fire gate, health bar, team colors, native `AGP_SalvageWalker`, Details UX cleanup, transition LOS diagnostics. Attack execution remains in `UGP_UnitCommandComponent` (not a new CombatComponent).

---

## Implemented MVP Surface

After GP-S29R on `main`:

- **Foundation / GAS:** tags, ASC, Player + Unit AttributeSets (incl. `OrbitalFerronite`, `FerroniteScore`), damage GE path, death.
- **Control:** CameraPawn, selection, smart commands → Move / Attack / Mine.
- **Movement:** server-authoritative straight-line `UGP_MovementComponent`; NavMesh used narrowly for resource approach / haul reachability queries — **not** general RTS pathfollowing.
- **Economy (planetary half):** ResourceNode, Mining, Cargo, Worker haul, MainBase `UGP_StorageComponent` (Empty/Filling/Ready; Launching = debug scaffold only). Threat rises on drop-off.
- **Combat:** UnitCommand Attack FSM (approach/Ready/cadence/damage) + 3-point LOS gate + combat presentation; Salvage Walker playable class; health bars; team colors.
- **Match scaffold:** GameState match timer / threat / win-reason fields exist; timer expiry → score evaluation still an integration gap.
- **UI:** TEMP planetary Ferronite HUD only; no Order Menu / MVVM resource VMs.
- **Absent:** OrbitalDeliverySubsystem, DropPod, production launch→Orbital/Score conversion path, Logistics Hub actor, TargetingComponent, AttackMove executor, FoW, SWARM, AI opponent, Steam lobby, Repair GA.
  - Note: `OrbitalFerronite` / `FerroniteScore` **attributes** exist on `UGP_PlayerAttributeSet`. Documented names `GE_GP_AddOrbital` / `GE_GP_AddScore` appear in TDD/ADR but are **not** confirmed as existing production GE classes/assets — **implementation-time verification required** (reuse if present; otherwise create minimal Instant GEs in GP-S30).

Canonical GDD loop still blocked at **container launch** (no spendable OrbitalFerronite / FerroniteScore from play).

---

## Missing MVP Systems

| System | Current State | Dependency | MVP Priority | Notes |
| --- | --- | --- | --- | --- |
| Container launch → OrbitalFerronite + FerroniteScore | Scaffold only (`Ready` / debug Launching) | Storage S28 complete | **Critical** | Unlocks score + spendable currency (ADR-0009) |
| OrbitalDeliverySubsystem + DropPod + Order Menu | Absent | Launch + OrbitalFerronite | Critical (next after launch) | Acquisition path for units/buildings |
| Logistics Hub / walls / turrets / build grid | Absent / tags | Orbital drops | High | Slice 8 content after delivery plumbing |
| Match win wiring (DeliveryQuota / timer score) | Partial timer only | FerroniteScore from launch | High | Needs score to mean anything |
| General NavMesh pathfollowing / LOS reposition | Straight-line only | Optional QoL | Medium | Explicitly deferred after S29R LOS hold behavior |
| TargetingComponent / AttackMove | Tags / docs only | Combat FSM exists | Medium | Improves combat QoL; does **not** unlock economy |
| CombatComponent (TDD/13 S29) | **Superseded** | — | N/A | Attack lives in UnitCommand; do not duplicate |
| SWARM / FerroniteThreatValue | Threat tracked; no director | Threat + launch relief | High (after launch) | Numbered waves **superseded**; continuous pressure in GDD/14 |
| AI opponent (`AGP_AIController`) | Absent | Economy + commands | High later | ADR-0008 MVP; needs ship/order loop |
| FoW / minimap layers | Absent | Presentation + net | Medium–High | GDD/11 MVP; not economy unlock |
| Steam lobby / session | LobbyState scaffold | Match flow | Later | Slice 12 |
| Repair | Tags only | Orbital cost GE | Medium | Worker capability; not next unlock |
| Full CommonUI MVVM HUD | TEMP only | Resource/score VMs | Medium | After launch readouts exist |

---

## Candidate Next Slices

### C1 — Container launch / orbital conversion (economy unlock)

- **Scope:** Ready container → launch telegraph → mutate OrbitalFerronite + FerroniteScore via canonical Instant GAS GEs (verify/reuse or create AddOrbital/AddScore path), lower `FerroniteThreatValue`, presentation stub.
- **Pros:** Closes GDD First_Playable gap; builds on S28 Storage; ADR-0009 already Accepted; no Attack FSM duplication; minimal temporary code.
- **Cons:** Does not yet enable Order Menu / drops (follow-on slice).

### C2 — General navigation / pathfinding

- **Scope:** Replace or wrap straight-line Move/Attack approach with NavMesh pathfollowing / obstacle avoidance; optional LOS reposition later.
- **Pros:** Improves combat around blockers (S29R temporary hold-and-wait).
- **Cons:** Does not unlock score/currency/win; larger movement architecture change; S29R explicitly deferred repositioning.

### C3 — Targeting / AttackMove (historical TDD Slice 7 remainder)

- **Scope:** Auto-acquire and/or AttackMove destination FSM.
- **Pros:** Matches older TDD/13 labels GP-S30–S32.
- **Cons:** Combat already playable; does not unlock MVP economy; risk of inventing CombatComponent-era duplication; lower playable-loop leverage than launch.

### C4 — Full OrbitalDelivery + DropPod + Order Menu

- **Scope:** Entire Slice 8 delivery stack in one go.
- **Pros:** Full acquisition fantasy.
- **Cons:** Too wide for one slice; launch GE path is a natural thinner first vertical.

---

## Recommended Next Slice

**GP-S30 — Container Launch / Orbital Conversion**

### Exact scope (proposal)

- Production path: MainBase containers in `Ready` can launch to orbit (authority).
- On successful launch: Planetary volume leaves storage; apply Instant GAS GEs on owning PlayerState ASC to increase `OrbitalFerronite` + `FerroniteScore` (confirm whether production AddOrbital/AddScore GEs already exist — reuse or create minimal canonical GEs; no direct attribute Set/Add bypass); lower team `FerroniteThreatValue`.
- Minimal launch telegraph / state (`Launching` → complete) using existing Storage model; DA-driven rates/duration (no hardcoded balance).
- Diagnostics + non-shipping contract covering Ready→launch→attrs/threat.
- Operator-visible: fill container via existing Worker loop → launch → OrbitalFerronite and FerroniteScore increase; Threat decreases.

### Why now

1. **Dependency order:** Planetary mine→store is done; everything above (drops, win quota, AI ship/order, SWARM relief fantasy) needs launch conversion.
2. **Playable value:** First closed half of ADR-0009 two-state Ferronite loop.
3. **MVP priority:** First_Playable_Match early/mid game centers on shipping containers.
4. **Minimal temporary code:** Storage already has Ready/Launching scaffold; attributes already exist.
5. **Unlocks:** spendable currency for Order Menu; score for win wiring; Threat down on launch for SWARM pressure fantasy.

### Dependencies

- GP-S28 Storage + Worker haul (merged)
- ADR-0009 Accepted
- GDD/06 + TDD/07 Container Update

### Out of scope (for S30)

- `UGP_OrbitalDeliverySubsystem` / `AGP_DropPod` / Order Menu UI
- Logistics Hub / walls / grid
- Pathfinding / AttackMove / TargetingComponent / CombatComponent
- SWARM continuous pressure / AI opponent / FoW / Steam (SWARM waves **superseded**)
- Match DeliveryQuota evaluation (can follow once score is live)
- Changing S29R LOS hold-and-retry semantics

### Operator-visible acceptance target

Worker fills MainBase container → container reaches Ready → player/authority launches → OrbitalFerronite and FerroniteScore increase; FerroniteThreatValue decreases; planetary storage decreases; no new operator content assets required beyond existing arena/Worker flow.

---

## Pillar 8 MVP Gate

| # | Question | Verdict |
| --- | --- | --- |
| 1 | Fun now? | **Yes** — first visible “ship to orbit → get currency/score” payoff |
| 2 | Clear to new player? | **Yes** — fill box, launch, get spendable + score |
| 3 | New decision type? | **Yes** — when to launch (bank score / get currency / reduce Threat) vs keep stockpiling |
| 4 | Cheap & fast? | **Yes** — Storage Ready + existing attrs; one vertical GE path |
| 5 | Scales via content? | **Yes** — conversion rates / telegraph via DataAsset |

**Gate verdict: PASS** — recommend materializing GP-S30 code task as SPEC only until approval.

---

## Architecture Reconciliation

| Topic | Still valid | Superseded | Deferred |
| --- | --- | --- | --- |
| ADR-0009 orbital two-state Ferronite | ✓ | | |
| Attack in UnitCommand FSM (S24/S25/S29R) | ✓ | TDD/13 `UGP_CombatComponent` as required S29 | |
| 3-point LOS fire gate | ✓ (S29R) | | LOS reposition / pathfinding around blockers |
| `UGP_TargetingComponent` / AttackMove | | as **immediate** next after S29R | later combat QoL slices |
| Storage launch → Orbital/Score Instant GE path (TDD historical S36 label) | ✓ as **next playable vertical**; attrs exist; specific AddOrbital/AddScore GE assets need factual verify-at-impl | chronological ID is **GP-S30** now | full DropPod catalog |
| Straight-line movement | ✓ interim | | general NavMesh pathfollowing |
| Local production / Barracks build | | ADR-0009 | — |
| Soldier/Trooper unit types | | GDD/04 Salvage Walker | — |

**Numbering note:** Historical TDD/13 mapped `GP-S30` → TargetingComponent. Chronological project IDs already used S28 / S29R. **Next ID is GP-S30** for the next approved stage. Targeting/AttackMove must be renamed clearly when materialized later (e.g. `GP-S3x_AttackMove_…`) — do not silently revive old S30 meaning.

---

## Proposed Task Identity

- Audit: [`Docs/Development/Next_Slice_Audit_Post_S29R.md`](Next_Slice_Audit_Post_S29R.md) (this file)
- Spec: [`Docs/Development/Claude_Tasks/GP-S30_Container_Launch_Orbital_Conversion.md`](Claude_Tasks/GP-S30_Container_Launch_Orbital_Conversion.md)
- Status: **SPEC_READY_FOR_APPROVAL**
- Code Allowed: **NO** until explicit tech-lead/operator approval

**No new ADR required** before GP-S30 (ADR-0009 already Accepted). Optional thin clarification during kickoff if launch telegraph/overflow edges need DA defaults — not a blocking design stage.

---

## Stop Condition

Planning complete. Status cursors synced (GP-S29R DONE).  
**NO production implementation** until tech-lead/operator explicitly approves GP-S30.  
Do **not** merge this audit branch without review. Do **not** start pathfinding / Targeting / DropPod in the same PR as launch.
