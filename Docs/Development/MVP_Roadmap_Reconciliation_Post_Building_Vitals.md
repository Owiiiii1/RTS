# MVP Roadmap Reconciliation — Post Building Vitals

**Status:** `PRODUCTION_HUD_LAYOUT_DOCUMENTATION_READY_FOR_REVIEW`
**Authority:** current-state MVP roadmap; supersedes historical S-number order as an execution cursor
**Baseline:** `origin/main` @ `317ce3f0367111081e3a8987c8ac8beebfbd6310`
**Audit date:** 2026-08-21
**Scope:** current factual roadmap and capability status.
**Current execution checkpoint:** Production HUD Resource/Match data foundation is on `main`.
Approved two-bar HUD IA is documented. No authored visual HUD, minimap function, or Order Menu
is included.

## 1. Why reconciliation is required

The implementation no longer follows the titles attached to the original S-number sequence in
[`TDD/13_Architecture_Proposal.md`](../TDD/13_Architecture_Proposal.md). Reconciliation slices reused or
extended IDs while delivering the underlying capability through narrower, project-specific shapes.
Consequently, an absent historical class is not evidence that its gameplay capability is absent.

Examples:

- Combat fire/LOS lives in `UGP_UnitCommandComponent` and `GPCombatLOS`, not a required
  `UGP_CombatComponent`.
- Target acquisition, timed retaliation, and Attack-Move are implemented by the unit-command pipeline.
- Orbital building deployment uses `AGP_BuildingPlacementGhost`, `AGP_PlayerController`,
  `GPBuildingDropAuthority`, `UGP_BuildGridSubsystem`, READY inventory, and `AGP_DropPod`; it does not
  require the old `AGP_DropReticle` class name.
- The original per-wall DropPod concept was superseded by Wall Package delivery to MainBase inventory.

This document classifies capabilities from current production code, then maps historical IDs onto that
factual state. Historical task files remain useful evidence and design history, but they are not the
current execution order.

## 2. Status vocabulary

- **DONE** — the MVP capability is implemented in production code.
- **DONE — SUPERSEDED IMPLEMENTATION SHAPE** — the capability exists through a different architecture
  than the historical class/task title.
- **PARTIAL** — meaningful production support exists, but the stated MVP capability is incomplete.
- **NOT STARTED** — no production implementation was found; tags or design documents alone do not count.
- **DEFERRED** — intentionally outside the immediate production path.
- **DESIGN REQUIRED** — implementation is blocked on a dedicated design/reconciliation decision.
- **POST-MVP** — explicitly outside MVP.

## 3. Current factual capability matrix

### Player control and combat

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| RTS camera | **DONE** | `AGP_CameraPawn`, camera config, bounds, and Enhanced Input paths are present. |
| Click/marquee selection and control groups | **DONE** | `UGP_SelectionComponent` and `AGP_PlayerController` local selection/input flow. |
| Move | **DONE** | RMB smart command, server validation/execution, and `UGP_MovementComponent`; movement reconciliation contracts exist. |
| Stop | **PARTIAL** | Server command/FSM cleanup exists, but no player input or smart-command path emits `GP.Command.Stop`. |
| Explicit Attack | **DONE** | Server-authoritative Attack FSM, LOS, range, cooldown, damage, and target lifecycle in `UGP_UnitCommandComponent`. |
| Auto-acquire | **DONE — SUPERSEDED IMPLEMENTATION SHAPE** | Idle unit/turret scanning is in `UGP_UnitCommandComponent`; `gp.Combat.RunAutoAcquireContractTest`. No separate targeting component is required. |
| Timed retaliation | **DONE — SUPERSEDED IMPLEMENTATION SHAPE** | Retaliation pursuit/expiry is in the command pipeline; `gp.Combat.RunRetaliationPursuitContractTest`. |
| Attack-Move | **DONE** | Controller modal/input plus travel, acquire, engage, and resume behavior; `gp.Combat.RunAttackMoveContractTest`. |
| Damage, health, death | **DONE** | GAS attributes/damage calculation, authority death path, and replicated state are present. |
| Health bars and team presentation | **DONE** | `UGP_HealthBarComponent`, `UGP_TeamPresentationComponent`, and focused contracts. |
| Salvage Walker | **DONE** | Native combat unit identity, definition/catalog, commands, and orbital payload path are present. |

