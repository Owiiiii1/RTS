# Cursor Work Report — FoW Multiplayer Local Unit Visibility Fix

## Status

**FOW_MULTIPLAYER_UNIT_PRESENTATION_FIX_READY_FOR_OPERATOR_VALIDATION**

**NOT MERGED. NOT FINALIZED.**

## Branch / base / head

- Branch: `fix/gp-fow-multiplayer-local-unit-visibility`
- Base: `origin/main` @ `f20439388a3e3bf0f3389a10193634749995c21d`
- Implementation head: `a42865f860d3870bb88c25bf10471b0cfe8daf84`

## Confirmed root cause

`UGP_LocalFoWUnitPresentationSubsystem` already decided visibility from the local player's `UGP_LocalFoWComponent`. The leak was the apply path:

`AGP_UnitBase::SetLocalFoWPresentationVisible` called `SetActorHiddenInGame`.

`AActor::bHidden` is a replicated Actor property. On a listen-server host that mutates shared actor hidden state, so Player 1's FoW presentation replicated onto Player 2's unit visuals. FoW world overlay mirrors were already per-local-player and were not this bug.

## Exact old behavior

Hide/show used Actor-level `SetActorHiddenInGame(!bVisible)`. HealthBar and Combat already had local gates, but the mesh/actor hide leaked across clients.

## Exact new presentation mechanism

Keep the actor logically present. On local FoW hide:

- enumerate owned visual `UPrimitiveComponent`s (including Blueprint/SCS/runtime-added meshes)
- skip `UWidgetComponent` (HealthBar owns its composed visibility)
- cache each component's original `bHiddenInGame`
- call local `UPrimitiveComponent::SetHiddenInGame(true)` (component `bHiddenInGame` is not replicated)

On show: restore original `bHiddenInGame` and refresh team presentation. Enumeration runs on visibility transitions only (the 10 Hz eval still early-outs when the local bool is unchanged).

## Why it is local-only

Component `SetHiddenInGame` does not replicate. Actor `bHidden` is never written by this path. Collision, movement, ASC, commands, and actor relevancy are untouched.

## Component / Blueprint child handling

`GetComponents<UPrimitiveComponent>` includes native capsule/generated visual parts and Blueprint-added meshes. A contract-spawned child `UStaticMeshComponent` is gated and restored. Widget health bars are excluded from that list.

## Healthbar / combat leak handling

Preserved:

- `HealthBarComponent->SetFoWPresentationAllowed(bVisible)`
- `CombatPresentationComponent->SetLocalPresentationAllowed(bVisible)`
- `TeamPresentationComponent->RefreshTeamPresentation()` on become-visible

Damaged enemy bars cannot show while local FoW disallows presentation. Combat multicast still no-ops when local presentation is disallowed.

## Focused contract results

| Command | Result |
| --- | --- |
| `gp.FoW.RunMultiplayerUnitPresentationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunWorldVisualizationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunProductionHUDFoundationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS**, Failures=0 |

Full suite **not run** (no focused failure; no unexpected architecture expansion).

## GPEditor / UHT

`Build.bat GPEditor Win64 Development` — **PASS**

GP Development / Shipping **not run** (post-operator finalization).

## Exact changed files vs `origin/main`

- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPFoWMultiplayerUnitPresentationContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected files / limitations

- **Content untouched** (no `GP/Content/**`, unit Blueprints, or `WBP_GP_HUD`)
- Protected Config/maps/DataAssets/Tools **untouched / unstaged**
- Gameplay replication/relevancy unchanged
- This is client presentation correctness, not FoW network secrecy
- **NOT MERGED**
- **NOT FINALIZED**
