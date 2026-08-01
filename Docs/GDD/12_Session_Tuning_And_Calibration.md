# Session Tuning and Calibration

Production document для команди. Що ми крутимо щоб налаштувати сесію, як ми тестуємо, як ми реагуємо на результати плейтестів, що залишається стабільним і що змінюється швидко.

**Не** теорія геймдизайну. **Так** — operational guide для дизайнерів, балансерів і tech lead-а.

> **Working principle:** усі параметри живуть у Data Assets (per [ADR-0002](../Architecture_Decisions/ADR_0002_Data_Driven_First.md)). Жоден balance number не hardcoded у C++. Designer змінює — engine reads — playtest проходить — рішення приймається.

## 1. Game Session Tuning — Global Parameter Inventory

Параметри сесії, винесені в один глобальний реєстр DataAssets. Команда крутить їх **без** перекомпіляції коду.

### Core Session DataAssets

| DataAsset | Path | Owner | Hot-reload |
| --- | --- | --- | --- |
| `DA_GP_Session_Default` | `/Game/GrimProtocol/DataAssets/Session/` | Design lead | Yes |
| `DA_GP_Resource_Ferronite` | `/Game/GrimProtocol/DataAssets/Resources/` | Design lead | Yes |
| `DA_GP_Building_FerroniteDeposit` | `/Game/GrimProtocol/DataAssets/Buildings/` | Design lead | Yes |
| `DA_GP_Building_MainBase` | `/Game/GrimProtocol/DataAssets/Buildings/` | Design lead | Yes |
| `DA_GP_Building_LogisticsHub` | `/Game/GrimProtocol/DataAssets/Buildings/` | Design lead | Yes |
| `DA_GP_Building_DefensiveTurret` | `/Game/GrimProtocol/DataAssets/Buildings/` | Design lead | Yes |
| `DA_GP_Building_Wall` | `/Game/GrimProtocol/DataAssets/Buildings/` | Design lead | Yes |
| `DA_GP_Unit_Worker` | `/Game/GrimProtocol/DataAssets/Units/` | Design lead | Yes |
| `DA_GP_Unit_SalvageWalker` | `/Game/GrimProtocol/DataAssets/Units/` | Design lead | Yes |
| `DA_GP_OrbitalDrop_*` | `/Game/GrimProtocol/DataAssets/OrbitalDrops/` | Design lead | Yes |
| `DA_GP_AI_Behavior_Default` | `/Game/GrimProtocol/DataAssets/AI/` | Design lead | Yes |
| `DA_GP_CameraConfig_Default` | `/Game/GrimProtocol/DataAssets/Camera/` | Design lead | Yes |

### `DA_GP_Session_Default` — Master Session Config

**Single DA, що тримає global session parameters.** Read by `AGP_GameMode` at match init.

```cpp
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_SessionConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    // === Match length ===
    UPROPERTY(EditAnywhere, Category = "GP|Match")
    float MatchDurationSeconds = 600.f;        // 10 min MVP

    UPROPERTY(EditAnywhere, Category = "GP|Match")
    float WarmupSeconds = 3.f;                  // pre-match countdown

    // === Victory ===
    UPROPERTY(EditAnywhere, Category = "GP|Victory")
    float DeliveryQuotaFerroniteScore = 5000.f; // primary win threshold (placeholder)

    UPROPERTY(EditAnywhere, Category = "GP|Victory")
    bool bAnnihilationCountsAsWin = true;       // MainBase destroyed → opponent wins

    UPROPERTY(EditAnywhere, Category = "GP|Victory")
    float TieBreakWindowSeconds = 60.f;         // tie-break sliding window

    // === SWARM escalation ===
    UPROPERTY(EditAnywhere, Category = "GP|SWARM")
    float WaveStartDelaySeconds = 60.f;         // first wave grace period

    UPROPERTY(EditAnywhere, Category = "GP|SWARM")
    float MinWaveSpacingSeconds = 30.f;         // floor between waves

    UPROPERTY(EditAnywhere, Category = "GP|SWARM")
    TSoftObjectPtr<UCurveFloat> ThreatToWaveSize;          // keyed on FerroniteThreatValue

    UPROPERTY(EditAnywhere, Category = "GP|SWARM")
    TSoftObjectPtr<UCurveFloat> ThreatToWaveFrequency;     // keyed on FerroniteThreatValue

    // === Orbital cycle ===
    UPROPERTY(EditAnywhere, Category = "GP|Orbital")
    float ContainerLaunchDelaySeconds = 2.5f;   // vulnerability window
    UPROPERTY(EditAnywhere, Category = "GP|Orbital")
    float ContainerLaunchCooldown = 1.0f;       // slot reusable after departure
    UPROPERTY(EditAnywhere, Category = "GP|Orbital")
    int32 MaxActivePodsPerTeam = 3;
    UPROPERTY(EditAnywhere, Category = "GP|Orbital")
    float PodDescentDurationDefault = 2.5f;

    // === Reward pacing ===
    UPROPERTY(EditAnywhere, Category = "GP|Rewards")
    float ScoreFlashDurationSeconds = 1.2f;
    UPROPERTY(EditAnywhere, Category = "GP|Rewards")
    float ToastQueueWindowSeconds = 3.0f;

    // === Player starting state ===
    UPROPERTY(EditAnywhere, Category = "GP|Startup")
    int32 StartingOrbitalFerronite = 0;
    UPROPERTY(EditAnywhere, Category = "GP|Startup")
    int32 StartingWorkers = 2;
    UPROPERTY(EditAnywhere, Category = "GP|Startup")
    int32 StartingMaxUnits = 5;
};
```