### Resource and economy

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| Ferronite Deposit | **DONE** | Resource-node actor, capacity/concurrency/depletion and reassignment behavior are implemented. |
| Worker mining/return/remine | **DONE** | `UGP_MiningComponent`, worker command chain, depletion and resilience contracts. |
| Worker cargo | **DONE** | `UGP_CargoComponent`, replicated cargo state, hauling and presentation support. |
| MainBase storage/containers | **DONE** | `UGP_StorageComponent` and team MainBase registry own accepted raw stock. |
| `FerroniteThreatValue` | **DONE** | Per-team replicated GameState value; accepted drop-off raises it and launch lowers it. |
| Container launch | **DONE** | Ready-container authority request and launch state transitions are implemented. |
| `OrbitalFerronite` | **DONE** | Launch conversion and orbital purchase spend use the player GAS attribute. |
| `FerroniteScore` | **DONE** | Cumulative launch score is replicated and does not decrease on spend. |
| Logistics Hub +5 unit cap | **DONE** | `AGP_LogisticsHub` applies/removes the definition-driven cap effect. |
| Logistics Hub storage-cap bonus | **NOT STARTED** | `AGP_LogisticsHub` explicitly marks the container-cap bonus deferred; no production bonus path was found. |

### Orbital procurement and placement

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| Unit catalog/manifest and unit DropPod | **DONE** | Data-driven unit catalog, manifest/cap checks, MainBase drop-zone placement, and payload spawn are implemented. |
| Building catalog and purchase-to-READY | **DONE** | Building drop definitions/catalog plus replicated READY inventory are implemented. |
| Building ghost/reticle capability | **DONE — SUPERSEDED IMPLEMENTATION SHAPE** | `AGP_BuildingPlacementGhost` renders payload/footprint validity; old `AGP_DropReticle` class is unnecessary. |
| Placement validation, snapping, reservation | **DONE** | `UGP_BuildGridSubsystem` and `GPBuildingDropAuthority` validate and reserve authority cells. |
| Placement confirm/cancel/input ownership | **DONE** | `AGP_PlayerController` owns local placement mode, LMB confirm, RMB/Esc cancel, and click-through suppression. |
| Building DropPod delivery/payload spawn | **DONE** | Confirmed READY deployment passes catalog-resolved payload/definition to `AGP_DropPod`. |
| Building payload ownership | **DONE** | Canonical chain is `UGP_OrbitalDropDefinition -> UGP_BuildingDefinition -> SpawnedClass`. |
| Delivery timing ownership | **DONE** | Unit/building/Wall Package paths use the established settings/catalog timing contract. |
| Wall Package purchase/delivery/inventory | **DONE** | Package catalog/authority, one DropPod to MainBase, pending state, and 0..5 segment inventory exist. |

### Buildings

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| MainBase | **DONE** | Storage, drop-off, container launch, unit drop zone, wall inventory, death registration, and canonical vitals are present. |
| Logistics Hub | **PARTIAL** | Orbital deployment and +5 cap are done; canonical design's storage-cap bonus is absent. |
| Defensive Turret | **DONE** | Free-standing orbital turret with authority auto-acquire/combat and definition-owned vitals. |
| Building vitals ownership | **DONE** | `BuildingDefinition.UnitDefinition` feeds the existing async unit-definition/GAS initialization once. |
| Wall actor | **NOT STARTED** | Wall tags/package inventory exist, but no `AGP_Wall` production actor was found. |
| Wall connection logic | **NOT STARTED** | No `UGP_WallConnectionComponent` production implementation. |
| Wall drag placement | **NOT STARTED** | No Build Wall surface placement, inventory-consume preview, or atomic wall spawn flow. |
| Wall-mounted Turret | **NOT STARTED** | No production `AGP_WallTurret` implementation. |
| Worker Repair | **NOT STARTED** | Gameplay tags/design exist; no repair ability or authority execution path was found. |
| Sell building | **NOT STARTED** | Tags/design exist; no sell fields/refund/RPC implementation was found. |
| Demolish wall | **NOT STARTED** | Tags/design exist; no demolish authority/cursor implementation was found. |

