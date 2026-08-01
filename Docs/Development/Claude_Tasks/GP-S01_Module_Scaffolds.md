# GP-S01 Module Scaffolds

## Slice Group
Slice 1 — Foundation

## Code Allowed
Yes — after explicit approval of GP-0601 **and** acceptance of DOCS-001 / SETUP-001 baseline.

## Depends On
- GP-0601 approved.
- SETUP-001 DONE (blank `GP` project on UE 5.8.1).
- DOCS-001 accepted (documentation cursor reset).

## Baseline Note (2026-08-01)
Current repo has only the Unreal primary module `GP` under `GP/Source/GP/`. Runtime modules `GPRuntime` / `GPGASRuntime` / `GPUIRuntime` are **not** present. This slice must **scaffold** those three modules (minimal module headers/cpp + Build.cs + target `ExtraModuleNames` / uproject Modules list as required by UE), not merely verify an already-imported foundation tree. Do not copy gameplay classes from any nested docs import.

## Goal
Create three runtime modules (`GPRuntime`, `GPGASRuntime`, `GPUIRuntime`) so they compile, link, and have correct dependency order. Enable Common UI / MVVM / ModelViewViewModel plugins.

## Scope
- Scaffold empty/minimal `GPRuntime`, `GPGASRuntime`, `GPUIRuntime` modules (module `.h`/`.cpp` + `.Build.cs` only; no gameplay classes).
- Wire modules into `GP.uproject` / targets as required for editor load.
- Update `GP.uproject` plugin list to enable: `CommonUI`, `CommonInput`, `CommonGame`, `ModelViewViewModel`.
- Set `GPRuntime.Build.cs` dependencies: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `OnlineSubsystem`, `OnlineSubsystemUtils`, `NetCore`, `UMG`, `GPGASRuntime`.
- Set `GPGASRuntime.Build.cs` dependencies: `Core`, `CoreUObject`, `Engine`, `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `NetCore`.
- Set `GPUIRuntime.Build.cs` dependencies: `UMG`, `Slate`, `SlateCore`, `GameplayAbilities`, `GameplayTags`, `CommonUI`, `CommonInput`, `CommonGame`, `ModelViewViewModel`, `GPRuntime`, `GPGASRuntime`.
- Verify `DefaultEngine.ini` / `DefaultInput.ini` consistent з module plan.
- Compile editor + standalone targets.

## Out of Scope
- Gameplay class additions (separate slices S02+).
- DataAsset class creation.
- UI widget creation.
- Importing prebuilt foundation trees from external/nested doc copies.

## Required Skill Pass
- `ue5-architecture`

## Files Touched
- `GP/GP.uproject` — Modules + Plugins arrays
- `GP/Source/GP.Target.cs` / `GP/Source/GPEditor.Target.cs` — ExtraModuleNames as needed
- `GP/Source/GPRuntime/` — module scaffold (`GPRuntime.h/.cpp`, `GPRuntime.Build.cs`)
- `GP/Source/GPGASRuntime/` — module scaffold (`GPGASRuntime.h/.cpp`, `GPGASRuntime.Build.cs`)
- `GP/Source/GPUIRuntime/` — module scaffold (`GPUIRuntime.h/.cpp`, `GPUIRuntime.Build.cs`)
- Config verification only: `DefaultEngine.ini` / `DefaultInput.ini` (no gameplay content)

## Acceptance Criteria
- [ ] `GPRuntime`, `GPGASRuntime`, `GPUIRuntime` exist under `GP/Source/` with module entry points + Build.cs.
- [ ] `GP.uproject` lists the three runtime modules and includes 4 plugin entries (CommonUI, CommonInput, CommonGame, ModelViewViewModel).
- [ ] Editor target compiles clean on UE 5.8.1.
- [ ] DevelopmentEditor + Development + Shipping configurations build.
- [ ] No new compile warnings introduced by this slice.
- [ ] Module dependency graph is `GPUIRuntime → GPRuntime → GPGASRuntime` (no upward dependency).
- [ ] No fourth gameplay runtime module created.
- [ ] No gameplay classes / DataAssets / widgets added beyond bare module scaffolds.

## Playtest / Validation Note
Open editor. Confirm no missing-module errors. Open Project Settings → Plugins → Common UI enabled. `Window → Output Log` clean of `LogModuleManager` errors. No assets touched.

## Risks / Edge Cases
- CommonUI may pull additional dependencies (e.g., `WidgetCarousel`); enable as transitive needed. Document if so.
- Mac/Linux builds out of MVP scope but сheck linux warnings if CI runs cross-platform.

## Linked
- [`../../TDD/13_Architecture_Proposal`](../../TDD/13_Architecture_Proposal.md) §Module Plan.
- [`../../TDD/01_Module_Architecture`](../../TDD/01_Module_Architecture.md).
- [`../../TDD/12_UI_Architecture`](../../TDD/12_UI_Architecture.md) §Required Plugins.
- [`../../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering`](../../Architecture_Decisions/ADR_0006_Indie_Scope_No_Overengineering.md).

## Stop Condition
STOP. Await approval before GP-S02.
