# Cursor Work Report — GP-S29R Health Bar Presentation Fix

## Status
GP-S29R_HEALTHBAR_FIX_READY_FOR_OPERATOR_VALIDATION

## Branch
feature/gp-s29r-combat-los-healthbar-teamcolors

## Base
d75fb426b043c80005c8363bef0f61ac37408fc5

## Root Cause
1. **Attachment lifecycle:** `UGP_HealthBarComponent` is created in `AGP_UnitBase` ctor before derived classes (`AGP_Worker` / `AGP_Unit` / `AGP_MainBase`) create `CapsuleComponent` and `SetRootComponent`. HealthBar had no ctor `SetupAttachment`. Prior attach ran only *after* `Super::BeginPlay()`, so WidgetComponent BeginPlay initialized while unattached / without a reliable scene transform.
2. **Native widget geometry:** C++-only `UGP_HealthBarWidget` relied solely on `NativePaint` without `RebuildWidget` Slate root, so WidgetComponent layout geometry could be zero.
3. **Screen-space draw tick:** WidgetComponent had `PrimaryComponentTick.bCanEverTick = false`, which prevents Screen-space projection/draw. Health attributes remain event-driven (no polling); Automatic tick is presentation-only.

## Fix
- `AGP_UnitBase::PostInitializeComponents` + `BeginPlay` (before/after Super) call `AttachHealthBarToOwnerRoot()` → `UGP_HealthBarComponent::EnsureAttachedToOwnerRoot()` snaps to **owner root** and applies `HealthBarWorldOffset`.
- `UGP_HealthBarWidget::RebuildWidget` returns fixed-size `SBox` Slate root; `SetLayoutDrawSize` syncs with DrawSize; paint uses non-deprecated `ToPaintGeometry`.
- WidgetComponent: `ETickMode::Automatic` + `RequestRedraw` on attribute refresh (still no Health polling).

## Attachment parents
| Actor | Health bar attach parent |
| --- | --- |
| Worker (`AGP_Worker`) | Worker `CapsuleComponent` (root) |
| MainBase (`AGP_MainBase`) | MainBase `CapsuleComponent` (root) |

Same path for all `AGP_UnitBase` descendants with a root set in derived ctors.

## Widget instance
Contract confirms `GetWidget()` is valid `UGP_HealthBarWidget` after spawn/Init for Worker and MainBase.

## HealthBar contract additions
After real spawn, for Worker and MainBase:
- IsRegistered
- AttachParent non-null and == owner root / same actor
- World location ≈ root + configured offset (5cm)
- WidgetClass valid / is `UGP_HealthBarWidget`
- GetWidget instance valid
- DrawSize X/Y > 1
- Visible / not HiddenInGame at full health
- Widget layout/desired size non-zero

## Tests

Headless `UnrealEditor-Cmd` `/Game/GrimProtocol/Maps/L_PrototypeArena` (`-game -nullrhi`).

| Command | Result |
| --- | --- |
| gp.Combat.RunHealthBarContractTest | Complete Failures=0 Cancelled=false |
| gp.Combat.RunTeamColorContractTest | Complete Failures=0 Cancelled=false |
| gp.Combat.RunLOSFireGateContractTest | Complete Failures=0 Cancelled=false |
| gp.Resource.RunS28RegressionSuite | Complete Failures=0 |

## Build
GPEditor Win64 Development + UHT — PASS

## Changed files
- `GP/Source/GPRuntime/Public/Presentation/GPHealthBarComponent.h`
- `GP/Source/GPRuntime/Private/Presentation/GPHealthBarComponent.cpp`
- `GP/Source/GPRuntime/Public/Presentation/GPHealthBarWidget.h`
- `GP/Source/GPRuntime/Private/Presentation/GPHealthBarWidget.cpp`
- `GP/Source/GPRuntime/Public/Units/GPUnitBase.h`
- `GP/Source/GPRuntime/Private/Units/GPUnitBase.cpp`
- `GP/Source/GPRuntime/Private/Debug/GPHealthBarContractTest.cpp`
- `GP/Source/GPRuntime/Public/Units/GPWorker.h`
- `Docs/Development/Claude_Tasks/GP-S29R_Combat_LOS_HealthBar_TeamColors.md`
- `Docs/Development/Cursor_Work_Report.md`

## Operator Local Assets
untouched: DefaultEngine.ini, L_PrototypeArena.umap, Blueprint/, Materials/, authored ResourceNode, Niagara, Tools/

## Commit
e6424059abc146c52f6be8a70401770da7ea6da4