### UI and feedback

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| TEMP gameplay HUD | **DONE (temporary)** | `UGP_TEMP_S28P_PlanetaryFerroniteHUD` exposes resources, launch, catalogs/READY, cap, timer, result, and Wall Package state. |
| CommonUI/MVVM prerequisites | **DONE — FOUNDATION** | Plugins/dependencies plus project-owned activatable and non-activatable widget bases, HUD root base, FoW/Resource/Match VMs, and push adapters exist in `GPUIRuntime`. |
| Production HUD | **PARTIAL — DATA FOUNDATION + APPROVED LAYOUT SPEC** | Data foundation on `main`. Approved IA: two bars × three blocks ([`GP-Production-HUD-Layout-Spec`](Claude_Tasks/GP-Production-HUD-Layout-Spec.md)). Not implemented: authored `WBP_GP_HUD`, visible HUD, SelectionVM, Context Action Grid, minimap function, Patrol, Order Menu, notifications, production end-of-match. TEMP HUD remains active. Old resource/score top-right / selection bottom-left / command bar bottom-center / minimap top-or-bottom-right layout is **SUPERSEDED**. |
| Production Order Menu | **NOT STARTED** | Purchases are usable through TEMP HUD only. |
| Minimap | **NOT STARTED** | No minimap subsystem/VM/widget production code. |
| Notifications | **NOT STARTED** | No notification VM/stack or authority-to-client notification pipeline. |
| Feedback/VFX foundation | **PARTIAL** | Combat presentation multicast, team colors, health bars, primitive visuals, and placement feedback exist; planned bundle/pool/damage-flash pass is incomplete. |
| Building placement UI | **DONE (functional)** | Current ghost, per-cell valid/invalid state, payload mesh, and status text make deployment usable; further visual polish is not an MVP blocker. |

### Fog of War

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| Per-team Unexplored/Explored/Visible runtime | **DONE — FOUNDATION** | `UGP_FogOfWarComponent` owns authority-only per-team bit grids, 10 Hz registered-source recompute, sticky exploration, and public server queries; contract and operator validation passed. Canonical grid: 100 cm / 2000×2000 / 0.10 s. Client rendering/relevance remain separate capability work. |
| Trusted owning-client mirror + FoW MVVM | **DONE — FOUNDATION** | Owner-only initial/delta range sync, revision guards, one-team `UGP_LocalFoWComponent`, `UGP_FoWViewModel`, adapter, CommonUI base, and local placement preview passed contracts, final builds, and two-player operator isolation. |
| Visual world/terrain FoW | **DONE — MERGED / OPERATOR ACCEPTED** | PerCellBlurredQuadRenderer plus 100 cm / 10 Hz / 2000×2000 gameplay grid. Planar / fixed ground-projection assumption. Voxel terrain-surface adaptation is a later Terrain-stage integration task (do not reopen FoW now). |
| FoW-gated selection/combat/drop placement | **PARTIAL** | Server auto-acquire and orbital building placement consume active visibility. Enemy local selection, explicit-Attack last-known behavior, unit-drop pod vision, and broad relevance filtering remain deferred FoW integration. |
| Last-known state and minimap layers | **NOT STARTED** | Design only. |