### Per-Entity DataAssets — Tuning Surface

Тут лежать **per-thing** numbers. Кожен DataAsset — окрема відповідальність.

| Parameter | Lives у DataAsset | Field | Hot-reload |
| --- | --- | --- | --- |
| Worker mine rate | `DA_GP_Unit_Worker` | `MineRatePerSecond` | Yes |
| Worker move speed | `DA_GP_Unit_Worker` | `MoveSpeed` | Yes |
| Worker carry capacity | `DA_GP_Unit_Worker` | `CarryCapacity` | Yes |
| Salvage Walker DPS / range | `DA_GP_Unit_SalvageWalker` | `Damage`, `AttackRange`, `AttackSpeed` | Yes |
| Deposit reserves | `DA_GP_Building_FerroniteDeposit` | `MaxCapacity` | Yes (per-instance also possible) |
| Container size | `DA_GP_Building_MainBase` | `ContainerVolume` | Yes |
| Containers per base | `DA_GP_Building_MainBase` | `BaseMaxContainerCount` | Yes |
| Container size bonus per Hub | `DA_GP_Building_LogisticsHub` | `ContainerCountBonus` | Yes |
| Unit cap per Hub | `DA_GP_Building_LogisticsHub` | `UnitCapContribution` | Yes |
| Drop cost (any) | `DA_GP_OrbitalDrop_*` | `Cost` | Yes |
| Drop descent | `DA_GP_OrbitalDrop_*` | `DescentDuration` | Yes |
| Drop footprint | `DA_GP_OrbitalDrop_*` + Building DA | `FootprintCells` | Yes |
| Threat per stored unit | `DA_GP_Resource_Ferronite` | `ThreatPerStoredUnit` | Yes |
| Score conversion rate | `DA_GP_Resource_Ferronite` | `ScoreConversionRate` | Yes |
| AI decision tick | `DA_GP_AI_Behavior_Default` | `DecisionInterval` | Yes |
| AI thresholds | `DA_GP_AI_Behavior_Default` | `TargetWorkerCount`, `DefenseThreshold`, ... | Yes |
| FoW cell size | `DA_GP_BuildGrid_Config` (з [`../TDD/06`](../TDD/06_Building_Architecture.md)) | `CellSize` | No (one per project) |
| Camera pan/zoom/rotate speed | `DA_GP_Camera_Default` | per-field | Yes |

### Map-Specific Overrides

Per-map can override **selected** values via `AGP_MapSettings : AInfo` actor, placed once per map:

- `OverrideMatchDuration : float` (optional)
- `OverrideQuota : float` (optional)
- `MapBoundsBox : FBox`
- `InitialFoWReveal : float`
- `DepositCapacityOverrides : TMap<AGP_FerroniteDeposit*, int32>` (per-deposit reserves)

Per-map overrides applied AFTER `DA_GP_Session_Default` load.

## 2. Debug Cheats / Tuning Cheats

**Purpose:** дизайнерам, балансерам, технічним тестерам — швидко крутити state у PIE / standalone. **Не** shipping feature.

### Compile Gate

Усі cheats закриваються `#if !UE_BUILD_SHIPPING` у C++. CI lint перевіряє відсутність cheat invocations у Shipping config.

### Console Command Schema

