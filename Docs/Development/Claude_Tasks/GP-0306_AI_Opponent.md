# GP-0306 AI Opponent

## Goal

Описати singleplayer AI opponent — primitive state machine, що грає симетрично з людиною (через orbital model). MVP-required per memory rule `project_ai_opponent_in_mvp` і [ADR-0008](../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md).

## Inputs

- [`../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md`](../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md) — clas choice (AAIController).
- [`../../GDD/03_Factions.md`](../../GDD/03_Factions.md) — initial AI scope.
- [`../../GDD/First_Playable_Match.md`](../../GDD/First_Playable_Match.md) — singleplayer flow з AI.
- [`../../GDD/10_Orbital_Delivery.md`](../../GDD/10_Orbital_Delivery.md) — orbital model.
- [`../../GDD/06_Resources.md`](../../GDD/06_Resources.md) — Container System.
- [`../../TDD/03_Multiplayer_Architecture.md`](../../TDD/03_Multiplayer_Architecture.md) — AI authority notes.
- Memory rule `project_ai_opponent_in_mvp`.

## Code Allowed

No.

## Scope

AI opponent identity, state machine taxonomy (states + transitions), decision tick, action APIs (symmetric з human gameplay), authority model, balance hooks via `UGP_AIBehaviorDefinition` DataAsset, симетричні FoW / spending / SWARM exposure (no cheating). Не реалізовувати код.

## Required Skill Pass

- `gp-mechanics-validator`
- `game-design-framework`

## Player Goal

Гравцеві у singleplayer потрібен **симетричний competitor**, що:
- Виконує ті ж дії (видобуток / shipping / orbital orders / defense).
- Тисне реальним темпом, не nominal score.
- Не "читерить" — підпадає під ті ж FoW / SWARM / cost rules.
- Робить розрізнювані рішення (combat push / economy boom / turtle), не лінійний script.
- Програшний за дизайном для new player, але не trivial.

## Design Requirements

### State Machine

5 states, всі server-side, low-frequency decision tick (placeholder 3 s).

```
EGP_AIState:
  Explore   — рухає Workers / Salvage Walker до unexplored зон. Active early-game.
  Mine      — Workers до closest deposit, утримує mining loop. Steady-state economy.
  Ship      — пріоритезує container fill + launch (e.g., chains Workers до nearest unfilled container).
  Order     — spends OrbitalFerronite на drop (Worker / Walker / Turret / LogisticsHub) per behavior thresholds.
  Defend    — react to SWARM aggression high OR enemy attack near base; redirects combat units, orders Defensive Turrets / Walls.
```

### Decision Tick

```
Кожен DecisionInterval (DA-driven, placeholder 3 s, server-only):
  1. Evaluate metrics:
     - CurrentWorkers, MaxWorkers (per BehaviorDef.TargetWorkerCount)
     - OrbitalFerronite, ContainersReady, ContainersFilling
     - FerroniteThreatValue (read from GameState)
     - EnemyVisibleNearBase (FoW visibility check)
     - OwnHealthSummary (MainBase + key buildings)
  2. EvaluateNextState() — priority ladder:
     a. If MainBase health < 30% OR FerroniteThreatValue >= DefenseThreshold OR enemy visible within base radius
        → Defend
     b. Else if ContainersReady > 0 AND OrbitalFerronite < AffordableActionCost
        → Ship (passive — actually no work needed, containers auto-ship; AI does nothing special, but state == Ship blocks Order)
     c. Else if OrbitalFerronite >= AffordableActionCost AND (CurrentWorkers < TargetWorkerCount OR CombatUnits < TargetCombatRoster OR DefensiveTurrets < TargetTurretCount)
        → Order
     d. Else if Workers idle AND deposit available
        → Mine
     e. Else
        → Explore
  3. ExecuteStateActions(NewState) — invoke same gameplay APIs as human (per ADR-0008):
     - Mine: per-Worker AssignToDeposit (or rely on auto-cycle)
     - Order: pick highest-priority drop type, pick safe Visible cell, issue order via UGP_OrbitalDeliverySubsystem
     - Defend: command Salvage Walker to MainBase region OR order Defensive Turret near choke
     - Explore: command Workers / scouts to nearest unexplored cells
     - Ship: passive (let auto-ship occur), но reduce parallel Order spend
```

### Symmetric Rules — No Cheating

- AI has standard `AGP_PlayerState` з ASC + `UGP_PlayerAttributeSet` (per ADR-0008).
- AI має own FoW grid (`UGP_FogOfWarComponent::VisibleByTeam[AI.TeamId]`). Не bypassing.
- AI spends Real `OrbitalFerronite` via `GE_GP_SpendOrbital`.
- AI subject to FerroniteThreatValue (raw stock at its own base): rises on drop-off, falls on launch, drives SWARM waves against AI symmetrically.
- AI cannot see hidden enemies (`IsNetRelevantFor` filters apply).

### Drop Targeting Heuristics

AI обирає drop point:

1. **Worker drops:** near MainBase (within 1500 cm).
2. **Combat unit drops:** at high FerroniteThreatValue → near base perimeter. At low FerroniteThreatValue → forward у visible expansion zone.
3. **Defensive Turret:** near MainBase choke points (DA-flagged "preferred turret sites" per map OR computed: edges of visible FoW where SWARM has historically attacked).
4. **Logistics Hub:** near MainBase if containers max-out frequently AND room available.

