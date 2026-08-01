# GP-0401 MVP HUD

## Goal

Описати мінімальний HUD для first playable.

## Inputs

- [`../../GDD/09_UI_UX.md`](../../GDD/09_UI_UX.md)
- [`../../TDD/00_Technical_Overview.md`](../../TDD/00_Technical_Overview.md)
- [`../../../STYLE.md`](../../../STYLE.md)

## Code Allowed

No.

## Scope

HUD information architecture only. No widget code.

## Required Skill Pass

- `game-design-framework`
- `gp-mechanics-validator`

## Deliverables

- HUD state list: resource, unit cap, selected entity, commands, match state.
- UI ownership rules.
- Read-only replicated/GAS state list.
- Local selection state list.

## Validation

- UI does not own gameplay state.
- UI shows required MVP decisions.
- UI avoids non-MVP panels.

## Stop Condition

Зупинитися після HUD spec.

## Output

- Design spec: created [`../../TDD/12_UI_Architecture.md`](../../TDD/12_UI_Architecture.md) with framework rules + "Detailed MVP HUD Rules (GP-0401)" section.
- **Framework decisions (project-wide):**
  - Common UI + MVVM mandatory. Plugins: `CommonUI`, `CommonGame`, `CommonInput`, `ModelViewViewModel`.
  - All native widget bases: `UCommonUserWidget` / `UCommonActivatableWidget`.
  - **Server updates only ViewModels.** Widgets bind to VMs via `UMVVMSubsystem`; widgets never query gameplay state directly.
  - Adapter layer translates replicated state → VM `Set*` calls; FieldNotify auto-broadcasts to widgets.
- HUD state inventory complete (21 entries) з source/sync/replication contract.
- ViewModel inventory: `UGP_ResourceVM`, `UGP_MatchVM`, `UGP_SelectionVM`, `UGP_ProductionVM`, `UGP_ConstructionVM`, `UGP_CargoVM`, `UGP_BuildMenuVM`, `UGP_NotificationVM`, `UGP_MinimapVM`.
- Anti-patterns documented (Tick polling, direct ASC reads in widgets, raw `AddToViewport` for screens, widget→RPC).
- `GPUIRuntime.Build.cs` dependency list enumerated.
- Code implementation deferred to follow-up task **GP-0401A MVP HUD Implementation** (Code Allowed: Yes).
- Note: TDD/12 file створено цим task; GP-0701 expand-ить його з theme / localization / accessibility (backlog updated).