### Terrain, leveling, and foundations

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| Voxel Plugin terrain backend | **NOT STARTED** | Intended backend per ADR-0010. Exact version/edition/API is **TECH-SPIKE REQUIRED**. |
| Authoritative terrain deformation | **NOT STARTED** | Generic deformation-event contract documented; no production service. Clients must not author destruction. |
| Worker terrain leveling / site prep | **NOT STARTED** | Command concept only (names TBD). Grey/yellow BuildGrid overlay. Zone sizing UX **DESIGN REQUIRED**. |
| Foundation Slab orbital procurement | **NOT STARTED** | Wall Package philosophy; cost/quantity/footprint **TBD** (do not copy 5). |
| Per-cell foundation coverage | **NOT STARTED** | Canonical model: track intact foundation per BuildGrid cell. Physical slab may cover multiple cells. |
| Building deploy requires foundation | **NOT STARTED** | Additional placement prerequisite for normal orbital buildings. Initial MainBase excepted. |
| Per-cell foundation destruction | **NOT STARTED** | Canonical; not all-or-nothing per original slab. Surviving-building-after-support-loss **DESIGN REQUIRED**. |
| Wall requires foundation? | **RESOLVED — NO** | Wall segments do not require Foundation Slabs. Terrain suitability TBD. Wall-mounted Turret follows Wall. |
| Generic local engineering job | **NOT STARTED** | Plan first, Worker assignment, physical work, completion. Exact job representation TBD. |
| Foundation Repair | **NOT STARTED** | Future Worker engineering job. Tunables TBD. |
| Earthquakes | **POST-MVP / NOT STARTED** | Reuse generic deformation/foundation-damage contract. Parameters TBD. |
| Dynamic nav after deformation | **DESIGN / TECH-SPIKE REQUIRED** | Do not assume full NavMesh rebuild per explosion. |
| World FoW follows voxel surface | **NOT STARTED** | Required integration in Terrain stage. Do not reopen FoW now. |

### RTS AI Opponent

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| `AGP_AIController` | **NOT STARTED** | No AI opponent production class; GP-S54 remains an unchecked implementation spec. |
| `UGP_AIBehaviorDefinition` | **NOT STARTED** | No production class/asset owner; GP-S55 remains an unchecked spec. |
| Explore/Mine/Ship/Order/Defend states | **NOT STARTED** | GP-S56 is design/pseudocode only. |

The RTS AI Opponent is a player-like strategic participant: it must Explore, Mine, Ship, Order, and
Defend using the same economy and authority rules as a player. It is not SWARM.

### Multiplayer and Steam

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| Replicated/server-authoritative gameplay primitives | **PARTIAL** | Core actors, GAS state, commands, economy, buildings, and match result are replicated/authority-owned. |
| Lobby state data | **PARTIAL** | `AGP_LobbyState` exists, but no complete lobby/session product flow was found. |
| Steam session subsystem | **NOT STARTED** | No `UGP_SessionSubsystem` or Host/Find/Join/Destroy implementation. |
| Main menu/lobby UI | **NOT STARTED** | No production CommonUI menu/lobby implementation. |
| ServerTravel/client join flow | **NOT STARTED** | Documented but not implemented as the MVP session flow. |
| Disconnect/failure handling | **NOT STARTED** | `AGP_GameMode::Logout` explicitly defers OpponentDisconnect victory; network/travel UX is absent. |

### Match flow and end

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| Countdown timer | **DONE** | Server countdown and replicated `MatchTimeRemaining`. |
| Delivery quota | **DONE** | Event-driven score threshold ends the match with DeliveryQuota reason. |
| Timer winner/tie-break | **DONE** | FerroniteScore, OrbitalFerronite, CurrentUnits, deterministic seed ladder. |
| MainBase annihilation | **DONE** | Authority MainBase death can finish match with Annihilation reason. |
| `FinishMatch` / `FGP_MatchResult` | **DONE** | Authority result creation and replicated result state. |
| Temporary result presentation | **DONE (temporary)** | TEMP HUD shows winner/reason. |
| Production end screen / return flow | **NOT STARTED** | No CommonUI EndOfMatch screen or return-to-menu/session cleanup. |
| Opponent disconnect result | **NOT STARTED** | Tag exists; GameMode explicitly logs it as deferred. |
| Full match start-to-return completion | **PARTIAL** | Core timer/result mechanics work, but menus, sessions, AI, SWARM, FoW, production UI, and return flow prevent the canonical end-to-end story. |

### SWARM

| Capability | Status | Factual evidence / boundary |
| --- | --- | --- |
| Threat input (`FerroniteThreatValue`) | **DONE** | Raw Planetary Ferronite currently stored in MainBase containers; drop-off raises it, launch lowers it. |
| SWARM actors/director/waves | **NOT STARTED** | No SWARM production classes, spawning pipeline, or wave director were found. |
| Final MVP design | **DESIGN REQUIRED** | Existing docs contain concepts and placeholders, not an approved implementable MVP contract. |

