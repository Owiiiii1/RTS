# Roadmap Reconciliation — Post GP-S32R

**Status:** `ROADMAP_RECONCILIATION_POST_GP-S32R_READY_FOR_REVIEW`  
**Audit branch:** `audit/roadmap-reconciliation-post-gp-s32r`  
**Type:** AUDIT-ONLY (no gameplay code, no Content assets)

---

## 1. Baseline SHA

| Fact | Value |
| --- | --- |
| `main` HEAD (audit base) | `2042d4ee395436ce8c0518e829e8cd4d6cd3bc82` |
| Last closed production stage on `main` | **GP-S32R** Orbital Building Drop — merged |
| Prior closed | GP-S31R, GP-S30, GP-S29R, … GP-S28 |
| Engine | UE 5.8.1 |

---

## 2. Historical roadmap summary

Canonical historical sequence in `Docs/TDD/13_Architecture_Proposal.md` (Slice 7–9 excerpt):

| ID | Historical title |
| --- | --- |
| GP-S29 | `UGP_CombatComponent` (LOS 3-trace, fire loop) |
| GP-S30 | `UGP_TargetingComponent` (auto-acquire) |
| GP-S31 | `GE_GP_Damage_Basic`, `GE_GP_Cooldown_Attack` |
| GP-S32 | Attack-move + `AttackMoveDestination` on `AGP_MobileUnit` |
| GP-S33 | `Multicast_PlayAttackVFX` scaffold |
| GP-S34 | `AGP_BuildingBase` + `UGP_BuildingDefinition` |
| GP-S35 | `UGP_BuildGridSubsystem` |
| GP-S36 | Storage launch → Orbital + Score |
| GP-S37 | `UGP_OrbitalDeliverySubsystem` |
| GP-S38 | `AGP_DropPod` + `UGP_OrbitalDropDefinition` |
| GP-S39 | `AGP_MainBase` |
| GP-S40 | `AGP_LogisticsHub` (+5 MaxUnits + storage bonus) |
| GP-S41–S46A | Turret / Wall / WallTurret / reticle / wall drag / Repair / Sell-Demolish |
| GP-S47–S53 | CommonUI/MVVM / FoW / VMs / HUD / Minimap / Order Menu |

**Factual divergence after GP-S28:** reconciliation IDs consumed chronological numbers differently:

| Delivered ID | Actual delivered intent |
| --- | --- |
| **GP-S29R** | Combat LOS + HealthBar + TeamColors + Salvage Walker (not CombatComponent class) |
| **GP-S30** | Container Launch / Orbital Conversion (not TargetingComponent) |
| **GP-S31R** | Minimal Orbital Unit Drop (not Damage/Cooldown GE slice) |
| **GP-S32R** | Orbital Building Purchase / READY / placement / DropPod (not Attack-Move) |

Historical IDs **GP-S30 / GP-S31 / GP-S32** as *architecture titles* are therefore **semantically consumed** by reconciliation work under the same or R-suffixed numbers. Remaining historical *capabilities* must be re-identified when proposed.

---

## 3. Factual implementation matrix (GP-S29 … GP-S53)

Capability vs historical class name are classified separately.

