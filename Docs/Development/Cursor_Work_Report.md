# Cursor Work Report — FoW World Visualization Operator Correction

## Status

**FOW_WORLD_VISUALIZATION_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `feature/gp-fow-world-visualization`
- Exact base: `origin/main` @ `7847c3ce27a571d92f7629369cc8d361bd981387`
- Validated correction implementation head: `857a97a4abe49ae25e9dabfa33746712d5248acc`
- Final branch head: report-record commit following the validated correction head

## Operator defect

Two-player PIE proved that terrain masks were team-isolated, but replicated enemy unit mesh/team
presentation and health bars remained visible through black Unexplored fog. The original viewport
overlay obscured terrain pixels but was not an actor-presentation policy.

## Enemy presentation hiding architecture

`UGP_LocalFoWUnitPresentationSubsystem` is a presentation-only `UWorldSubsystem` in `GPRuntime`.

- Every `AGP_UnitBase` lifecycle-registers in `BeginPlay` and unregisters in `EndPlay`; no
  `TActorIterator`, whole-world discovery, or per-unit Tick was added.
- It binds to the local `AGP_PlayerController`'s trusted one-team `UGP_LocalFoWComponent`.
- LocalFoW update/reset events immediately reevaluate the registered weak list.
- A repeating 0.1-second/10 Hz bounded registered-list evaluation catches replicated actor movement
  across a static FoW boundary when the FoW grid revision itself does not change.
- Own-team actors are always presentation-allowed, independent of LocalFoW state.
- Cross-team UnitBase actors (units and buildings) are allowed only when their current location is
  LocalFoW `Visible`; `Explored`, `Unexplored`, and NotReady deny presentation.
- Actor `SetActorHiddenInGame` gates all authored/generated primitive visuals and team-tinted meshes.
- `UGP_HealthBarComponent` receives an explicit LocalFoW gate.
- `UGP_CombatPresentationComponent` suppresses hidden actors' local cosmetic/debug presentation.
- Restore clears only the actor-level FoW hidden flag, reapplies current health policy, and refreshes
  the current team tint.

The gate does not destroy actors, disable collision, alter transforms/TeamId/death/combat, issue RPCs,
or change replication flags.

## Distinction from replication relevance

This is temporary local presentation hiding. Enemy actors still replicate and remain gameplay actors.
No `IsNetRelevantFor`, replication graph, dormancy, last-known snapshot, or server relevance policy
was added. Dedicated relevance/last-known work remains deferred.

## Health-bar policy and composition

Health and MaxHealth continue to update through existing GAS attribute delegates; no polling was added.

- `Health >= MaxHealth` within `max(KINDA_SMALL_NUMBER, abs(MaxHealth) * 1e-4)` — hidden.
- `0 < Health < MaxHealth - tolerance`, alive — health-policy visible.
- `Health <= 0`, invalid MaxHealth, or dead — hidden.

Final visibility is:

`owner/death allows && LocalFoW presentation allows && damaged-health policy allows`

Therefore a damage delegate cannot independently re-show an enemy health bar while FoW denies that
enemy. When the enemy becomes Visible again, the bar returns only if the actor is still damaged/alive.
The policy applies to all units/buildings using `UGP_HealthBarComponent`.

## Conservative smoothing

The source-only Slate projected-run renderer remains in place. It now:

1. samples the same LocalFoW enum states for the bounded viewport rectangle;
2. emits the existing exact base overlay runs;
3. detects only edges where a neighboring cell is more obscured;
4. adds a black alpha-gradient quad inside the less-obscured cell.

Smoothing width is `0.22` cell = **44 cm** at the current 200 cm gameplay cell size. Projected pixel
width varies naturally with camera zoom/pitch.

- Visible -> Explored: a dark gradient extends 44 cm into Visible; Explored stays at 0.68.
- Visible -> Unexplored: a black gradient extends 44 cm into Visible; Unexplored stays fully black.
- Explored -> Unexplored: additional black gradient extends 44 cm into Explored; Unexplored stays
  fully black.

This conservative direction may reduce visually clear area near an edge but never reveals a hidden
cell. It does not interpolate or promote LocalFoW state, recompute sight circles, change the 200 cm
grid, or affect combat/drop/placement visibility.

## Performance impact

- Sample work remains viewport-local and capped at 65,536 cells.
- One temporary plain `EGP_FoWState` array is capped at 65,536 entries; no per-cell UObject/component.
- Base runs remain coalesced and use 8,000 quads per Slate batch.
- Feather geometry is perimeter-driven in normal masks and hard-capped at 32,768 quads.
- Static camera + unchanged mirror serial reuses cached base/feather vertices.
- Unit presentation reevaluates only a lifecycle-registered weak list at 10 Hz plus immediate mirror
  events; there is no world scan and no per-frame actor scan.

