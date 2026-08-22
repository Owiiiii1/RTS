# Cursor Work Report — Production HUD Threat Normalized

## Status

**HUD_THREAT_NORMALIZED_READY_FOR_OPERATOR_VALIDATION**

## Branch / base / head

- Branch: `feature/gp-hud-threat-normalized`
- Base: `origin/main` @ `ec1eb9981fd3dd798d913cb04d43973494fd44de`
- Head: `626c0ad4f9f84f356eaf58d76902cea68d6db224`
- **NOT MERGED**
- **NOT FINALIZED**

## Exact normalization formula

```
ThreatPresentationMax = Storage.GetTotalCapacity() * Storage.GetThreatPerStoredUnit()
FerroniteThreatNormalized = Clamp(FerroniteThreatValue / ThreatPresentationMax, 0, 1)
```

If `ThreatPresentationMax` is invalid or `<= 0`, normalized = `0`.

No hardcoded 1000. No gameplay ThreatMax. Raw `FerroniteThreatValue` is unchanged.

## Factual source getters / delegates

Getters:

- `AGP_GameState::GetFerroniteThreatValueForTeam`
- `AGP_GameState::FindMainBaseForTeamClientSafe`
- `AGP_MainBase::GetStorageComponent`
- `UGP_StorageComponent::GetTotalCapacity`
- `UGP_StorageComponent::GetThreatPerStoredUnit`

Delegates:

- `AGP_GameState::OnTeamFerroniteThreatValueChanged`
- `AGP_GameState::OnResolvedMainBaseChanged`
- `UGP_StorageComponent::OnStorageChanged`

## Event binding / unbinding lifecycle

`UGP_MatchViewModelAdapter::Initialize` binds GameState match/threat/result plus
`OnResolvedMainBaseChanged`, then binds the local team's MainBase `OnStorageChanged`.
`Shutdown` / `BeginDestroy` remove those handles and `RemoveDynamic` storage.
Rebind calls `Shutdown` first (no duplicate delegates).

## GPRuntime presentation hook

No new gameplay delegate type.

Existing `OnStorageChanged` now also broadcasts from:

- `ConfigureFromDefinition` (capacity/config applied on authority)
- `OnRep_StorageLayout` (`ContainerCapacity` / `ContainerCount` replication)

This closes the capacity-arrives-without-container-change gap without polling.

## HUDDump

`gp.UI.HUDDump` now includes:

`FerroniteThreatValue=<v> ThreatNormalized=<v> ThreatPresentationMax=<v>`

Example shape: `Threat=250.00 ThreatNormalized=0.50 ThreatPresentationMax=500.00`
(`FerroniteThreatValue` is the raw field; `ThreatPresentationMax` is adapter diagnostic only,
not a ViewModel property).

## Contract results

| Command | Result |
| --- | --- |
| `gp.UI.RunThreatPresentationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunProductionHUDFoundationContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunHUDViewModelBridgeContractTest` | **PASS**, Failures=0 |
| `gp.UI.RunHUDBootstrapContractTest` | **PASS**, Failures=0 |
| `gp.FoW.RunClientPresentationFoundationContractTest` | **PASS**, Failures=0 |

## GPEditor / UHT

`Build.bat GPEditor Win64 Development` — **PASS**

GP Win64 Development / Shipping **not run** (post-operator finalization).

## Exact changed files

- `GP/Source/GPUIRuntime/Public/ViewModels/GPMatchViewModel.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPMatchViewModel.cpp`
- `GP/Source/GPUIRuntime/Public/ViewModels/GPMatchViewModelAdapter.h`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPMatchViewModelAdapter.cpp`
- `GP/Source/GPUIRuntime/Private/ViewModels/GPHUDViewModelSubsystem.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPProductionHUDFoundationContractTest.cpp`
- `GP/Source/GPUIRuntime/Private/Debug/GPThreatPresentationContractTest.cpp`
- `GP/Source/GPRuntime/Public/Resources/GPStorageComponent.h`
- `GP/Source/GPRuntime/Private/Resources/GPStorageComponent.cpp`
- `Docs/TDD/12_UI_Architecture.md`
- `Docs/GDD/09_UI_UX.md`
- `Docs/Development/MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`
- `Docs/Development/AI_Project_Log.md`
- `Docs/Development/Cursor_Work_Report.md`

## Ownership / constraints

- Presentation projection only. Gameplay threat/storage/SWARM semantics unchanged.
- Widgets remain read-only. ResourceVM/MatchVM remain subsystem-owned.
- No Tick / timer / world scan. No `GetActorOfClass`.
- Authored ProgressBar remains operator-local `WBP_GP_HUD` work.
- **Content/protected files untouched**
- **NOT MERGED**
- **NOT FINALIZED**
