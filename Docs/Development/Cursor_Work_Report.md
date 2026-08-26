# Cursor Work Report

## Status

**BOTTOM_HUD_MARQUEE_UNITS_ONLY_READY_FOR_OPERATOR_VALIDATION**

This is an **INTERMEDIATE Bottom HUD checkpoint**, not merge-ready. Do not merge. Do not run production finalization. Group/Context Action seams from the previous checkpoint remain; this slice only changes marquee eligibility.

## Branch / base / head

- Branch: `ui/gp-bottom-hud`
- Base: `origin/main` @ `0667b6f912fce288422848d5d2355bc4510b748c`
- Head: this implementation commit on `ui/gp-bottom-hud`
- Behind `origin/main`: **0**
- `GP Win64 Development` / `GP Win64 Shipping` / full suite: **not run** (intermediate gate)

## Exact marquee filter

Path: `AGP_PlayerController::ResolveAndApplyMarqueeSelection` in `GPPlayerController.cpp`.

Candidates are `TActorIterator<AGP_UnitBase>`. A candidate is kept only if **all** of the following hold:

1. `IsValid(Unit)`
2. `Unit->GetTeamId() == LocalTeamId` (local PS team; blocked if team `< 1`)
3. `Unit->IsGameplaySelectable()` (dead → false; requires `Capability_Selectable`)
4. **New:** `Unit->IsSelectionTypeUnit() == true`
5. **New:** `Unit->IsSelectionTypeBuilding() == false`
6. `ProjectWorldLocationToScreen` succeeds
7. Projected point is inside the axis-aligned screen rectangle

No class hardcode (Worker / SalvageWalker / MainBase / LogisticsHub / DefensiveTurret). Semantic seam is `IsSelectionTypeUnit()` / `IsSelectionTypeBuilding()`.

FoW / local-visibility was **not** previously in this filter and was **not** added.

Selection cap **24** unchanged. Shift/Ctrl still operate on the filtered candidate list only (Replace / Add / Toggle). Mixed unit+building via marquee can no longer occur.

## Click-selection unchanged

`ProcessSelectionClickAtScreenPosition` body was not edited. Click on a friendly selectable building still selects that building; click on a friendly selectable unit still selects that unit.

Contract-only wrappers (not UFUNCTION, not gameplay input):

- `ApplyMarqueeSelectionForContract` → `ResolveAndApplyMarqueeSelection`
- `ProcessSelectionClickForContract` → `ProcessSelectionClickAtScreenPosition`

## Context Action consequence

Marquee over units + building now yields **UnitGroup** (all selected are units), not **None** from mixed types. Click-selected single MainBase remains **MainBase** mode.

## Exact changed files

New:

- `GP/Source/GPRuntime/Private/Debug/GPSelectionMarqueeContractTest.cpp`

Modified:

- `GP/Source/GPRuntime/Public/Player/GPPlayerController.h`
- `GP/Source/GPRuntime/Private/Player/GPPlayerController.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPContextActionPresentationContractTest.cpp`
- `Docs/Development/Cursor_Work_Report.md` (this file)

## Exact focused tests

`L_PrototypeArena` `-game -unattended -nop4 -NullRHI`. Editor killed after Complete (no `quit` in ExecCmds). Exit `-1` after kill is not a test failure.

| Command | Result |
| --- | --- |
| `gp.Selection.RunMarqueeUnitsOnlyContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunContextActionPresentationContractTest` | **Complete Failures=0 Cancelled=false** |
| `gp.UI.RunSelectionViewModelContractTest` | **Complete Failures=0 Cancelled=false** |

Marquee contract coverage:

- 2 friendly units + MainBase in one rect → selected count **2**, MainBase absent
- friendly building only in rect → selection **empty**
- single click projected MainBase → MainBase **still selected**

Context Action case **K**: same mixed marquee → mode **UnitGroup**, count 2, MainBase not selected.

Full suite / GP Development / GP Shipping: **not run**.

## GPEditor / UHT

| Target | Result |
| --- | --- |
| `GPEditor Win64 Development` + UHT | **Succeeded** (UHT processed `GPPlayerController.h`; 0 generated files written — contract seams are not UFUNCTION) |

## Protected-file audit

**Not staged / not committed:**

- `GP/Config/DefaultEngine.ini`
- `GP/Config/DefaultGame.ini`
- `GP/Content/GrimProtocol/Maps/L_PrototypeArena.umap`
- `GP/Content/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.uasset`
- `GP/Content/Basic_VFX/`
- `GP/Content/GrimProtocol/Blueprint/` (includes `WBP_GP_HUD`)
- `GP/Content/GrimProtocol/DataAssets/`
- `GP/Content/GrimProtocol/Materials/`
- `GP/Content/Mixed_Magic_VFX_Pack/`
- `GP/Content/RocketThrusterExhaustFX/`
- `Tools/`
- `GP/GP.uproject`

No `git reset --hard`, `git clean`, `git restore .`, or broad stash.

## Operator note

Marquee/drag must select units only. Click a friendly MainBase / LogisticsHub / turret to inspect or select that building. Mixed marquee should no longer produce a mixed group.

After operator PASS: **do not finalize**. Next Cursor task on this same branch remains PURCHASE categories.