| Hist. ID | Historical intent | Factual implementation | Capability | Architecture | Status |
| --- | --- | --- | --- | --- | --- |
| S29 | CombatComponent LOS+fire | Attack FSM in `UGP_UnitCommandComponent` + `GPCombatLOS` | DONE | SUPERSEDED | **SUPERSEDED** (intent done) |
| S29R* | Combat presentation reconciliation | HealthBar, TeamPresentation, SalvageWalker, LOS contracts | DONE | R-slice | **DONE / MERGED** |
| S30 hist. | TargetingComponent auto-acquire | **No** targeting class; **no** idle auto-engage | NOT STARTED | — | **NOT STARTED** |
| S30 deliv. | *(reconcile)* Container launch | `UGP_StorageComponent` launch + AddOrbital/AddScore GEs | DONE | Renamed use of S30 | **DONE / MERGED** |
| S31 hist. | Damage GE + Cooldown GE | `UGP_GE_Damage_Basic` + MMC; cooldown = **attribute + FSM timer** (no Cooldown GE) | PARTIAL | Deviated | **PARTIAL** |
| S31R* | Orbital unit drop | Settings + DropPod unit payload + TEMP HUD + SpendOrbital | DONE | R-slice | **DONE / MERGED** |
| S32 hist. | Attack-Move destination FSM | Tag `GP.Command.AttackMove` only; PC validate rejects non Move/Attack/Mine | NOT STARTED | Tag only | **NOT STARTED** |
| S32R* | Orbital building drop | READY inventory + ghost + building DropPod + LogisticsHub identity | DONE | R-slice | **DONE / MERGED** |
| S33 | Attack VFX multicast | `UGP_CombatPresentationComponent::Multicast_CombatPresentationEvent` | DONE | Renamed | **DONE** |
| S34 | BuildingBase + BuildingDefinition | `AGP_BuildingBase` exists (minimal); **no** `UGP_BuildingDefinition` | PARTIAL | Base ahead | **PARTIAL** |
| S35 | BuildGridSubsystem | Absent; interim placement in `GPBuildingDropAuthority` | NOT STARTED | Interim only | **NOT STARTED** |
| S36 | Storage launch | Delivered as GP-S30 | DONE | — | **DONE** |
| S37 | OrbitalDeliverySubsystem | Authority helpers + settings (no subsystem UObject) | PARTIAL | SUPERSEDED shape | **SUPERSEDED** (equiv. PARTIAL) |
| S38 | DropPod + DropDefinition DA | `AGP_DropPod` unit+building; catalog = `UGP_OrbitalDeliverySettings` soft refs | PARTIAL | No DA | **PARTIAL** |
| S39 | MainBase | `AGP_MainBase` + Storage + UnitDropZone + registry | DONE (MVP host) | — | **DONE** (MVP) |
| S40 | LogisticsHub + bonuses | `AGP_LogisticsHub` identity/spawn; **no** +5 MaxUnits / storage bonus | PARTIAL | Bonuses deferred | **PARTIAL** |
| S41 | DefensiveTurret | Tag only | NOT STARTED | — | **NOT STARTED** |
| S42 | Wall + connection | Tag only | NOT STARTED | — | **NOT STARTED** |
| S43 | WallTurret | Tag only | NOT STARTED | — | **NOT STARTED** |
| S44 | DropReticle / GhostWall | `AGP_BuildingPlacementGhost` Engine cube; no wall ghost | PARTIAL | TEMP cube | **PARTIAL** |
| S45 | Wall drag-build | Absent | NOT STARTED | — | **NOT STARTED** |
| S46 | Repair GA | Tag only | NOT STARTED | — | **NOT STARTED** |
| S46A | Sell / Demolish | Tags only | NOT STARTED | — | **NOT STARTED** |
| S47 | CommonUI + widget bases | Plugins/deps present; GPUIRuntime stub; no widget bases | PARTIAL | — | **PARTIAL** |
| S48 | FoW | Absent | NOT STARTED | — | **NOT STARTED** |
| S49–S50 | ViewModels / adapters | Absent | NOT STARTED | — | **NOT STARTED** |
| S51 | Match HUD | `UGP_TEMP_S28P_PlanetaryFerroniteHUD` only | PARTIAL | TEMP | **PARTIAL** |
| S52 | Minimap | Absent | NOT STARTED | — | **NOT STARTED** |
| S53 | Order Menu | Absent; TEMP purchase/drop panels only | NOT STARTED | — | **NOT STARTED** |

\*Reconciliation / delivered project IDs (not historical TDD/13 titles).

---

## 4. Combat reconciliation

| Concern | Historical | Factual | Action |
| --- | --- | --- | --- |
| Fire loop + LOS | CombatComponent | UnitCommand Attack FSM + `GPCombatLOS` 3-point gate | Keep; do not resurrect CombatComponent |
| Auto-acquire | TargetingComponent | **Missing** | **NEXT gap (see §10)** |
| Damage | GE_Damage_Basic | Present | None |
| Attack cooldown | Cooldown GE | Attribute `AttackCooldown` + hit timer | Accept deviation unless GAS CD required later |
| Attack-Move | MobileUnit destination | Tag registered; unsupported in validate | Later QoL after acquire |
| Attack VFX | Multicast_PlayAttackVFX | CombatPresentation multicast | Done (scaffold) |
| Salvage Walker | Combat unit | `AGP_SalvageWalker` + orbital payload | Done |
| Health / team | — | HealthBar + TeamPresentation | Done (S29R) |

**Attack-Move MVP question (explicit):**

- GDD/09 lists hotkey `A` → Attack-move mode.
- GDD/04 Salvage Walker `AllowedCommands` lists **Move, Stop, Attack** — **not** AttackMove.
- SW Behavior explicitly requires **auto-target in AttackRange**.
- Factual code: Attack + Move playable; AttackMove **not** executed.