Heuristic logic у `AGP_AIController::FindDropPoint(DropType)`. DA values tune the bias (e.g., `PreferredDropRadiusFromBase : float`).

### Wall Logic — Optional у MVP

AI може будувати walls на choke points (per BehaviorDef.bUsesWalls flag, default false для MVP щоб не ускладнювати). Якщо true — AI вибирає 2 cells на narrow corridor біля MainBase, drag-build wall. Defer до AI tuning pass post-MVP.

### Reactive Behavior

State transitions react до GameState changes:
- FerroniteThreatValue cross threshold (Low → High) → consider Defend.
- MainBase takes damage → immediate Defend (interrupt current state).
- Player visible attacker → immediate Defend (rally Salvage Walkers).

### Difficulty Curve (MVP — single tier)

MVP — single AI behavior tier. Post-MVP: easy / medium / hard via different `DA_GP_AI_Behavior_*` instances з tuned thresholds.

## Deliverables

- AI state taxonomy + transition matrix (5 states + priority ladder).
- `UGP_AIBehaviorDefinition` DA schema (decision interval, target counts, thresholds, drop biases, preferred drops list — all soft refs per ADR-0002).
- Drop targeting heuristic per drop type.
- Symmetric authority confirmation (no cheating mechanics).
- Reactive behavior triggers (SWARM threshold, base damage).
- 1-tier MVP difficulty; difficulty curve у post-MVP backlog.

## Validation

- AI uses same RPCs / APIs as human (no AI-only authority hooks).
- AI bound by FoW (cannot target hidden enemies / drop into Unexplored).
- AI bound by `OrbitalFerronite` cost rules.
- AI subject до `FerroniteThreatValue` (raw stock at base; up on drop-off, down on launch) driving its own SWARM waves.
- AI 5-state machine описана з explicit transitions.
- All thresholds / values DA-driven (placeholders only).
- Pillar 8 5-question gate: passes (1-2 sentence explanation, fun у v1 via competitive pressure, new decision type — AI behavior recognition, cheap implementation, scales via more behavior DAs post-MVP).

## Stop Condition

Зупинитися після AI design spec.

## Feel / MVP Fun Maximization (Validator Pass)

AI повинен **читатися як competitor**, не як "rule-based bot". Per 5-component rubric:

**AI activity visible to human (Clarity + Motivation):**
- Whenever AI drops pod у player's Visible FoW → telegraph cosmetic per TDD/14 fires (player sees opponent's pod descending).
- AI's containers ship → opponent score visibly increments + "+N" flash on `OpponentScore` HUD readout.
- AI's combat units enter player Visible: standard FoW reveal — player sees "they're coming".
- Player **never wonders** "what is AI doing" — they see it через standard FoW/score signals.

**AI behavior signal — clarity through action variety (Motivation):**
- State transition logged at Verbose only (debug); player must INFER from observable behavior.
- AI uses **same VFX/SFX** as human (no AI-tagged "this is AI" particles). Pod = pod. Worker = Worker. Player reads behavior, not entity-type.

**AI reactive triggers feel earned (Satisfaction):**
- Damage MainBase → AI defends within 1-2 decision ticks (3-6 s). Player **sees** Salvage Walkers rally to base. Cause-effect readable.
- SWARM wave near AI base → AI orders Defensive Turret. Player can scout to see it.

**Asymmetric difficulty curve (Motivation + MVP scope):**
- MVP — single tier (`DA_GP_AI_Behavior_Default`).
- AI should lose to focused-play but win against aimless / greedy / overcommit player.
- Pillar 8 check: 1-sentence "AI mines, ships, drops, defends — same rules as you, different priorities." Fun у v1 — yes (competitive pressure без long tutorial). New decision — economic vs combat reading. Cheap — single state machine. Scales — більше DA tiers post-MVP.

**Anti-cheat reaffirmation:**
- AI has own FoW grid. Cannot see hidden enemy economy.
- AI spending validates against own ASC. No "free units".
- AI subject to SWARM scaling driven by its own FerroniteThreatValue (raw stock at base).

## Output

- Design spec lives у:
  - Memory rule `project_ai_opponent_in_mvp` (canonical scope).
  - [`../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md`](../../Architecture_Decisions/ADR_0008_AI_Opponent_AAIController.md) (class choice).
  - This task file (gameplay design detail).
  - Future TDD section / dedicated file: `TDD/16_AI_Architecture.md` (created у GP-0702 expand task — currently scope-only у backlog).
- Decisions:
  - Class: `AGP_AIController : AAIController` (per ADR-0008).
  - 5 states: `Explore / Mine / Ship / Order / Defend` із priority ladder.
  - Decision interval: DA placeholder 3 s.
  - DataAsset: `UGP_AIBehaviorDefinition` (thresholds, target counts, drop biases — all soft refs).
  - Drop heuristics per drop type (Worker / Combat / Turret / LogisticsHub).
  - Reactive triggers: SWARM threshold cross, MainBase damage.
  - Wall usage flag OFF за default у MVP (defer tuning).
- Implementation deferred to follow-up slices **GP-S54 / GP-S55 / GP-S56** (materialized у Slice 10 of [`../../TDD/13_Architecture_Proposal.md`](../../TDD/13_Architecture_Proposal.md)).
