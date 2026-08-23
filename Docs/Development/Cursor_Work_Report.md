# Cursor Work Report — FoW Multiplayer Local Unit Visibility Fix

## Status

**FOW_MULTIPLAYER_UNIT_PRESENTATION_FIX_FINALIZED_READY_TO_MERGE**

**NOT MERGED.**

## Branch / base / head

- Branch: `fix/gp-fow-multiplayer-local-unit-visibility`
- Tracks: `origin/fix/gp-fow-multiplayer-local-unit-visibility`
- Base: `origin/main` @ `f20439388a3e3bf0f3389a10193634749995c21d`
- Implementation: `a42865f860d3870bb88c25bf10471b0cfe8daf84`
- Pre-finalization SHA-record: `b1ebefca5c25ca899d9224058c60be14f9fdd0fd`

## Operator PASS details

Operator validation **PASSED** in a 2-player PIE/network session:

- each player's FoW world visualization is correct
- Player 2 now sees enemy units according to Player 2's own FoW
- Player 1 visibility no longer reveals/hides units for Player 2
- reciprocal local visibility behavior is correct
- own-team units remain visible
- enemy units disappear/reappear according to the observing client's own FoW

This confirms the multiplayer presentation coupling bug is fixed.

## Confirmed root cause

`UGP_LocalFoWUnitPresentationSubsystem` already resolved visibility from **that client's** `UGP_LocalFoWComponent` via `ShouldPresentUnitForLocalPlayer`:

- invalid unit → false
- team ≤ 0 (neutral) → true
- own team (`UnitTeamId == LocalTeamId`) → true
- enemy → LocalFoW ready + matching LocalTeamId + `IsVisible(unit location)`

The leak was the apply path: `AGP_UnitBase::SetLocalFoWPresentationVisible` called `SetActorHiddenInGame`. `AActor::bHidden` is replicated. On a listen-server host, Player 1's FoW hide/show mutated shared actor hidden state and leaked onto Player 2.

FoW world overlay mirrors were already per-local-player and were **not** this bug.

## Exact old mechanism

Hide/show used Actor-level `SetActorHiddenInGame(!bVisible)`. HealthBar and Combat already had local gates, but the mesh/actor hide wrote replicated `bHidden` and coupled clients.

## Exact new mechanism

Keep the actor logically present. **Never** write Actor `bHidden` / `SetActorHiddenInGame`.

On local FoW hide:

- `GetComponents<UPrimitiveComponent>` (native capsule/generated parts and Blueprint/SCS/runtime-added meshes)
- skip `UWidgetComponent` (HealthBar owns composed visibility)
- cache each component's original `bHiddenInGame` in `TWeakObjectPtr` entries
- call local `UPrimitiveComponent::SetHiddenInGame(true, false)` (component `bHiddenInGame` is not replicated)

On show: restore original `bHiddenInGame`, `LocalFoWVisualPrimitives.Reset()`, refresh team presentation. Enumeration runs on visibility transitions only (the 10 Hz eval still early-outs when the local bool is unchanged). `EndPlay` restores.

## Why it is local-only

Engine fact: `USceneComponent::bHiddenInGame` / `SetHiddenInGame` on primitives is **not** replicated. Actor `bHidden` is never written by this path. Decision still uses only the observing client's `UGP_LocalFoWComponent`. Collision, movement, ASC, command components, and actor relevancy are untouched.

## Blueprint child component handling

`GetComponents<UPrimitiveComponent>` includes Blueprint-added visual primitives. The multiplayer contract attaches a child `UStaticMeshComponent` and proves it is gated and restored. `UWidgetComponent` is excluded from generic primitive gating.

## Healthbar / combat / team presentation

Preserved:

- `HealthBarComponent->SetFoWPresentationAllowed(bVisible)`
- `CombatPresentationComponent->SetLocalPresentationAllowed(bVisible)`
- `TeamPresentationComponent->RefreshTeamPresentation()` when becoming visible

Damaged enemy bars cannot show while local FoW disallows presentation. Combat multicast still no-ops when local presentation is disallowed. HealthBar FoW gating remains intact; CombatPresentation local gating remains intact.

hidden → visible → hidden restores original component hidden state (cache original flag, restore on show, reset cache, recapture on next hide). Weak ptrs skip destroyed components; cache lives only while hidden.

## Focused contract results

Re-run after operator PASS (`UnrealEditor-Cmd.exe`, `/Game/GrimProtocol/Maps/L_PrototypeArena`, `-game -nullrhi`):

| Command | Result |
| --- | --- |
| `gp.FoW.RunMultiplayerUnitPresentationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunWorldVisualizationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunProductionHUDFoundationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS**, Failures=0 |

Full suite **not run** (no focused failure; no unexpected shared-system expansion).

## GPEditor / UHT

`Build.bat GPEditor Win64 Development` — **PASS** (UHT processed; 0 generated files written)

## GP Development

`Build.bat GP Win64 Development` — **PASS** (`GP.exe`)

## GP Shipping

`Build.bat GP Win64 Shipping` — **PASS** (`GP-Win64-Shipping.exe`)

## Factual final diff review vs `origin/main`

Reviewed `origin/main...HEAD` plus uncommitted docs. **No defects found. No code fix required.**

| Check | Result |
| --- | --- |
| local FoW presentation no longer calls `SetActorHiddenInGame` | **confirmed** (none remain under `GP/Source`) |
| Actor `bHidden` is not used for local FoW gating | **confirmed** |
| actor replication/relevancy semantics unchanged | **confirmed** (`GetLifetimeReplicatedProps` still only `TeamId` / `bIsDead`; no `IsNetRelevantFor` change) |
| each client resolves visibility from its own `UGP_LocalFoWComponent` | **confirmed** (`ShouldPresentUnitForLocalPlayer`) |
| own-team units remain always presented | **confirmed** |
| enemy visibility = LocalFoW ready + matching LocalTeamId + `IsVisible(location)` | **confirmed** |
| local hide/show only affects presentation components | **confirmed** |
| collision unchanged | **confirmed** |
| movement unchanged | **confirmed** |
| ASC unchanged | **confirmed** |
| command components unchanged | **confirmed** |
| Blueprint-added visual primitive components covered | **confirmed** |
| `UWidgetComponent` not incorrectly hidden by generic primitive gating | **confirmed** |
| HealthBar FoW gating intact | **confirmed** |
| CombatPresentation local gating intact | **confirmed** |
| TeamPresentation refresh on becoming visible | **confirmed** |
| hidden → visible → hidden restores original component hidden state | **confirmed** |
| no stale component cache safety issue | **confirmed** (weak ptrs; cache reset on show; EndPlay restore) |
| no gameplay/network authority state mutation introduced | **confirmed** |
| no Content/protected files committed | **confirmed** |

## Exact changed files vs `origin/main`

- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPFoWMultiplayerUnitPresentationContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPFoWWorldVisualizationContractTest.cpp`
- `Docs/TDD/15_Fog_of_War.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Protected / content files untouched

- **Content untouched** (no committed `GP/Content/**`, unit Blueprints, maps, DataAssets, or `WBP_GP_HUD`)
- Protected Config/maps/DataAssets/Tools **untouched / unstaged**
- Local dirty Content/Config remain unstaged and are **not** part of this branch

## Gameplay replication / relevancy

Unchanged. This is **presentation correctness**, not network secrecy / replication culling.

## Merge

**NOT MERGED.**