`FerroniteScore` and `OrbitalFerronite` do not drive SWARM pressure. SWARM is hostile PvE pressure and
must remain separate from the RTS AI Opponent.

## 4. Completed MVP capabilities that need no replacement slice

- RTS camera, selection, Move, Attack, auto-acquire, retaliation, and Attack-Move.
- GAS damage/health/death, health bars, team presentation, and Salvage Walker.
- Ferronite deposits, mining, cargo, MainBase containers, threat accounting, launch, orbital currency,
  and cumulative score.
- Unit catalog/manifest/cap validation and orbital unit delivery.
- Building purchase/READY/deploy, functional placement ghost, server validation/grid reservation,
  DropPod delivery, and DataAsset-owned payload/vitals.
- MainBase runtime role, Logistics Hub +5 cap, and free-standing Defensive Turret combat.
- Wall Package purchase, one-rocket delivery, pending state, and MainBase segment inventory.
- Match countdown, quota win, timer result, deterministic tie-break, annihilation, replicated result,
  and temporary result display.
- Authoritative three-state per-team FoW runtime, registered sight sources, persistent exploration,
  auto-acquire visibility gating, and authority building-placement visibility gating.
- Trusted one-team client FoW mirror, server-originated snapshot/delta sync, stale revision protection,
  team isolation, FoW ViewModel/CommonUI foundation, and local FoW-aware placement preview.
- World FoW presentation (PerCellBlurredQuadRenderer, planar / fixed ground projection) — **MERGED /
  operator accepted**. Voxel-surface adaptation is Terrain stage 3E.

Do not schedule another implementation slice merely to recreate an obsolete historical class name.

## 5. Remaining MVP capabilities

These are factual product gaps, not an implied strict order:

1. Continue the production CommonUI/MVVM HUD after the delivered Resource/Match data foundation
   (authored root and bounded visual panels remain).
2. Minimap + FoW minimap presentation.
3. Terrain / Voxel / Foundation system (3A–3E below).
4. Primitive RTS AI Opponent (`Explore / Mine / Ship / Order / Defend`).
5. Remaining bounded core-loop gameplay: player-facing Stop, Worker Repair, Logistics Hub storage-cap bonus, necessary feedback.
6. Building-system design gate (Wall terrain suitability, drag/path, Worker construction details, auto-connect, Wall Turret, Sell/Demolish).
7. Steam 2-player session/lobby/host/find/join/travel/disconnect flow.
8. Match completion product flow.
9. SWARM design/reconciliation gate.
10. SWARM implementation.
11. Full MVP validation/stabilization.

## 6. Deferred and redesign-dependent work

### Footprint/geometry ownership cleanup — **DEFERRED**

The current `PlacementFootprintBounds`, DataAsset footprint, native fallback, grid, reservation,
navigation-obstacle, snap, collision, and replicated placement behavior remain compatibility/runtime
infrastructure. A standalone ownership cleanup is not immediate MVP work.

### Building placement/construction redesign — **DESIGN REQUIRED / DEFERRED**

Wall surface construction and any broader redesign of building placement no longer wait on a Wall/foundation yes/no decision (**RESOLVED: Walls do not require Foundation**). Remaining Wall questions are terrain suitability, drag/path, Worker construction flow, auto-connect, Wall Turret, and Sell/Demolish. This does not invalidate the existing orbital building deployment capability, which is **DONE** as a delivery path; Terrain stage later adds leveled + intact foundation as an additional prerequisite for **normal orbital buildings**.

Neither item is an immediate optimization/refactor package. Revisit only when the concrete surface
construction redesign requires it.

## 7. Updated dependency-based implementation order

This order replaces mechanical continuation of the historical Slice 8 -> 13 sequence:

1. **Production UI foundation / HUD**
2. **Minimap + FoW minimap presentation**
3. **Terrain / Voxel / Foundation system** — must exist before AI and final building/wall design because both depend on construction-site rules and navigation. This stage must also establish the **generic local engineering job contract**, **Worker assignment/contribution model**, and **reusable work-presentation hooks** before final Wall implementation.
   - **3A.** Voxel Plugin technical spike + authoritative terrain deformation foundation
   - **3B.** Worker leveling + generic local engineering job/work hooks
   - **3C.** Foundation procurement / install / repair foundation support
   - **3D.** Building placement migration to leveled + intact foundation requirement
   - **3E.** navigation + current world-FoW terrain-surface integration
