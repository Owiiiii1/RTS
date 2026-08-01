# Orbital Delivery — Mechanics Review (Post-Audit Skill Pass)

Цей документ — результат двох skill-passes, виконаних після узгодження документації з Orbital Delivery pivot (per `grim_protocol_docs_audit_v2.md` + [ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md)):

1. **Pass 1 — `game-design-framework`** (5-Component Filter, State Machine Checklist, Numbers Policy).
2. **Pass 2 — `gp-mechanics-validator`** (22 categories, 17 anti-patterns, production tier, verdict).

Scope оцінки — поточна canonical модель: mining → MainBase containers → launch → OrbitalFerronite + FerroniteScore, orbital orders → drop pods, SWARM pressure = `FerroniteThreatValue`, win = delivery quota / highest FerroniteScore, Worker repair у MVP.

---

# Pass 1 — Game Design Framework

## 5-Component Filter — Core Orbital Loop

| Component | Result | Evidence |
| --- | --- | --- |
| **Clarity** | Strong | Drop telegraph 2-3 s (light beam + descent VFX + minimap marker, per TDD/14). Container fill/launch states visible на HUD. Threat bar (`FerroniteThreatValue`) readable. |
| **Motivation** | Strong (1 risk) | Greed-vs-safety: накопичений Planetary Ferronite = score potential, але піднімає swarm pressure; launch = safety. **Risk:** at match-end clarity (див. Numbers Policy / escalation floor). |
| **Response** | Strong | Order → target → click → spend → pod. Targeting mode cancel (Esc/RMB) без spend. Reject feedback per-reason. |
| **Satisfaction** | Strong | Layered telegraph + impact (camera shake, scorch ring), `+N` flash на OrbitalFerronite/FerroniteScore, auto-highlight нового asset. ≥ 2 feedback channels на significant action. |
| **Fit** | Strong | Helldivers reference fantasy, industrial extraction tone, no military RTS drift. |

**Conflict priority (Response > Clarity > Satisfaction > Fit > Motivation):** no conflicts — усі компоненти Strong; єдиний відкритий пункт — Motivation на climax (escalation floor), не конфлікт між компонентами.

## State Machine Checklist

### Worker Repair (STAYS in MVP) — complete

| Property | Definition | Source |
| --- | --- | --- |
| Entry | RMB на own-team damaged target (`Health < MaxHealth`), Worker selected | TDD/05 §Repair |
| Exit | Target full HP, OR target out of range, OR `OrbitalFerronite` = 0 | TDD/05 scenarios 6-7 |
| Interruptibility | New command перериває; out-of-orbital cancels | TDD/05 |
| Chained | Після repair Worker → Idle / resume mining (auto-cycle) | TDD/05 |
| Cost | `GE_GP_Cost_RepairTick` per tick (`OrbitalFerronite -= TickCost`, TBD) | TDD/05, GDD/04 |
| Edge cases | Own undamaged building → smart-command resolves to Move (guard); target dies mid-repair → ability ends | TDD/05 §smart-command |

**Verdict:** Repair state machine fully specified. No gaps.

### Orbital Drop / Targeting — complete

Entry (Order Menu select) → targeting mode → valid click (server validate spend + FoW + grid) → pod scheduled → land → asset; exit on Esc/RMB (no spend) or reject (no spend). Edge cases enumerated у TDD/14 (`EGP_OrderRejectReason` 12 reasons). No gaps.

## Numbers Policy Audit

- Усі balance numbers — DataAsset placeholders, позначені TBD (per ADR-0002 + memory rule). **Compliant** — no "industry standard" claims.
- **Applied (Numbers Policy → "starting value + test plan"):** SWARM escalation floor question зафіксовано у GDD/06, GDD/12, TDD/07 з framing "starting value + test plan in balance pass, no firm numbers". Це закриває Numbers Policy gap, який створює FerroniteThreatValue-flip.

## Debugging Priorities (if it feels wrong)

1. "I don't care at the end" → **Motivation/escalation** — resolve via escalation-floor balance pass (не tune numbers першим; це structural).
2. "Didn't know wave was coming" → **Clarity** — verify threat-bar + wave telegraph before tuning wave size.
3. "Drop feels laggy" → **Response** — verify 200 ms green pulse acknowledges intent before pod spawn.

## Definition of Done — Core Loop

- [x] 5-Component Filter evaluated.
- [x] State Machine Checklist complete (Repair, Orbital Drop).
- [x] Edge cases enumerated (reject reasons, storage-full, out-of-orbital).
- [x] ≥ 2 feedback channels на significant action (TDD/12, TDD/14, GDD/12).
- [x] Playtest scripts present (TDD/14, TDD/07, TDD/05).
- [x] Numbers justified per Numbers Policy (TBD placeholders + escalation-floor test plan).