**Verdict:** Attack-Move is **desirable RTS QoL**, documented in UI, but **not the earliest canonical combat gap**. Auto-acquire is the stronger GDD unit-behavior requirement. Attack-Move should follow once engage-on-contact / acquire exists (A-move depends on it). Do **not** implement Attack-Move solely because historical GP-S32 says so.

---

## 5. Resource / economy reconciliation

| Loop step | Status |
| --- | --- |
| Mine → Cargo → MainBase Storage | DONE |
| Ready container → Launch → Orbital + Score + Threat↓ | DONE (GP-S30) |
| Spend Orbital → Unit DropPod → units | DONE (GP-S31R) |
| Spend Orbital → Building READY → ghost → DropPod → building | DONE (GP-S32R) |
| `MaxUnits` / `CurrentUnits` | Attrs exist; drop authority can reject when MaxUnits **> 0**; default soft-open (**MaxUnits≈0 ⇒ cap inactive**) |
| Logistics Hub +5 / storage bonus | **NOT implemented** (hub is identity-only) |
| Win evaluation from Score / DeliveryQuota | Scaffold fields + `SetMatchResult`; full timer→winner wiring still a later match-flow gap |

Economy acquisition half of ADR-0009 is closed for MVP units + one building type. Cap progression via Hub bonuses remains deferred and is **not** blocking while MaxUnits soft-open remains 0.

---

## 6. Building / orbital reconciliation

| Historical S34–S40 responsibility | Status after GP-S32R |
| --- | --- |
| BuildingBase ancestor | **DONE** (`AGP_BuildingBase`) |
| BuildingDefinition DataAsset | **NOT STARTED** (settings catalog instead) |
| BuildGrid | **NOT STARTED** (interim radius + capsule overlap) |
| Storage launch | **DONE** (as S30) |
| Orbital delivery authority | **DONE** (helpers + settings; no subsystem class) |
| DropPod lifecycle | **DONE** (shared unit/building) |
| OrbitalDropDefinition DA | **NOT STARTED** (settings soft classes) |
| MainBase host | **DONE** (MVP) |
| LogisticsHub spawn/identity | **DONE** |
| LogisticsHub gameplay bonuses | **NOT STARTED** |
| Building ghost | **PARTIAL** (TEMP cube) |

**Do not** next-slice duplicate orbital delivery. BuildingDefinition / BuildGrid / Hub bonuses / turrets-walls are separate later slices. Preferred building follow-on after combat QoL: **LogisticsHub bonuses** (when activating MaxUnits baseline) or BuildGrid when walls/turrets need cells — not before.

---

## 7. UI / FoW reconciliation

| Item | Status |
| --- | --- |
| TEMP planetary/orbital/unit/building HUD | PARTIAL / playable |
| CommonUI + MVVM widget bases | PARTIAL (plugins only) |
| FoW (ADR scope / GDD/11) | NOT STARTED |
| Order Menu / VMs / Minimap | NOT STARTED |

Production UI/FoW is **category D** — after core command/economy gaps.

---

## 8. Intentional architecture deviations

1. **No `UGP_CombatComponent`** — Attack lives in `UGP_UnitCommandComponent` (validated S29R).
2. **No `UGP_OrbitalDeliverySubsystem`** — authority namespaces + DeveloperSettings.
3. **No `UGP_OrbitalDropDefinition` / `UGP_BuildingDefinition` yet** — soft class + cost keys on `UGP_OrbitalDeliverySettings`.
4. **No BuildGrid** — `INTERIM_MVP_PLACEMENT_VALIDATION`.
5. **Cooldown cooldown** via attribute/timer, not Cooldown GE.
6. **Chronological IDs reused** for reconciliation (S30/S31R/S32R ≠ historical TDD titles).

These are accepted unless a later ADR overturns them.

---

## 9. Remaining MVP gaps (dependency order)

| Pri | Gap | Category | Notes |
| --- | --- | --- | --- |
| 1 | **Combat auto-acquire** | A | GDD SW behavior; missing entirely |
| 2 | Attack-Move command + hotkey | A | UI hotkey; needs acquire for engage-along-path |
| 3 | MaxUnits baseline + LogisticsHub +5 (and optional storage bonus) | B/C | Hub currently inert; soft-open cap |
| 4 | Match win wiring (timer / quota → `FinishMatch`) | B | Score exists; end condition incomplete |
| 5 | BuildGrid + FoW-aware placement | C/D | When multi-building / walls |
| 6 | DefensiveTurret / Walls | C | After grid + targeting |
| 7 | Production Order Menu / MVVM HUD | D | Replace TEMP |
| 8 | FoW gameplay | D | GDD/11 |
| 9 | AI opponent | E | Later |
| 10 | Session / Steam | F | Later |