Префікс: `gp.cheat.<category>.<action> <args>`. Implemented у `UGP_CheatManager : UCheatManager` (UE engine cheat extension), registered per `AGP_PlayerController` у non-shipping.

### Cheat Reference

#### Resource Cheats (server-only)

| Command | Effect |
| --- | --- |
| `gp.cheat.give.orbital <N>` | `OrbitalFerronite += N` for caller. |
| `gp.cheat.take.orbital <N>` | `OrbitalFerronite -= N` clamped to 0. |
| `gp.cheat.give.planetary <N>` | Fill containers from current "imaginary" deposit. Adds N to current Container fill state. |
| `gp.cheat.container.fill <SlotIdx>` | Force container at slot index to 100% Ready state. |
| `gp.cheat.container.launch <SlotIdx>` | Trigger immediate launch on Ready container. Bypass `LaunchCooldown`. |
| `gp.cheat.container.set_capacity <N>` | Override MaxContainerCount runtime для caller's MainBase. |

#### FoW Cheats (local + server)

| Command | Effect |
| --- | --- |
| `gp.cheat.fow.reveal.all` | Set entire grid Visible for caller's team (single-frame snapshot, не permanent). |
| `gp.cheat.fow.reveal.permanent` | Set entire grid Explored permanently. |
| `gp.cheat.fow.reveal.radius <X> <Y> <R>` | Visible circle at world (X,Y) radius R for caller team. |
| `gp.cheat.fow.hide.all` | Reset Explored bitmap to all-Unexplored (next sight tick re-reveals local). |

#### Spawn Cheats (server-only)

| Command | Effect |
| --- | --- |
| `gp.cheat.spawn.unit <DropDefName> <X> <Y>` | Bypass orbital — spawn payload immediately at world (X,Y) for caller team. |
| `gp.cheat.spawn.building <DropDefName> <X> <Y>` | Same as above for buildings. |
| `gp.cheat.spawn.swarm <Count> <X> <Y>` | Spawn N SWARM units at (X,Y). |
| `gp.cheat.spawn.wave <Intensity>` | Trigger immediate SWARM wave proportional to Intensity (1-10). |
| `gp.cheat.kill.target` | Apply lethal damage to actor under cursor. |

#### Time / Difficulty (server-only)

| Command | Effect |
| --- | --- |
| `gp.cheat.time.speed <multiplier>` | `WorldSettings.TimeDilation = multiplier`. Range 0.1-5.0. |
| `gp.cheat.time.pause` | Pause match (singleplayer only). |
| `gp.cheat.time.add <seconds>` | Advance match timer by N seconds. |
| `gp.cheat.time.set <seconds>` | Set match time remaining. |

#### Win / Lose (server-only)

| Command | Effect |
| --- | --- |
| `gp.cheat.win` | Trigger immediate EndMatch для caller's team. |
| `gp.cheat.lose` | Same for opposing team (caller loses). |
| `gp.cheat.draw` | Force draw resolution. |

#### Live Balance Tuning (server-only)

| Command | Effect |
| --- | --- |
| `gp.cheat.tune.minerate <multiplier>` | Runtime multiplier applied to `MineRatePerSecond`. Affects all workers. |
| `gp.cheat.tune.unitcost <DropDefName> <newCost>` | Override Cost of specific drop type at runtime. |
| `gp.cheat.tune.wavefreq <multiplier>` | Multiplier on wave frequency. |
| `gp.cheat.tune.wavesize <multiplier>` | Multiplier on wave size. |
| `gp.cheat.tune.threat.per_stored <value>` | Override ThreatPerStoredUnit runtime. |
| `gp.cheat.tune.ai.tier <name>` | Hot-swap AI behavior DA (e.g., `easy`, `default`, `hard`). |
| `gp.cheat.tune.reset` | Reset all runtime tuning multipliers to DA defaults. |

#### AI Cheats

| Command | Effect |
| --- | --- |
| `gp.cheat.ai.disable` | Suspend AI decision tick. AI becomes idle. |
| `gp.cheat.ai.enable` | Resume AI tick. |
| `gp.cheat.ai.state <StateName>` | Force AI to specific state (Explore/Mine/Ship/Order/Defend). |
| `gp.cheat.ai.give.orbital <N>` | Add OrbitalFerronite to AI player. |

### Output

Кожен cheat:
- Logs `LogGPCheat` Warning з invoking PC і result.
- HUD shows non-intrusive toast "Cheat: <command> applied" (3 s).
- Server cheats validate caller has admin role (singleplayer host OR explicit `bAllowCheats=true` у PIE).