4. **RTS AI Opponent** — Explore/Mine/Ship/Order/Defend using completed authority APIs, FoW, and construction-site rules
5. **Remaining bounded core-loop gameplay** — player-facing Stop, Worker Repair, Logistics Hub storage-cap bonus, and necessary feedback
6. **Building-system design gate**, then only approved remaining surface-building capabilities
   (**Wall Foundation Rule — RESOLVED: Wall does not require Foundation.** Remaining: terrain suitability/slope, drag/path placement, local Worker construction flow details, auto-connect, Wall Turret, Sell/Demolish). Do not pre-emptively clean geometry.
7. **Steam multiplayer product flow** — sessions, lobby, host/find/join, travel, disconnect, and menus.
8. **Match completion product flow** — production end screen, OpponentDisconnect result, cleanup/
   return, and singleplayer/multiplayer completion checks.
9. **SWARM design/reconciliation gate.**
10. **SWARM implementation — last gameplay implementation stage of MVP.**
11. **Full MVP end-to-end validation and stabilization.**

Small slices may subdivide a stage, but dependency order and capability status take precedence over old
S-number titles.

## 8. SWARM final-MVP stage

**Status: MVP — FINAL IMPLEMENTATION STAGE**
**Gate: DESIGN REVIEW REQUIRED BEFORE IMPLEMENTATION**

SWARM must not be implemented from the current placeholders. The dedicated design/reconciliation gate
must resolve:

- what exactly constitutes SWARM in MVP;
- enemy archetypes and minimum roster;
- spawning model;
- spawn locations, zones, and constraints;
- wave/director model;
- `FerroniteThreatValue -> pressure/intensity/frequency` mapping;
- target selection and strategic objectives;
- interaction with MainBase, Workers, combat units, and defenses;
- navigation rules;
- scaling during a match;
- multiplayer/server authority;
- replication requirements;
- victory/loss interaction;
- performance/scalability budget;
- what is explicitly out of scope for MVP.

All items above are **DESIGN REQUIRED**. Existing references to Grunts, wave timing, spawn points, curves,
or priorities are concepts/placeholders, not approved final answers.

Canonical relationship already fixed:

- raw Planetary Ferronite currently in MainBase containers drives `FerroniteThreatValue`;
- Worker drop-off raises threat;
- container launch lowers threat;
- `FerroniteScore` does not drive SWARM pressure;
- `OrbitalFerronite` does not drive SWARM pressure.

## 9. Final MVP validation stage

After SWARM implementation:

- run the complete singleplayer core loop from launch through result and return;
- run the required Steam host/client full match and disconnect/failure matrix;
- validate economy, combat, building, FoW, AI Opponent, and SWARM interaction;
- validate server authority and replication under real host/client conditions;
- perform performance/scalability profiling against the approved SWARM budget;
- run full regression, soak/stability, packaging, Development, and Shipping validation;
- resolve only defects or optimization work that blocks MVP acceptance.

## 10. Historical mapping