---

## 10. Recommended NEXT production slice

**Exactly one:**

### GP-S30R — Combat Auto-Acquire

*(Reconciliation ID: historical TDD GP-S30 Targeting intent; chronological GP-S30 already = Container Launch.)*

---

## 11. Why this is next

1. **Category A before C/D:** Core combat unit behavior still missing after economy/orbital vertical closed (S30→S32R).
2. **GDD/04 Salvage Walker Behavior** requires auto-target in AttackRange; factual code has none.
3. **Smallest vertical:** Reuse existing Attack FSM + LOS + damage; do not invent CombatComponent or full BuildingDefinition.
4. **Unblocks Attack-Move** as a later thin slice (engage-while-moving needs acquire).
5. **Does not duplicate** GP-S32R delivery / ghost / READY work.
6. Historical numeric “next” GP-S34 (BuildingDefinition) would be **wrong** — BuildingBase exists and delivery already works; Definition/Grid are not the earliest playable gap.

---

## 12. Explicit NOT-NEXT items

- Full `UGP_BuildingDefinition` / multi-type catalog  
- BuildGrid / FoW placement  
- LogisticsHub +5 / storage bonuses (follow after acquire or with MaxUnits activation)  
- Attack-Move (follow-on after acquire)  
- DefensiveTurret / Wall / WallTurret  
- Real-mesh building ghost  
- Production Order Menu / CommonUI HUD rewrite  
- FoW / Minimap  
- AI / Steam  
- Resurrecting `UGP_CombatComponent`  

---

## 13. Proposed slice ID / name

| Field | Value |
| --- | --- |
| **ID** | **GP-S30R** |
| **Name** | Combat Auto-Acquire |
| **Historical mapping** | TDD/13 GP-S30 `UGP_TargetingComponent` intent (capability), not the chronological GP-S30 launch slice |

---

## 14. Acceptance boundary (proposed next slice)

### Goal
Combat-capable units (at minimum Salvage Walker) **server-authoritatively auto-engage** a valid enemy in AttackRange when not already executing a higher-priority command, using the **existing** Attack FSM / LOS / damage path.

### In scope (sketch)
- Auto-acquire scan (rate-limited) while Idle (and optionally while pure Move — only if trivial; else Idle-only MVP)
- Target filters: living enemy units (team ≠ self); buildings optional if cheap
- Priority stub: nearest enemy unit (SWARM priority deferred)
- Explicit `Command_Attack` still forces specific target
- Stop / new Move cancels auto-engage cleanly
- Diagnostics + contract test (acquire → engage → LOS/damage path unchanged)
- Architecture: prefer UnitCommand (or minimal helper), **not** mandatory new `UGP_TargetingComponent` class unless it is the cleanest seam

### Out of scope
- Attack-Move command / `A` hotkey mode  
- Full historical TargetingComponent API surface / priority matrix  
- FoW visibility gate for acquire  
- Turret AI  
- BuildingDefinition / BuildGrid / Hub bonuses  
- UI Order Menu  

### Operator acceptance sketch
1. Spawn/drop friendly SW + enemy unit in range.  
2. With SW Idle (no Attack click), SW auto-acquires and fires (LOS permitting).  
3. Issue Move away → disengages.  
4. Issue explicit Attack on chosen target → respects that target.  
5. Existing Attack / LOS / unit-drop / building-drop regressions still Failures=0.

---

## Documents / code inspected

**Docs:** README, DOCUMENTATION_INDEX, AI_Project_Log, TDD/13, GDD/04/05/06/09/10, First_Playable_Match (index), TDD/03/05/06/07/14/15, ADR-0009, ADR-0006, prior audits Post-S29R / Post-S30, Claude tasks S29R–S32R.

**Code (factual):** `GPUnitCommandComponent`, `GPCombatLOS`, `GPCommandComponent`, `GPSalvageWalker`, HealthBar/TeamPresentation/CombatPresentation, `GPBuildingBase`/`GPLogisticsHub`/`GPMainBase`, `GPDropPod`, `GPOrbitalDeliverySettings`, `GPBuildingDropAuthority`/`GPUnitDropAuthority`, `GPBuildingPlacementGhost`, `GPPlayerAttributeSet`, TEMP HUD, GPUIRuntime stub, gameplay tags.