### Anti-Patterns

- ❌ Cheats у shipping build.
- ❌ Client cheats що write to gameplay state directly.
- ❌ Cheats що bypass Replication (clients seeing fake state).
- ❌ Cheats без logging.

## 3. Balance Calibration Article

Не теорія. Operational guide — як ми **знаємо**, що сесія працює.

### Normal Pacing — What Good Looks Like

Орієнтир для 10-хв MVP сесії. Per-minute milestones:

| Time | Player state | Sign of good pacing |
| --- | --- | --- |
| 0:00 - 1:00 | Discovery | Player знаходить deposit, відправляє Workers, бачить перший Container fill. |
| 1:00 - 2:00 | First ship | Перший Container launched. Player отримує перший Orbital Ferronite. |
| 2:00 - 3:00 | First drop | Player order-ить перший Worker/Walker/Turret. Перший pod descent. |
| 3:00 - 5:00 | Economy ramp | 2-3 containers shipped. SWARM перші waves з'являються. Player building defense or expanding. |
| 5:00 - 7:00 | Mid-match tension | Player feels economy AND threat tension. Defending while shipping. |
| 7:00 - 9:00 | Late push | Player optimizing — куди останні Orbital Ferronite. Може engage opponent. |
| 9:00 - 10:00 | Climax | Last containers shipping, peak SWARM, score race tight. |

Якщо плейтест показує "idle moments" >= 30 s OR "overwhelmed" > 60 s — pacing broken.

### Player Engagement Heuristics

| Question | Yes = good |
| --- | --- |
| Player робить decision чи action **кожні 5-15 s**? | yes |
| Player отримує **feedback** (positive чи negative) кожні 10-20 s? | yes |
| Player знає, що робити далі, **без UI читання**? | yes |
| Player колись думає "що тут відбувається"? | NO (Clarity violation) |
| Player колись чекає 30+ s без чогось важливого? | NO (idle violation) |
| Player колись paniks-clicks без розуміння? | NO (overwhelm violation) |

### Avoid: Idle Stretches

Якщо player has nothing to do for 30+ s → щось зламано:

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| "Workers all mining, нема чим зайнятись" | Mining too slow / cap too tight | Increase `MineRatePerSecond` 20% OR lower `Cost` для перших drops |
| "Жду drop pod descent" | Descent too slow | Decrease `PodDescentDurationDefault` 20% (was 2.5s → try 2.0s) |
| "Wave довго не приходить" | `WaveStartDelaySeconds` too high | Lower; OR scale `ThreatToWaveFrequency` curve steeper |
| "Орбітальний пул заповнюється швидше ніж я можу витратити" | Drops cost too low | Raise key drop costs OR raise pod cap to force batching |

### Avoid: Chaos / Overwhelm

Якщо player drowns у events / loses bearings → щось зламано:

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| "Wave too big занадто рано" | Threat curve steep | Flatten `ThreatToWaveSize` curve у early (low-threat) region |
| "Decisions неможливо встигати" | Too many concurrent prompts | Lengthen `MinStateDuration` для AI/SWARM, throttle notifications |
| "UI забивається toast-ами" | Notification deduplication broken | Increase deduplication window у `UGP_NotificationConfig` |
| "Container management constantly required" | `BaseMaxContainerCount` too low | Raise default to 7-8 (placeholder) for less micro |

### Rhythm — Calm / Plan / Risk / Crisis / Reward

Гра має чергувати ці 5 станів. Жоден стан не триває > 90 s.

| State | What player does | When it should occur |
| --- | --- | --- |
| **Calm** | Mining loop, base building | 0-30 s after wave defense, after big drop |
| **Plan** | Decide next drop, reposition | Every 30-60 s |
| **Risk** | Aggressive ship while wave incoming | Container nears full в late game |
| **Crisis** | Defend під tier-N wave / opponent push | Every 90-180 s |
| **Reward** | Container launches, drop arrives, opponent score visible behind | After plan + risk completion |

If a state extends beyond 90 s without rotation → broken pacing.

### Cross-System Interaction Map

Як ресурс / загрози / юніти / база / карта **разом**:

```
Mine more → containers fill → raw Ferronite stored at base → ship → orbital ferronite → orders → drops → defense
                                          |                     |
                                          ↓                     ↓
                          FerroniteThreatValue RISES    FerroniteThreatValue DROPS
                          (hoarding = more SWARM)       (shipping = swarm relief)
                                          |
                                          ↓
                          SWARM intensity scales with raw Ferronite stored RIGHT NOW
                                          |
                                          ↓
                          Defense investment competes with economy spend
                                          |
                                          ↓
                          Hoard too long = overwhelming SWARM = base loss = lose
                          Ship too eagerly = less stockpile risk but score still banks (greed-vs-safety)
```

Знаходити **рівновагу** між hoard (greed) і ship (safety) — це core MVP gameplay.

> **Open Question (design TBD):** since shipping LOWERS `FerroniteThreatValue`, a fast-shipper match may never escalate to a climax. A slow secondary global escalation floor (mild time/score baseline under the threat curve) may be needed so every match still escalates and ends. Starting value + test plan in balance pass — no firm numbers yet.

## 4. Core Gameplay Loop — Canonical (Post-Pivot)

Замінює застаріле `02_Core_Gameplay_Loop.md` (там pivot notice). Це canonical loop.

```
1.  Initial landing       — match start, MainBase + 2 Workers pre-deployed.
2.  Scout                 — Workers + initial sight bubble. Find deposits.
3.  Mine                  — Workers extract from deposits.
4.  Transport             — Workers carry to MainBase.
5.  Container fill        — Drop-off accumulates у containers; raw stock at base raises FerroniteThreatValue (more SWARM).
6.  Launch                — Full container ships to orbit (2-3 s telegraph window); FerroniteThreatValue drops (SWARM relief).
7.  Orbital reward        — OrbitalFerronite + FerroniteScore increment.
8.  SWARM pressure        — Wave size/frequency scale with FerroniteThreatValue (raw Ferronite stored at base RIGHT NOW; up on drop-off, down on launch).
9.  Order                 — Spend OrbitalFerronite на Worker / Walker / Turret / Wall / Hub drops.
10. Drop                  — Pod descent (telegraph). Asset lands.
11. Expand                — More territory revealed, more deposits available.
12. Defend                — React to SWARM waves + opponent harass.
13. Decision pressure     — Greed vs Safety; Push vs Turtle; Economy vs Combat.
14. Repeat 3-13            — Until match end.
15. Match end             — Delivery quota reached OR timer expiry → score / annihilation resolution.
```

Кожен step — **observable і actionable** для player. Не hidden process.

## 5. Reward Loop — 4-Tier System

Reward — не просто UI number. Це **feedback signal**, що підтримує темп.

### Short Rewards (every 5-15 s)

| Trigger | Reward | Channel |
| --- | --- | --- |
| Mining tick | Cargo bar fills visually на Worker | V-Mat + V-UI |
| Worker arrives base | Drop-off animation, container fill increments | V-VFX + V-UI + A-3D |
| New cell explored | Map area lit up | V-UI minimap reveal + soft "scan" SFX |
| Survives sutich (combat hit) | Damage flash on attacker (visible "I hurt them") | V-VFX + V-Mat |
| Enemy unit killed | Death VFX + corpse mesh | V-VFX + V-Mesh + A-3D |

### Medium Rewards (every 30-60 s)

| Trigger | Reward | Channel |
| --- | --- | --- |
| Container Ready | Container icon glow + "armed" SFX | V-UI + A-2D |
| Drop pod arrives | Big VFX + impact shake + asset reveal | V-VFX + Camera + A-3D + V-UI |
| New unit ready | Auto-highlight 1 s, "ready for orders" cue | V-Mat + A-2D |
| New building deployed | Cap +5 flash, container slot opens, structural rumble | V-UI flash + A-3D |
| New territory secured | Minimap zone tint + Player sees expansion | V-UI |

### Large Rewards (every 60-180 s)

| Trigger | Reward | Channel |
| --- | --- | --- |
| Container launched to orbit | Rocket lift-off VFX + OrbitalFerronite +N flash + Score +N flash + audio rumble | V-VFX + V-UI flash × 2 + A-3D |
| Major wave defended | "Wave repelled" toast + threat bar relief | HUD-Toast + V-UI |
| First Salvage Walker deployed | Spotlight on unit + voice line (post-MVP) | V-Mat + A-2D |
| Opponent score milestone | Their score visible passes yours / yours passes theirs — flash | V-UI flash |

### Strategic Rewards (campaign-level, post-MVP)

Reserved для post-MVP. У MVP — single-match focus.