## Gameplay visibility proof

The focused contract records LocalFoW revision and hidden-cell states before smoothing-policy calls
and verifies they remain unchanged. It also verifies the presentation gate does not change actor team,
location, replication enabled, or movement replication. Authoritative
`UGP_FogOfWarComponent` regressions remain green.

## Validation

- `gp.FoW.RunWorldVisualizationContractTest` — **PASS**, `Failures=0`
  - own unit never FoW-hidden;
  - enemy Visible/Explored/Unexplored/Visible presentation transitions;
  - damaged hidden enemy health-bar suppression;
  - gameplay/replication values unchanged;
  - conservative smoothing direction, no LocalFoW mutation/promotion, bounded geometry.
- `gp.FoW.RunClientPresentationFoundationContractTest` — **PASS**, `Failures=0`
- `gp.FoW.RunRuntimeFoundationContractTest` — **PASS**, `Failures=0`
- `gp.Combat.RunHealthBarContractTest` — **PASS**, `Failures=0`
  - full-health unit/building hidden, damaged visible, FoW composition, zero hidden.
- `gp.Combat.RunTeamColorContractTest` — **PASS**, `Failures=0`
- GPEditor Win64 Development + UHT — **PASS**
  - initial full correction build: succeeded;
  - final incremental build after VisualDump diagnostics: succeeded.

No focused unit-visual or combat-presentation contract exists. Team-color and health-bar contracts
cover the directly affected presentation components. No failure triggered broader-suite escalation.
GP Development and Shipping remain reserved for finalization after operator PASS.

## Changed files in correction head

Production:

- `GP/Source/GPRuntime/Public/Presentation/GPLocalFoWUnitPresentationSubsystem.h`
- `GP/Source/GPRuntime/Private/Presentation/GPLocalFoWUnitPresentationSubsystem.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Public/Presentation/GPHealthBarComponent.h`
- `GP/Source/GPRuntime/Private/Presentation/GPHealthBarComponent.cpp`
- `GP/Source/GPRuntime/Public/Combat/GPCombatPresentationComponent.h`
- `GP/Source/GPRuntime/Private/Combat/GPCombatPresentationComponent.cpp`
- `GP/Source/GPUIRuntime/Public/Presentation/GPFoWWorldPresentationSubsystem.h`
- `GP/Source/GPUIRuntime/Private/Presentation/GPFoWWorldPresentationSubsystem.cpp`
- `GP/Source/GPUIRuntime/Public/Widgets/GPFoWWorldOverlayWidget.h`
- `GP/Source/GPUIRuntime/Private/Widgets/GPFoWWorldOverlayWidget.cpp`

Contracts:

- `GP/Source/GPRuntime/Private/Debug/GPHealthBarContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`

Documentation:

- `Docs/Development/Claude_Tasks/GP-FoW-World-Visualization.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/Development/Cursor_Work_Report.md` (this report-record update)

## Protected content

Factual base-to-correction-head diff under `GP/Config`, `GP/Content`, and `Tools` is empty.

Existing local Config, map, Blueprint, DataAsset, material, VFX, Tools, and other Content changes
remain unstaged and untouched. The operator-local LongRange Salvage Walker UnitDefinition with
`Fog Of War Sight Radius = 2000` was not committed, reverted, restored, stashed, cleaned, or modified.

## Updated operator retest

1. Start two-player listen-server PIE in separate player windows.
2. Confirm each player still has a distinct black/grey/visible terrain mask.
3. For each player, confirm own units remain rendered even outside that player's LocalFoW.
4. Damage an own unit/building: bar appears; restore full health: bar hides; zero/dead stays hidden.
5. Move a damaged enemy from Visible into Explored and Unexplored:
   - mesh/authored primitives disappear;
   - team presentation disappears;
   - health bar disappears;
   - no leftover combat presentation is visible.
6. Move the enemy back into Visible: current mesh/team state returns and the health bar returns only
   if still damaged/alive.
7. Observe Visible/Explored, Visible/Unexplored, and Explored/Unexplored borders at normal zoom:
   confirm softer 44 cm transitions and no meaningful scene reveal inside hidden cells.
8. Run `gp.FoW.VisualDump`; confirm Ready/team/revision, registered presentation count, 0.10-second
   evaluation interval, bounded sampled/run/feather/batch counts.
9. Confirm commands, combat, server placement validation, and replication behavior remain unchanged.

No authored asset editing is required.

**NOT MERGED. NOT FINALIZED.**