---

# Pass 2 — GP Mechanics Validator

# Mechanics Review — Orbital Delivery Core Loop (post-pivot)

## Domain
resource/economy + building + match flow + UI + Steam/multiplayer (cross-domain core loop).

## Core Loop Mapping
`Read` (deposits, container state, threat bar, FoW) → `Select` (Workers, MainBase) → `Command` (Mine, OrderDrop, Repair) → `Validate` (server: ownership, OrbitalFerronite, FoW, grid) → `Resolve` (container fill → launch → GE_GP_AddOrbital/AddScore; pod land) → `Feedback` (telegraph, flashes, threat bar) → `Escalate` (FerroniteThreatValue ↑ → SWARM, delivery quota → win).

## Player Goal
Видобути Ferronite, відвантажити на орбіту заради FerroniteScore (delivery quota), при цьому утримуючи swarm pressure (стимульований сирим Ferronite на базі) і opponent під контролем.

## System Rules
No local production/construction. Усі non-initial assets — orbital drop. Planetary Ferronite (containers, не spendable) → launch → OrbitalFerronite (spendable) + FerroniteScore (victory). `FerroniteThreatValue` = raw stored-at-base stock: ↑ на drop-off, ↓ на launch; драйвить wave intensity. Failure: storage full → drop-off lost; MainBase destroyed → loss (if `bAnnihilationCountsAsWin`).

## Authority Model
Client = intent (`Server_RequestOrbitalDrop`, `Server_RequestCommand`). Server = validation + spend (`GE_GP_SpendOrbital`) + pod spawn + payload spawn + container state machine + threat tracking. AI (`AGP_AIController : AAIController`) викликає той самий server-side command layer напряму (no client RPC). UI — read-only через MVVM ViewModels (`UGP_OrderMenuVM`, storage VM); local-only: drop reticle, selection.

## Data Requirements
- Data Assets: `DA_GP_OrbitalDrop_*`, `DA_GP_Building_{MainBase,LogisticsHub,DefensiveTurret,Wall,FerroniteDeposit}`, `DA_GP_Unit_{Worker,SalvageWalker}`, `DA_GP_Resource_Ferronite`, `DA_GP_Session_Default`.
- Gameplay Tags: `GP.Command.{OrderDrop,Repair,Move,Stop,Attack,Mine,Sell,Demolish}`, `GP.Drop.Type.{Unit,Building,Wall}`, `GP.State.PodInFlight`, `GP.Building.Type.*`, `GP.Match.WinReason.*`, `GP.Resource.Type.Ferronite`.
- Attributes: `UGP_PlayerAttributeSet.{OrbitalFerronite (COND_OwnerOnly), FerroniteScore (COND_None), MaxUnits, CurrentUnits}`; `AGP_GameState.FerroniteThreatValue`; `UGP_UnitAttributeSet.CarriedFerronite`.
- UI state: OrbitalFerronite, FerroniteScore (own+opponent), container fill/launch, in-flight pods, threat bar, FoW.

## 5-Component Check
| Component | Result |
| --- | --- |
| Clarity | Strong — telegraph + container/threat readouts. |
| Motivation | Risk — climax motivation залежить від escalation floor (A14, див. Risks). |
| Response | Strong — intent→validate→resolve, cancelable targeting. |
| Satisfaction | Strong — multi-channel reward на launch + drop. |
| Fit | Strong — extraction/Helldivers identity. |

## Production Cost vs Value
**Owner domains:** `GPRuntime H` (subsystem + storage + threat), `GPGASRuntime M` (orbital/score/repair GEs), `GPUIRuntime M` (Order Menu VM, threat/container HUD), `Data Assets M`, `Tags L`, `VFX/audio M` (pod telegraph), `Steam/session L` (симетрично для AI/human), `QA M`.

**Tier:** Medium.

Cost виправданий: одна subsystem замінює production+construction component family (net simplification per ADR-0009), DataAsset-driven catalog масштабується без коду, симетрична модель для AI прибирає окремий AI шлях.