| Trigger | Reward | Notes |
| --- | --- | --- |
| Match won | Final score breakdown + winner cinematic stub | EndOfMatch overlay |
| Score record beaten | Persistent score record (local profile, post-MVP) | Out of MVP — placeholder |
| Doctrine progress | Asymmetric corporate doctrine unlock | Out of MVP (per GP-0801) |
| Fleet command level | Cross-session expedition advancement | Out of MVP |

### Reward Pacing Rule

**Кожен reward feedback channel має ≥ 2 signals** (visual + audio мінімум). Per [`../TDD/12_UI_Architecture.md`](../TDD/12_UI_Architecture.md) §Detailed Feedback Matrix.

## 6. Player Flow — Session States

Гравець проходить ці 10 станів за матч. Кожен має **trigger**, **player feeling**, **dev guarantee**.

| # | State | Trigger | Player feels | Dev must guarantee |
| --- | --- | --- | --- | --- |
| 1 | **Orientation** | 0:00 landing | "Що тут? Звідки почати?" | UI прозоро показує MainBase + Workers. Tooltip-free. |
| 2 | **First simple decision** | 0:15 | "Я бачу deposit. Send Worker?" | LMB на Worker + RMB на deposit працює immediately. Smart-command resolves. |
| 3 | **First reward** | 0:30 | "О, container fills!" | Container fill visibly animates. Сo per-tick UX. |
| 4 | **First risk** | 1:00 | "Container майже повний. Wave скоро?" | Threat bar (FerroniteThreatValue) visible. SWARM telegraph не surprise. |
| 5 | **First threat** | 1:30 - 2:00 | "Перші SWARM grunts. Defense?" | Wave size manageable. Salvage Walker оrderable якщо economy ok. |
| 6 | **First expansion** | 2:30 - 3:30 | "Order перший Salvage Walker. Drop pod arrives." | Drop telegraph satisfying. Tier-1 unit feels powerful enough. |
| 7 | **First crisis** | 4:00 - 5:00 | "MainBase під tiск!" | Wave bigger but defendable з committed defense. Walls / Turrets recommended. |
| 8 | **First strong success** | 5:00 - 7:00 | "I'm shipping consistently. Score climbing." | Visible score lead OR catch-up against opponent. |
| 9 | **Final tension** | 8:30 - 9:30 | "Last 90 s. Все або нічого." | Threat peak. Timer red. Containers all-in. |
| 10 | **Session completion** | 10:00 | "I won / I lost. Чому?" | Clear EndOfMatch screen з причиною (quota / annihilation / timer-tie). |

**Anti-pattern:** player stays у State 2 (early decision) > 2:00 (engagement stall). Mining too slow OR Container fill too far.

## 7. Balance Parameters Table

Master snapshot **тільки** placeholders (TBD у balance pass). Designers свапнуть тут після кожного playtest.

### Match-Level

| Parameter | Default | Range | Owner |
| --- | --- | --- | --- |
| Match duration | 600 s | 480-720 | Design lead |
| Delivery quota | 5000 score | 3000-8000 | Design lead |
| Pod cap per team | 3 | 2-5 | Design lead |
| Warmup | 3 s | 1-5 | Tech lead |

### Worker

| Parameter | Default | Range | Owner |
| --- | --- | --- | --- |
| Move speed | 350 cm/s | 250-450 | Design |
| Mine rate | 10 /s | 5-20 | Design |
| Carry capacity | 50 | 25-100 | Design |
| Health | 50 | 30-80 | Design |
| Cost (orbital drop) | TBD | TBD | Design |

### Ferronite Deposit

| Parameter | Default | Range | Owner |
| --- | --- | --- | --- |
| Max capacity | 3000 | 1500-6000 | Design |
| Concurrent miners | 4 | 3-6 | Design |
| Depleted behavior | Destroy | — | Tech lead (architectural) |

### Container

| Parameter | Default | Range | Owner |
| --- | --- | --- | --- |
| Volume per container | 100 | 50-200 | Design |
| Base container count | 5 | 3-8 | Design |
| Per-Hub bonus | +2 | +1..+5 | Design |
| Launch delay | 2.5 s | 1.5-4.0 | Design |
| Threat per stored unit | 1.0 | 0.5-2.0 | Design |
| Score per shipped unit | 1.0 | 0.5-2.0 | Design |
| Orbital per shipped unit | 1.0 | 0.5-2.0 | Design |

### SWARM