| Historical ID/range | Current capability verdict |
| --- | --- |
| S01-S11 | Foundation, tags, GAS, match actors, asset loader, and lobby data largely **DONE**; lobby product flow remains partial. |
| S12-S15 | Camera capability **DONE**. |
| S16-S19 | Selection/smart command capability largely **DONE**; player-facing Stop emission remains **PARTIAL**. |
| S20-S23 | Movement/result propagation **DONE**; Stop execution exists but player-facing emission is **PARTIAL**. |
| S24-S28 | Attack foundation, mining, cargo, Worker loop, storage/threat **DONE** through original and reconciliation slices. |
| S29 historical | Combat fire/LOS **DONE — SUPERSEDED IMPLEMENTATION SHAPE** in unit-command/LOS pipeline. |
| S29R | Combat presentation, health bar, team colors, Salvage Walker **DONE**. |
| S30 historical | Auto-acquire **DONE — SUPERSEDED IMPLEMENTATION SHAPE** through GP-S30R. |
| S30 delivered | Container launch/orbital conversion **DONE**. |
| S31 historical | Damage capability **DONE**; cooldown uses attribute/FSM timing rather than requiring the historical GE shape. |
| S31R | Orbital unit drop **DONE**. |
| S32 historical | Attack-Move **DONE** through GP-S32A. |
| S32R | Orbital building purchase/READY/deploy **DONE**. |
| S33 | Combat presentation scaffold **DONE — SUPERSEDED IMPLEMENTATION SHAPE**. |
| S34 | Building base/definition capability **DONE**, including later canonical data-ownership reconciliations. |
| S35 | BuildGrid/placement validation **DONE** through GP-S36G naming. |
| S36 | Container launch **DONE** earlier as delivered S30. |
| S37-S38 | Orbital authority/catalog/DropPod/drop definitions **DONE — SUPERSEDED IMPLEMENTATION SHAPE**; no subsystem-class resurrection required. |
| S39 | MainBase **DONE**. |
| S40 | Logistics Hub **PARTIAL**: +5 cap done; storage-cap bonus missing. |
| S41 | Defensive Turret **DONE** through GP-S37T. |
| S42A | Wall Package purchase/delivery/inventory **DONE**. |
| S42B-S42C | Wall actor/connection/Build Wall drag **REMAINING**. Foundation-under-wall is **RESOLVED NO**. Remaining: terrain suitability, Worker construction job, auto-connect, Wall Turret. |
| S43 | Wall-mounted Turret **REMAINING**, dependent on wall system. |
| S44 | Building reticle/ghost capability **DONE — SUPERSEDED IMPLEMENTATION SHAPE** via `AGP_BuildingPlacementGhost`; wall ghost remains part of S42C concern. |
| S45 | Old per-segment pod cascade **SUPERSEDED — DO NOT IMPLEMENT**. |
| S46 | Worker Repair **REMAINING**. |
| S46A | Sell/Demolish **REMAINING**. |
| S47 | CommonUI/MVVM prerequisites and first project activatable/ViewModel base **DONE — FOUNDATION**; full production HUD remains. |
| S48 | FoW authority, trusted client mirror/MVVM, and world overlay **DONE / MERGED**; relevance/last-known remain. Voxel-surface FoW adaptation is Terrain stage 3E. |
| S49-S53 | Resource/Match VMs, adapters, local-player ownership, widget/HUD-root bases **DONE — FOUNDATION**; authored HUD/minimap/Order Menu and remaining panel VMs **REMAINING**. Approved two-bar layout spec is documented; visual HUD is not implemented. TEMP HUD remains active. |
| S54-S56 | RTS AI Opponent **REMAINING** and distinct from SWARM. |
| S57-S60 | Feedback pass **PARTIAL**; implement only MVP-readable gaps. |
| S61-S64 | Steam sessions/lobby/travel/menu **REMAINING**. |
| S65 | Quota/timer/annihilation/result **DONE** through GP-S34W; disconnect completion remains. |
| S66 | Production end screen **REMAINING**; TEMP result display exists. |
| S67 | Full-match stress/acceptance **REMAINING** and belongs after SWARM. |
| SWARM (new final gameplay stage) | **DESIGN REQUIRED, THEN IMPLEMENT LAST**. Not an alias for S54-S56. |

## 11. Immediate NEXT recommendation

**Production HUD layout documentation is ready for review.** Canonical IA is two bars × three
blocks. Visual HUD is still not implemented. Next implementation slice must follow
[`GP-Production-HUD-Layout-Spec`](Claude_Tasks/GP-Production-HUD-Layout-Spec.md).

Status: `PRODUCTION_HUD_LAYOUT_DOCUMENTATION_READY_FOR_REVIEW`. **NOT MERGED.**

Execution order remains: implement remaining production UI/HUD visuals using the approved layout →
minimap + FoW minimap → Terrain stage 3A–3E → RTS AI Opponent → bounded core-loop gaps →
Building-system / Walls gate → Steam → match completion → SWARM gate → SWARM → full MVP
stabilization.

**Wall Foundation Rule — RESOLVED:** Walls do not require Foundation. Do not list it as an open design gate.

Do not start minimap function, Order Menu, or Terrain runtime work in this documentation slice.