## Risks
- **A14 SWARM-Playability Drift (Risk, not blocking):** FerroniteThreatValue-flip означає, що fast-shipper тримає threat ≈ 0 → SWARM може ніколи не тиснути / матч не ескалює. Mitigation вже зафіксовано як escalation-floor design-TBD (GDD/06, GDD/12, TDD/07) + threat telegraph + readable threat bar. Має бути закрито у balance pass перед first playable sign-off.
- **A11-A13, A15-A17:** clean. Single resource (two states ≠ multi-resource, A15 clean), capacity strategic via Logistics Hub (A16 clean), no hero units (A17 clean), no military/time-travel drift.
- **A2 Client authority leak:** clean — spend/spawn server-only.
- **A9 UI owns gameplay state:** clean — MVVM read-only, server updates VMs.
- **A3 Data-less tuning:** clean — TBD placeholders у DataAssets.

## Playtest Scenarios
- New player test: чи зрозуміло, що сирий Ferronite на базі притягує SWARM і що launch знижує тиск (без tooltip)?
- Stress test: spam orders при pod cap; всі containers Ready (storage full); масовий late-game shipping race.
- Abuse test: fast-ship-everything стратегія — чи матч лишається грабельним без escalation floor (валідація A14)?
- Readability test: спостерігач бачить хто веде (FerroniteScore visible), чому прийшов wave (threat bar), що падає (pod silhouette).

## Recommendations
1. Закрити escalation-floor питання у balance pass: задати starting value для secondary global threat baseline + pass/fail метрику (matches that never reach "Crisis" state).
2. Verify, що HUD threat bar явно показує напрямок (зростає на drop-off, спадає на launch) — інакше Clarity на core risk loop падає.
3. Тримати `FerroniteScore` (victory) і `OrbitalFerronite` (spend) візуально різними у HUD, щоб гравець не плутав score зі spendable currency.
4. Підтвердити symmetric AI використовує той самий orbital шлях у Steam 2-player playtest (no special-case AI economy).

## Per-Category Tags
| # | Category | Tag | One-line evidence |
| --- | --- | --- | --- |
| 1 | MVP Alignment | Strong | Core loop = MVP scope per ADR-0009. |
| 2 | RTS Verb | Strong | Mine / Order / Repair / Move / Attack. |
| 3 | Clarity | Strong | Telegraph + container/threat readouts. |
| 4 | Motivation | Risk | Climax залежить від escalation floor (A14). |
| 5 | Response | Strong | Cancelable targeting, intent ack. |
| 6 | Satisfaction | Strong | Multi-channel launch/drop reward. |
| 7 | Fit | Strong | Extraction/Helldivers identity. |
| 8 | Multiplayer Authority | Strong | Server-auth spend/spawn; shared command layer. |
| 9 | GAS Fit | Strong | Spend/income/repair via GE. |
| 10 | Data-Driven Fit | Strong | Soft refs + DA catalog. |
| 11 | Gameplay Tags | Strong | OrderDrop/Drop.Type/WinReason registry. |
| 12 | Production Cost | Strong | Net simplification vs production family. |
| 13 | UI/UX Surface | Strong | MVVM Order Menu + threat/container HUD. |
| 14 | Playtestability | Strong | Scenarios + cheats (GDD/12). |
| 15 | Scope Control | Strong | Out-of-MVP lists guard creep. |
| 16 | Industrial Extraction Fit | Strong | Mining/shipping = score driver. |
| 17 | Engineer-Not-Soldier Fit | Strong | Worker = mine/transport/repair, no build/no combat. |
| 18 | One-Resource Fit | Strong | Single Ferronite, two states. |
| 19 | Capacity Strategic Fit | Strong | MaxUnits via Logistics Hub drop. |
| 20 | Corporate Identity Fit | Strong | Orbital expedition tone. |
| 21 | SWARM Boundaries | Risk | Threat model needs escalation floor (A14). |
| 22 | Animation Budget Fit | Strong | Worker drone, limited keyframes. |

## Final Verdict
**Excellent Core Loop Fit, Strong MVP Slice** — з єдиним відкритим Risk: **SWARM escalation floor (A14)** має бути закритий у balance pass. Не review-blocking (немає pillar violation), але обов'язковий до first-playable sign-off.

---

## Applied Changes (from both passes)

Зміни, які surface-нули skill-passes, вже внесені у canonical docs під час цього pass:

- SWARM escalation-floor design question (Numbers Policy framing) — GDD/06, GDD/12, TDD/07.
- Worker Repair state machine (entry/exit/interrupt/cost/edge) — TDD/05, GDD/04.
- HUD threat readout `FerroniteThreatValue` + score/currency separation — GDD/09, TDD/12.
- Symmetric AI orbital path (no client RPC) — TDD/03, GDD/03, ADR_0008.

Цей документ — captured review (артефакт passes). Жодних numeric balance values не вигадано; усі — TBD у balance pass.