| Parameter | Default | Range | Owner |
| --- | --- | --- | --- |
| First wave delay | 60 s | 30-120 | Design |
| Wave threat scaling (ThreatToWaveSize / ThreatToWaveFrequency) | curve | DA editable | Design |
| Min wave spacing | 30 s | 15-60 | Design |

### Drop Costs (placeholder)

| Drop type | Cost | Range | Owner |
| --- | --- | --- | --- |
| Worker | 80 | 50-150 | Design |
| Salvage Walker | 200 | 150-400 | Design |
| Defensive Turret | 150 | 100-300 | Design |
| Wall segment | 30 | 15-60 | Design |
| Wall Turret | 100 | 60-200 | Design |
| Logistics Hub | 400 | 300-700 | Design |

### Build Grid

| Parameter | Default | Range | Owner |
| --- | --- | --- | --- |
| Cell size | 200 cm | (fixed) | Tech lead |
| Wall clearance | 2 cells | (fixed) | Design (mechanic), Tech (impl) |
| A* iteration cap | 200 | 100-500 | Tech lead |

### AI Behavior

| Parameter | Default | Range | Owner |
| --- | --- | --- | --- |
| Decision interval | 3 s | 1-10 | Design |
| Target worker count | 6 | 4-12 | Design |
| Defense threshold | 50 threat (FerroniteThreatValue) | 30-100 | Design |

## 8. Playtest Metrics — What We Measure

Automatic capture during each playtest session. Server logs to JSON під `Saved/Playtests/` (PIE) або central collector (closed beta).

### Per-Match Metrics

| Metric | Goal | Red flag |
| --- | --- | --- |
| Time to first mine | < 15 s | > 30 s = orientation broken |
| Time to first ship | < 90 s | > 180 s = pacing broken |
| Time to first drop | < 150 s | > 240 s = economy too slow |
| Mining time / total | 30-50% | < 20% (too much micro) OR > 70% (boring) |
| Idle time per player | < 60 s total | > 120 s = pacing broken |
| Drops per match | 8-15 | < 5 (boring) або > 25 (overwhelming) |
| Containers shipped | 10-20 | < 6 = economy broken |
| Final score gap | < 30% | > 50% = mismatch (rebalance) |
| Match end reason | quota / timer | annihilation rate > 20% = base too vulnerable |

### Per-Player Behavior Markers

- Did player explore beyond starting zone? (yes/no)
- Did player order > 2 drop types? (yes/no — variety = engagement)
- Did player use walls? (yes / no / by force)
- Did player retry similar build order? (yes = strategy emerging)
- Did player visibly panic у last 30 s? (yes = good tension; no = anti-climax)

### Subjective Survey (post-match)

3-question micro-survey:
1. "What did you do well? (free text 1 line)"
2. "What confused you? (free text 1 line)"
3. "Did you have fun? (1-5 scale)"

## 9. Balance Adjustment Rules — Workflow

**Hot vs Cold changes.** Що змінюється швидко, що залишається стабільним.

### Hot — Change Freely (per-playtest)

Designer може крутити будь-коли:

- Cost numbers (`UGP_*Definition.Cost`).
- Stat numbers (`Health`, `Damage`, `MoveSpeed`, `Mine rate`, `AttackRange`, `AttackSpeed`).
- Timing numbers (`DescentDuration`, `LaunchDelay`, `WaveStartDelay`).
- Container parameters (`Volume`, `BaseMaxContainerCount`).
- Threat curves (`ThreatPerStoredUnit`, `ThreatToWaveSize` / `ThreatToWaveFrequency`).
- AI thresholds (`DecisionInterval`, `TargetWorkerCount`, ...).
- Map-level overrides (`AGP_MapSettings`).

Hot changes — same-day playtest iteration.

### Warm — Discuss Before Change (per-week)

Need short discussion з tech lead before swap:

- DataAsset schema additions (new field).
- New drop types (`DA_GP_OrbitalDrop_*`).
- Asset Manager primary asset categories.
- New tag entries у `FGPGameplayTags`.

### Cold — Architectural; Need ADR (per-sprint+)

Need ADR draft before change:

- Grid cell size.
- Module split.
- Replication topology.
- ASC ownership rules.
- New subsystem.
- Pillar changes.
- Win condition model.

### Adjustment Workflow

```
Playtest → Capture metrics → 3 questions to player.
↓
Designer identifies 1-3 numbers to tune.
↓
Hot change у DataAsset → save → next playtest.
↓
If 2-3 hot changes don't fix → escalate to Warm category.
↓
If still broken → architectural review (Cold).
```

**Anti-pattern:** Changing > 5 numbers between playtests. Make small focused changes, observe.

## 10. MVP Scope Guards

**Що залишається стабільним до first playable:**

- Core gameplay loop (per §4).
- One resource.
- One container type.
- One ship-to-orbit pipeline.
- Orbital delivery model (per ADR-0009).
- 10-min match.
- 3-level FoW.
- Steam 2-player.
- Singleplayer AI opponent.
- Build grid + walls.

**Що можна крутити швидко:**

- Балансові цифри.
- Visual / VFX / SFX assets.
- Map layout.
- Drop catalog content.
- AI behavior thresholds.

**Що ще не у MVP (Out of Scope):**

- Multiple resources.
- Multiple factions.
- Doctrine tree.
- Black market / smugglers.
- Repair/upgrade modules.
- Replay system.
- Reconnect.
- Dedicated server.
- Stealth / scanner reveal.

Перелік стабільний; додавання — окрема ADR.

## 11. Ownership Matrix

Хто крутить що.

| Параметр | Designer | Tech Lead | Engineer | Notes |
| --- | --- | --- | --- | --- |
| Balance numbers у DataAssets | ✓ | review | — | Designer-owned hot changes |
| New DataAsset class schema | discuss | ✓ | implement | Tech-lead review |
| Tag taxonomy additions | discuss | ✓ | implement | Tech-lead approves; engineer adds |
| New gameplay component | discuss | review | ✓ | Engineer-owned, follows architecture |
| New ADR | propose | ✓ | implement after approval | Tech lead final call |
| Cheat additions | request | review | ✓ | Engineer implements, non-shipping |
| Map content | ✓ | — | — | Pure designer / level art |
| AI tuning curve | ✓ | review | — | Designer + DataAsset |
| Module / subsystem | — | ✓ | implement | Tech lead architectural decision |

## 12. Pillar 8 Re-Check (this document)

- 1-2 sentence summary: "Single global session config DA, hot-tunable DataAssets per entity, console cheats for fast iteration, observable metrics and reward loop."
- Fun у v1: yes — gameplay loop and reward tiers are explicit and observable.
- New decision type: pacing rhythm tuning per playtest data.
- Cheap: standard DataAsset workflow, no new subsystem (cheat manager is engine-extensible).
- Scales via content: add more drop types / units / behaviors via DataAsset only.

Passes.

## References

- [ADR-0002 Data-Driven First](../Architecture_Decisions/ADR_0002_Data_Driven_First.md) — soft refs + Asset Manager loading.
- [ADR-0009 Orbital Delivery Pillar](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md) — core model.
- [01_Game_Pillars](01_Game_Pillars.md) — Pillar 8 (Simple Core) + meta-rules.
- [02_Core_Gameplay_Loop](02_Core_Gameplay_Loop.md) — pre-pivot detail (under Pivot Notice).
- [06_Resources](06_Resources.md) §Two-State Storage + Container System.
- [10_Orbital_Delivery](10_Orbital_Delivery.md) — drop fantasy.
- [11_Fog_of_War](11_Fog_of_War.md) — visibility.
- [`../TDD/07_Resource_Architecture`](../TDD/07_Resource_Architecture.md) §Container System Update + Feel section.
- [`../TDD/12_UI_Architecture`](../TDD/12_UI_Architecture.md) §Detailed Feedback Matrix.
- [`../TDD/14_Orbital_Delivery`](../TDD/14_Orbital_Delivery.md) §Feel section.
- [`../TDD/15_Fog_of_War`](../TDD/15_Fog_of_War.md).
- [`../Development/Slice_Template`](../Development/Slice_Template.md) — implementation slice format.

## Open Questions

1. Should playtest metrics flow to central collector у closed beta? Recommend yes; defer infrastructure до GP-0807.
2. Survey delivery — in-game prompt OR external form? Recommend in-game one-shot toast з link to form.
3. Cheat manager — single class per CheatManager spec OR plugin? Recommend single class у GPRuntime (UE extension pattern).
4. Should `DA_GP_Session_Default` allow runtime hot-reload (editor swap → match restart) OR snapshot-at-init only? Recommend snapshot-at-init для consistency; cheats handle runtime override.
5. Per-map curve override granularity — full curve replacement OR multiplier on default curve? Recommend multiplier для simplicity.

Реcommend create follow-up task GP-0807 "Playtest Metrics Collection" якщо ми бачимо потребу у central collection.
