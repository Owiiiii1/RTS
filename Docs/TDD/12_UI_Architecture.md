# UI Architecture

## Scope

Engineering-canonical UI architecture for GrimProtocol. Canonical in-match HUD IA:
[`GP-Production-HUD-Layout-Spec`](../Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md) +
[`../GDD/09_UI_UX`](../GDD/09_UI_UX.md). Broader UI architecture (theme system, localization,
settings panel, accessibility, scaling) — pending GP-0701.

This TDD describes **state ownership, data binding, replication contracts, module separation, and
the approved two-bar layout**. The coarse pre-2026-08-21 HUD placement is **SUPERSEDED**.

## Framework — Common UI + MVVM

**HARD REQUIREMENT — canon, non-negotiable for all UI.** Project-wide rule (confirmed canon, per `project_ui_framework` memory and Audit Section 8):

- UE **Common UI** plugin для widget hierarchy, input routing, focus stack, activation system.
- **MVVM** pattern (UE `ModelViewViewModel` plugin) для data binding.
- **Authoritative state reaches widgets only through ViewModels.** Replication/trusted mirrors feed
  adapters, adapters update ViewModels, and widgets never query gameplay components, attributes, або
  actor state напряму.

### Required Project Setup (Prerequisite)

These mandatory prerequisites are enabled in the current project. Keep them intact for all production
UI work.

**1. `GP.uproject` — enable stock UE 5.8.1 plugins only:**

- `CommonUI` (engine plugin; Common Input capability comes with Common UI)
- `ModelViewViewModel` (engine plugin)
- `EnhancedInput` where not already enabled by the blank project
- **Do not enable `CommonGame`.** It belongs to the Lyra / CommonUser ecosystem, is not assumed present in a blank UE 5.8.1 install, and is forbidden by [ADR-0005](../Architecture_Decisions/ADR_0005_No_Lyra.md).
- **Do not require a separate `CommonInput` plugin entry** unless the installed engine exposes it as a standalone plugin. Prefer `CommonInput` as a Build.cs **module** dependency under Common UI.

**2. `GPUIRuntime.Build.cs` — declare module dependencies:** must depend on `CommonUI`, `CommonInput` (module), `ModelViewViewModel`, and `UMG` — **not** `CommonGame`:

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine",
    "UMG", "Slate", "SlateCore",
    "CommonUI", "CommonInput",
    "ModelViewViewModel",
    "GameplayAbilities", "GameplayTags",
    "GPRuntime", "GPGASRuntime"
});
```

**3. Module-boundary guideline (verify):** `GPRuntime` should **NOT** depend on `UMG` or any UI module — all UI lives in `GPUIRuntime`. This keeps gameplay free of a hard dependency on the UI layer. Flagged as a guideline to verify against the actual `GPRuntime.Build.cs`.

**4. No Lyra UI foundation:** do not import Lyra, `CommonGame`, `CommonUser`, or Lyra sample `CommonPlayerController` / experience stack. Project PlayerController remains a GP-owned `APlayerController` subclass that uses Common UI action routing APIs available from the Common UI plugin.

> Until the `.uproject` plugins and `GPUIRuntime.Build.cs` dependencies above are in place, none of the class bases or binding contracts below will compile. Treat this as a gate.

### Class Bases

| Role | Base class | Notes |
| --- | --- | --- |
| HUD root | `UGP_HUDRootWidget : UGP_UserWidgetBase` (BP child for layout) | Production lifetime root. Authored child `WBP_GP_HUD` is operator-local (not committed). Runtime bootstrap is implemented. |
| Activatable screen (OrderMenu, EndOfMatch, Pause, Lobby) | `UCommonActivatableWidget` | Pushed into `UCommonActivatableWidgetStack`. |
| Inline panels (SelectionPanel, ResourceReadout, MatchTimer, etc.) | `UGP_UserWidgetBase : UCommonUserWidget` | Never raw `UUserWidget`. |
| Buttons / list items | `UCommonButtonBase`, `UCommonListView` items | Reusable styles via `UCommonButtonStyle` DataAssets. |
| ViewModels | `UMVVMViewModelBase` subclasses | Properties marked `UPROPERTY(FieldNotify)`. |

### Current production foundation — finalized (2026-08-20)

- `CommonUI` and `ModelViewViewModel` are enabled; `GPUIRuntime` already depends on both and depends
  forward on `GPRuntime` / `GPGASRuntime`.
- `UGP_ActivatableWidgetBase : UCommonActivatableWidget` is the first project-owned production CommonUI
  base. No screen stack, HUD root, or authored Widget Blueprint is created by this foundation.
- `UGP_FoWViewModel : UMVVMViewModelBase` is the first production ViewModel. It exposes local team,
  grid metadata, readiness, and a coarse revision FieldNotify plus a read-only world-location query.
- `UGP_FoWViewModelAdapter` binds directly to the local PlayerController's trusted
  `UGP_LocalFoWComponent` update delegate. It has no Tick and never scans the world.
- The server updates the owning-client mirror; the adapter projects that trusted replicated
  presentation state into the ViewModel. Widgets remain read-only and never call FoW gameplay authority.
- Listen-host and remote-client team isolation plus restart/reinitialization passed operator validation.
- `UGP_FoWWorldPresentationSubsystem` is the first specialized world-presentation consumer. It binds
  directly to the trusted one-team LocalFoW mirror, not gameplay authority, and paints a viewport-local
  **PerCellBlurredQuadRenderer** (one feathered world-space tile per non-Visible cell). Gameplay
  FoW is 100 cm / 10 Hz / 2000×2000. Post-process and fullscreen/sampled mask approaches are abandoned.
- This direct mirror binding is intentionally limited to the project-owned world renderer. Ordinary
  HUD/minimap widgets still consume ViewModels.
- The TEMP HUD has been retired. Production HUD is the active match HUD (`UGP_HUDViewModelSubsystem` → configured `ProductionHUDWidgetClass` → `WBP_GP_HUD`).

### Production HUD data foundation, ViewModel bridge, and bootstrap — operator-validated / finalized (2026-08-21)

- `UGP_UserWidgetBase : UCommonUserWidget` is the project-owned base for non-activatable widgets.
  `UGP_ActivatableWidgetBase` remains the base for modal/activatable screens.
- `UGP_HUDRootWidget : UGP_UserWidgetBase` is the native root for authored `WBP_GP_HUD`.
  `NativeConstruct` (after Super) resolves the owning `ULocalPlayer`, reads
  `UGP_HUDViewModelSubsystem`, and assigns the **already owned** Resource/Match ViewModels into
  authored Manual MVVM slots `GP_ResourceViewModel` / `GP_MatchViewModel` via UE 5.8
  `UMVVMSubsystem::GetViewFromUserWidget` + `UMVVMView::SetViewModel(FName, TScriptInterface<INotifyFieldValueChanged>)`.
  The widget never creates ViewModels, never queries ASC/PlayerState/GameState, and does not Tick or
  poll. Missing LocalPlayer / subsystem / MVVM View / slot fails safely with a non-shipping warning.
  Operator-validated: authored `WBP_GP_HUD` became visible at runtime and
  `GP_ResourceViewModel.OrbitalFerronite` → To Text (Float) → `TXT_OrbitalFerroniteValue.Text`
  updated live. Remaining HUD fields/actions are not claimed complete. `WBP_GP_HUD` remains
  operator-local and is not committed.
- `UGP_ResourceViewModel` exposes FieldNotify `OrbitalFerronite`, `FerroniteScore`, `CurrentUnits`,
  `MaxUnits`, `OpponentFerroniteScore`, and `PlanetFerronite`. Numeric types remain `float`, matching
  current GAS attributes. `PlanetFerronite` is the exact raw stored Ferronite from the local team's
  MainBase `UGP_StorageComponent::GetTotalStored()`. It is not a second currency and is not
  reconstructed from `FerroniteThreatValue`. Operator-validated: authored
  `TXT_PlanetFerroniteValue` is bound to `GP_ResourceViewModel.PlanetFerronite` through
  To Text (Float) and updates in PIE. `WBP_GP_HUD` remains operator-local and uncommitted.
- `UGP_MatchViewModel` exposes factual current `AGP_GameState` presentation fields:
  `MatchTimeRemaining`, `MatchStateTag`, owning-team `FerroniteThreatValue`,
  `FerroniteThreatNormalized`, `WinnerTeamId`,
  `WinReasonTag`, `MatchDuration`, and `bMatchFinished`.
  `FerroniteThreatNormalized` is presentation-only: Clamp(FerroniteThreatValue /
  (MainBase `GetTotalCapacity()` × `GetThreatPerStoredUnit()`), 0, 1). Invalid/zero
  denominator → 0. There is no gameplay ThreatMax and no hardcoded 1000. Raw
  `FerroniteThreatValue` remains. Operator-validated: authored `PB_Threat.Percent` is bound to
  `GP_MatchViewModel.FerroniteThreatNormalized` and updates in PIE; operator-local `ThreatToColor`
  drives Fill Color and Opacity with green → yellow → red. `WBP_GP_HUD` and `ThreatToColor`
  remain uncommitted. Final art is not claimed complete. Current gameplay/storage tuning can
  drive the bar to high/full quickly; later UX/balance tuning may revisit the presentation scale.
- `UGP_HUDViewModelSubsystem : ULocalPlayerSubsystem` owns exactly one ResourceVM, MatchVM, and their
  push adapters per local player. Authored `WBP_GP_HUD` Manual MVVM entries receive those same
  instances from `UGP_HUDRootWidget` (no duplicate ViewModels). Widgets remain read-only consumers.
- Resource binding uses current GAS attribute-change delegates on the local PlayerState ASC and the
  opposing PlayerState's public `FerroniteScore`. Opponent resolution is through the replicated
  `AGP_GameState::PlayerArray`, never a world actor scan. `PlanetFerronite` is bound from the local
  team's MainBase via `AGP_GameState::FindMainBaseForTeamClientSafe` plus
  `UGP_StorageComponent::OnStorageChanged` / `AGP_GameState::OnResolvedMainBaseChanged`.
  No Tick, timer, polling, or world scan.
- Match binding uses existing `AGP_GameState` timer/state/per-team-threat/result delegates. The only
  gameplay-side additions are read-only lifecycle notifications:
  `AGP_PlayerController::OnPlayerStatePresentationReady` for the owning link and
  `AGP_GameState::OnPlayerStateRosterChanged` for player add/remove. They make late replication and
  travel rebind explicit without introducing UI types into gameplay.
- Lifecycle retry is event-driven: `ULocalPlayerSubsystem::PlayerControllerChanged`,
  `UWorld::GameStateSetEvent`, GameState roster changes, and PlayerState team changes. There is no
  UI Tick or timer polling. Rebind removes old handles before adding new ones.
- `gp.UI.HUDDump` reads only subsystem/ViewModel state for operator diagnostics, including
  `PlanetFerronite` next to `OrbitalFerronite`.
- Production HUD runtime creation is owned by the same `UGP_HUDViewModelSubsystem`
  (GPUIRuntime `ULocalPlayerSubsystem`). There is no second LocalPlayer UI owner and
  `AGP_PlayerController` does not reference GPUIRuntime types. `GPRuntime` still does not
  depend on `GPUIRuntime`.
- Authored class is soft-configured via `UGP_UIPresentationSettings::ProductionHUDWidgetClass`
  (`TSoftClassPtr<UGP_HUDRootWidget>`). Project Settings → Game → GP UI Presentation.
  This slice does not write `GP/Config` and does not hardcode `/Game/.../WBP_GP_HUD`.
  Unconfigured class → safe no-op + non-shipping warning. TEMP HUD is retired and is not a fallback.
- For each valid local player/controller the subsystem creates at most one production HUD
  root, adds it to that player's viewport (`SelfHitTestInvisible`: root/background does not
  consume clicks, interactive children remain clickable; no gameplay mouse consume,
  no input-mode change), then `UGP_HUDRootWidget::NativeConstruct` injects subsystem-owned
  VMs through the existing bridge. Repeated ensure does not duplicate. Teardown removes
  the widget and clears the reference (Initialize / PlayerControllerChanged / Deinitialize).
- `gp.UI.HUDStatus` prints LocalPlayer, configured class, instance, widget class/name, and
  ViewModel Ready for operator validation. It does not bypass bootstrap.
- Operator PIE validation passed for the live push path: initial dump Ready/TeamId=1/zero resources;
  after live play, OrbitalFerronite=100, FerroniteScore=100, FerroniteThreatValue=250.
  Follow-up PIE validation of runtime bootstrap: authored `WBP_GP_HUD` appeared automatically;
  `gp.UI.HUDStatus` ConfiguredClass=`WBP_GP_HUD_C`, InstancePresent=true, Ready=Ready;
  `gp.UI.HUDDump` PlanetFerronite / OrbitalFerronite=100.00; Manual MVVM
  `GP_ResourceViewModel.OrbitalFerronite` → To Text (Float) → `TXT_OrbitalFerroniteValue.Text`
  updated in the visible HUD.
- The TEMP HUD is retired. Production HUD is the active match HUD and remains **PARTIAL**. Native
  right-side Launch Menu presentation exists (`UGP_LaunchMenuPresenter` + HUD-root accessors);
  authored WBP layout is operator-local. Still not implemented: visible resource/timer HUD
  completeness, Selection UI, Context Action Grid, MainBase PURCHASE panel, minimap function,
  notifications, and production end-of-match screen.
  Operator-validated runtime visibility of authored `WBP_GP_HUD` (local, not committed).
  Operator-validated: `GP_ResourceViewModel.PlanetFerronite` → To Text (Float) →
  `TXT_PlanetFerroniteValue.Text` updates live when Workers deposit Ferronite.
  `WBP_GP_HUD` remains operator-local and is not committed.
- **Approved visual IA (2026-08-21):** two bars × three blocks, plus MainBase PURCHASE inside the
  bottom-right panel. See
  [`GP-Production-HUD-Layout-Spec`](../Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md).
  Global `O` Order Menu is **SUPERSEDED** as the production HUD path. TEMP HUD procurement is
  retired. Future production context-action UI will call existing PlayerController gameplay request APIs. Backend orbital flows are unchanged.

### MVVM Data Flow

```
Server-authoritative state                 Replication                ViewModel             Widget (View)
────────────────────────────               ──────────────             ─────────             ─────────────
UGP_PlayerAttributeSet.OrbitalFerronite ─► GAS attribute repl   ─►   UGP_ResourceViewModel ◄─  future resource widget
UGP_SelectionComponent (local)       ─►    OnSelectionChanged    ─►   UGP_SelectionVM      ◄─►  future Selection/Info + Action Grid
AGP_GameState.MatchTimeRemaining     ─►    RepNotify/delegate    ─►   UGP_MatchViewModel    ◄─  future match widget
UGP_OrbitalDeliverySubsystem.Catalog ─►    OnRep / delegate      ─►   future procurement VM  ◄─►  MainBase PURCHASE panel (bottom-right)
```

> Post-pivot (ADR-0009 + 2026-08-08 refinement): **no Build / Production queue UI**. Ordering surfaces:
> - **Unit Order** — manifest builder (slots / costs) → DropPod → MainBase Unit Drop Zone (no world reticle for normal units).
> - **Building Order** — Purchase → READY list → Deploy ghost (placement) → DropPod (no second spend).
> Shared DropPod presentation. Future production context-action UI may host both panels. TEMP HUD is retired. Pre-pivot Production/Construction/BuildMenu VMs superseded.

Rules:

1. **VM populated by VM-owner adapter in local-player presentation lifetime.** Production Resource/Match
   VMs and adapters are owned by `UGP_HUDViewModelSubsystem`; specialized FoW presentation retains its
   existing ownership. Each adapter:
   - Subscribes до replicated state / attribute / delegate.
   - Translates state → VM `Set*` methods (broadcasts `OnPropertyChanged`).
   - Owns lifetime + unbind on EndPlay.
2. **Widget binds to VM only.** Widget gets VM reference via Common UI’s `UMVVMSubsystem` (`UMVVMSubsystem::GetViewModel(...)`). Per-widget BlueprintGraph або C++ uses `BindFieldValueChanged` to react.
3. **Widget never reads ASC / actor state directly.** Прямі `GetAttributeBase` / `FindComponentByClass` у widget code = review-blocking violation.
4. **Widget input → PC.** Button click → widget calls injected PC handler (Common UI delegate) → PC sends RPC → server processes → state changes → replicates → adapter updates VM → widget rerenders. Closed loop, no shortcuts.
5. **No reverse write.** Widgets never write VM fields. VMs мутую тільки adapter (server-side trigger).
6. **VM scope:** local-only per PC. VMs themselves not replicated — їхній underlying state — replicates per existing TDD/03 rules.

### ViewModel Inventory (MVP)

| ViewModel | Source state | Owner adapter | Widget consumer |
| --- | --- | --- | --- |
| `UGP_ResourceViewModel` | `UGP_PlayerAttributeSet.{OrbitalFerronite, FerroniteScore, MaxUnits, CurrentUnits}` (own + opponent score) plus exact local MainBase `UGP_StorageComponent::GetTotalStored()` as `PlanetFerronite` | `UGP_ResourceViewModelAdapter` (`UGP_HUDViewModelSubsystem`) | Future top-right Planet/Orbit/Cap + top-left Score |
| `UGP_MatchViewModel` | `AGP_GameState.{MatchStateTag, MatchTimeRemaining, TeamFerroniteThreatValues, WinnerTeamId, WinReasonTag, MatchResult.MatchDuration}` plus presentation `FerroniteThreatNormalized` from local MainBase storage | `UGP_MatchViewModelAdapter` (`UGP_HUDViewModelSubsystem`) | Future top-center timer + top-left Threat bar |
| `UGP_SelectionVM` | `UGP_SelectionComponent.{SelectedUnits, InspectedTarget}` (local PC) — **not implemented** | Future adapter | Future bottom-center Selection/Info + bottom-right Context Action Grid |
| `UGP_OrderMenuVM` | `UGP_OrbitalDeliverySubsystem` drop catalog (`DA_GP_OrbitalDrop_*`), current `OrbitalFerronite`, current `CurrentUnits/MaxUnits`, shuttle slots, READY, Wall stock — **not implemented** | Future adapter | Future MainBase PURCHASE panel (bottom-right). Not a fullscreen Order Menu. |
| `UGP_CargoVM` | `UGP_CargoComponent.CurrentCargo` of single-selected worker — **not implemented** | Future adapter | Future single-entity Selection/Info |
| `UGP_NotificationVM` | Local notification queue (PC pushes) — **not implemented** | PC native | Future notification stack |
| `UGP_MinimapVM` | `UGP_MinimapSubsystem` snapshot — **not implemented** | Subsystem self | Future bottom-left minimap (layout currently reserves a square placeholder only) |

Production Resource/Match VMs are created once by `UGP_HUDViewModelSubsystem` per `ULocalPlayer`.
Future widgets receive them from that subsystem; no gameplay module type owns a `GPUIRuntime` object.

### Input Routing — Common UI

- Project Settings → CommonUI Input Routing enabled.
- `AGP_PlayerController` extends project `APlayerController` (not Lyra `CommonPlayerController`) and integrates Common UI action routing (`UCommonUIActionRouterBase` / LocalPlayer Common UI services) without `CommonGame`.
- Action sets: `CommonUI.Default`, `CommonUI.EndOfMatch` — switch on activatable widget activation.
  Production MainBase procurement is an **in-panel HUD state**, not a fullscreen activatable Order Menu.
  Historical `CommonUI.OrderMenu` naming is superseded for the production HUD path.
- Gameplay IMC (`IMC_GP_Camera`, `IMC_GP_Selection`, `IMC_GP_Commands`) via Enhanced Input — gated by `IsMatchInput` predicate; suspended коли activatable modal у focus.

## Module Ownership

UI код live у `GPUIRuntime`. Per [`01_Module_Architecture`](01_Module_Architecture.md):

- Native UMG widget classes (`UGP_UserWidgetBase : UCommonUserWidget`, screens, HUD root) — у `GPUIRuntime`.
- ViewModel classes (`UGP_*VM : UMVVMViewModelBase`) — у `GPUIRuntime` (за convention з namespace `/Public/ViewModels/`).
- VM adapters — у `GPUIRuntime` (UObject helpers, ideally а `UGP_PlayerController` Subsystem за PC scope).
- Blueprint UI children (`WBP_GP_*`) — у `Content/GrimProtocol/UI/`.
- UI читає state виключно через ViewModels. **UI не пише** у gameplay state.

## Hard Rules

1. **UI не власник gameplay state.** Replicated attributes, gameplay tags, RPC results — лише через ViewModels.
2. **Жодних tick-poll.** ViewModels оновлюються push-based (`OnAttributeChanged`, `RepNotify`, multicast delegates → adapter → VM `Set*`).
3. **UI не виконує gameplay authority.** Click on button → широка через PC RPC. Widget сам не змінює state, не пише у VM, не виконує gameplay logic.
4. **Selection — local-only.** Adapter читає `UGP_SelectionComponent` напряму (local PC owns), оновлює `UGP_SelectionVM`.
5. **Cosmetic feedback ≠ gameplay truth.** UI може предіктивно показати pulse decal / sound, але authority — server.
6. **Widget never queries ASC / Actor state.** Direct reads of attributes, components, transforms — banned. Все через VM.
7. **Common UI activation stack** для modal screens (EndOfMatch, Pause). Не raw `AddToViewport` для screens.
   Production MainBase PURCHASE is in-panel HUD, not a modal Order Menu.

Specialized exception: FoW world presentation is a native local-player adapter, not an interactive HUD
screen. `UGP_FoWWorldPresentationSubsystem` may read the trusted local mirror to rebuild viewport-local
per-cell feathered quads, but cannot read authority or mutate gameplay. Camera motion rebuilds those
tiles. The paired `UGP_LocalFoWUnitPresentationSubsystem` is likewise a native world-presentation
adapter: UnitBase actors lifecycle-register, LocalFoW revisions push immediate reevaluation, and a
bounded 10 Hz registered-list pass catches movement across a static visibility edge. It only composes
local primitive/health/combat presentation and never changes actor replication or gameplay state.

## Detailed MVP HUD Rules (GP-0401)

Stage — design only. Поверх existing widget naming у [`GDD/09_UI_UX`](../GDD/09_UI_UX.md). Цей розділ formalizes state binding, ownership і read-only contracts.

### HUD State Inventory

| Display | Source | Sync | Replication |
| --- | --- | --- | --- |
| Match timer | `AGP_GameState.MatchTimeRemaining` | `OnRep_MatchTimeRemaining` (1 s push from server) | All clients |
| Match state (Lobby / Playing / Finished) | `AGP_GameState.MatchState` (enum / tag) | `OnRep_MatchState` | All clients |
| Own Orbital Ferronite (spendable) | `UGP_PlayerAttributeSet.OrbitalFerronite` (own ASC) | `OnAttributeChanged` delegate | `COND_OwnerOnly` |
| Own score | `UGP_PlayerAttributeSet.FerroniteScore` (own ASC) | `OnAttributeChanged` | All clients |
| Opponent score | `UGP_PlayerAttributeSet.FerroniteScore` (remote PlayerState.ASC lookup) | `OnAttributeChanged` on remote ASC | All clients |
| Current unit count | `UGP_PlayerAttributeSet.CurrentUnits` (own ASC) | `OnAttributeChanged` | `COND_OwnerOnly` |
| Max unit cap | `UGP_PlayerAttributeSet.MaxUnits` (own ASC) | `OnAttributeChanged` | `COND_OwnerOnly` |
| Planet Ferronite (exact stored amount) | `UGP_ResourceViewModel.PlanetFerronite` = local MainBase `UGP_StorageComponent::GetTotalStored()` | `OnResolvedMainBaseChanged` + `OnStorageChanged` | Owner / FoW-gated detail |
| SWARM / Ferronite Threat (pressure) | `AGP_GameState` per-team `FerroniteThreatValue` | `OnTeamFerroniteThreatValueChanged` | All clients |
| Selected entities | `UGP_SelectionComponent.SelectedUnits` (local PC) | `OnSelectionChanged` delegate | **Local only — no replication** |
| Inspected target | `UGP_SelectionComponent.InspectedTarget` (local PC) | Same delegate | Local only |
| Inspected target HP | `UGP_UnitAttributeSet.Health` on target ASC | Standard GAS replication | Per ASC settings (Mixed) |
| Selected unit current command | Unit's component state (e.g., `MiningComponent.State`, `CombatComponent.HasEngagement`) | Component dispatcher / OnRep | Mixed (per component) |
| Active in-flight drop pods | `UGP_OrbitalDeliverySubsystem` active-pod count (per team) | `OnActivePodCountChanged` delegate | Owner (HUD pod counter) |
| Container fill / launch state | `UGP_StorageComponent.Containers[i]` (state + volume) on MainBase | `OnRep_Containers` | All clients (FoW-gated detail) |
| Worker cargo bar | `UGP_CargoComponent.CurrentCargo` (worker) | `OnRep_CurrentCargo` | All clients (for "carry over head" indicator) |
| Order menu items | `UGP_OrbitalDeliverySubsystem` drop catalog (`DA_GP_OrbitalDrop_*`, read via PC) | Static (set at match start) | N/A |
| Command bar items | `SelectedUnits[i].UnitDefinition.AllowedCommands` | On `OnSelectionChanged` | N/A |
| Drop-target reticle / building ghost | Local **building deploy** ghost (units use Unit Drop Zone — no free placement reticle) | N/A | Local only |
| Predictive command pulse decal | Local PC spawn | N/A | Local only |
| Rejected command red pulse | `Client_NotifyCommandRejected` RPC | One-shot | RPC-driven |
| Idle worker alert | `UGP_MiningComponent::OnIdleWithNoDeposit` delegate per worker | Delegate | Local only |
| Minimap fog of war | `UGP_FogOfWarComponent` team visibility (3-level) | `OnRep_FoWState` / fog texture update | Per-team (FoW is in MVP — see [`15_Fog_of_War`](15_Fog_of_War.md)) |
| Minimap dots | Replicated unit positions / building positions | Standard transform replication | All clients |
| Minimap camera viewport | Local camera transform | Local | Local only |
| Match end screen | `AGP_GameState.MatchState == Finished` + `WinnerTeamId` | `OnRep_MatchState` | All clients |

**Hard rule:** жодне поле з цієї таблиці не записується із UI. UI binds → reads → renders.

### Widget Hierarchy

Canonical in-match HUD is two bars × three blocks. Old hierarchy (resource/score top-right,
selection bottom-left, command bar bottom-center, minimap top-right) is **SUPERSEDED**.

```
WBP_GP_HUD (authored child of UGP_HUDRootWidget; operator-local, not committed)
├── TopBar
│   ├── TopLeft  Threat + Player Ferronite Score
│   ├── TopCenter Match Timer
│   └── TopRight Planet Ferronite / Orbital Ferronite / CurrentUnits/MaxUnits
├── BottomBar
│   ├── BottomLeft  Minimap square placeholder (function later)
│   ├── BottomCenter Selection / Current Info (widest; single-entity or 10×3 group)
│   └── BottomRight Context Action Grid + Message Strip
│       (Unit / Building / MainBase PURCHASE → UNITS|BUILDINGS|DEFENSE)
├── RightLaunchMenu (authored on WBP_GP_HUD; operator-local, not committed)
│   ├── Launch button (top)
│   └── Vertical container list (one fill bar per local MainBase container)
├── DropReticle / building ghost (visual layer)
├── NotificationStack (not implemented; Message Strip is panel-local)
└── EndOfMatch (hidden until match end)
```

A fullscreen / modal Order Menu is **not** part of the production HUD. TEMP HUD procurement
is retired; future production context-action UI will call existing PlayerController gameplay request APIs. Native production HUD root class is `UGP_HUDRootWidget : UGP_UserWidgetBase`.
Authored `WBP_GP_HUD` is created at runtime by `UGP_HUDViewModelSubsystem` from
`UGP_UIPresentationSettings::ProductionHUDWidgetClass` (operator-local asset, not committed).
The widget is not fully wired; remaining top/bottom fields and actions still need authored
bindings and layout work.

Visual prototype contract: medium/dark grey major blocks, lighter grey inner cells, thin borders,
modest rounding, stronger contrast for selected/hover. No final art/textures/icons required.

Planet Ferronite and Threat currently present the **same underlying stored-Ferronite source**
(pressure vs exact number). Do not invent a second currency. Threat bar fill uses
`FerroniteThreatNormalized` derived from actual MainBase storage capacity × ThreatPerStoredUnit;
this is presentation normalization, not a gameplay threshold. ResourceVM exposes exact
`PlanetFerronite` from local MainBase `UGP_StorageComponent::GetTotalStored()` (event-driven via
MainBase resolve + storage change). It is not reconstructed from Threat. Operator-validated:
authored `TXT_PlanetFerroniteValue` is bound to `GP_ResourceViewModel.PlanetFerronite` through
To Text (Float) and updates when Workers deposit Ferronite. `WBP_GP_HUD` remains operator-local
and uncommitted.

**Right-side Launch Menu (production HUD, 2026-08-23):** a vertical panel on the right of
`WBP_GP_HUD`. Top control is Launch. Below it, one row per local MainBase storage container
with a fill bar: yellow while filling, green when full/ready. Presentation is owned by
`UGP_LaunchMenuPresenter` on `UGP_HUDViewModelSubsystem` (LocalPlayer). `UGP_HUDRootWidget`
exposes read-only `GetLaunchContainerRows` / `GetLaunchContainerPresentations` /
`CanLaunchReadyContainer` / `GetReadyLaunchContainerCount`, forwards Launch through
`AGP_PlayerController::RequestLaunchReadyContainer` → `Server_RequestLaunchReadyContainer`,
and notifies authored WBP via `BP_OnLaunchMenuChanged`. Source of truth remains
`UGP_StorageComponent` on the resolved local MainBase. Event-driven via
`OnResolvedMainBaseChanged` + `OnStorageChanged`. No Tick, no world scan, no TEMP HUD.
Authored right-side layout stays operator-local; this slice does not modify `GP/Content`.
Production HUD root visibility is `SelfHitTestInvisible` so empty HUD chrome does not eat
pointer hits while `BTN_Launch` and future procurement controls remain clickable. Operator
found Launch OnClicked dead under `HitTestInvisible`; that was a root hit-test bug, not a
gameplay launch-path bug. No global input-mode change.
Operator PIE validation **PASSED** (2026-08-23): TEMP HUD retired; Production HUD active;
right-side Launch menu rows/fill/colors/enablement/click/launch-gameplay all live-correct.
Authored `WBP_GP_HUD` / `WBP_GP_LaunchContainerRow` remain operator-local and uncommitted.

### MVVM Binding Contract

Per widget — bind ViewModel via `UMVVMSubsystem`. Adapter populates VM.

| Widget | ViewModel (FieldNotify props) | Adapter subscribes to |
| --- | --- | --- |
| Future top-center Match Timer | `UGP_MatchViewModel.MatchTimeRemaining` | `AGP_GameState.OnMatchTimeRemainingChanged` |
| Right-side Launch Menu | `UGP_HUDRootWidget` accessors over `UGP_LaunchMenuPresenter` rows (`FillNormalized`, `bIsReadyForLaunch`); Launch via `RequestLaunchReadyContainer` | Local MainBase `OnResolvedMainBaseChanged` + `UGP_StorageComponent::OnStorageChanged` |
| Future top-left Threat | `UGP_MatchViewModel.FerroniteThreatNormalized` (bar); raw `FerroniteThreatValue` remains available | Local-team `OnTeamFerroniteThreatValueChanged`, `OnResolvedMainBaseChanged`, MainBase `OnStorageChanged` |
| Future top-left Score + top-right Orbit/Cap | `UGP_ResourceViewModel.{PlanetFerronite, OrbitalFerronite, FerroniteScore, CurrentUnits, MaxUnits}` | Own ASC attribute-change delegates; local MainBase `OnResolvedMainBaseChanged` + `OnStorageChanged` for `PlanetFerronite` |
| Future top-right Planet Ferronite | `UGP_ResourceViewModel.PlanetFerronite` = local MainBase `GetTotalStored()` | `FindMainBaseForTeamClientSafe` + `OnResolvedMainBaseChanged` + `OnStorageChanged` |
| Future bottom-center Selection/Info | Future `UGP_SelectionVM` single-entity vs 10×3 group | `UGP_SelectionComponent.OnSelectionChanged` (local) |
| Future bottom-right Context Action Grid | Future selection/command + MainBase procurement presentation | Same selection delegate; Unit vs Building vs PURCHASE states |
| Future right-side Message Strip | Contextual procurement/action status (shuttle slots, funds, cap, wall stock) | Existing orbital reject/status; not a global toast stack |
| Future MainBase PURCHASE panel | Future procurement VM (catalog, manifest, READY, Wall stock) | `UGP_OrbitalDeliverySubsystem` + existing Purchase/Confirm/Deploy RPCs |
| Future bottom-left Minimap | Future `UGP_MinimapVM` — layout placeholder only in the next visual slice | Later minimap subsystem |
| Future `WBP_GP_HUD_NotificationStack` | `UGP_NotificationVM.{ActiveToasts[]}` | PC `OnHUDNotification` multicast |
| Future `WBP_GP_EndOfMatch` (Activatable) | `UGP_MatchViewModel.{MatchStateTag, WinnerTeamId, WinReasonTag, MatchDuration, bMatchFinished}` | `AGP_GameState` match-state/result delegates |

**Historical abbreviated FieldNotify example (implemented production names are listed above):**

```cpp
UCLASS()
class GPUIRUNTIME_API UGP_ResourceVM : public UMVVMViewModelBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)  float Ferronite;
    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)  float OwnScore;
    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)  float OpponentScore;
    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)  int32 CurrentUnits;
    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)  int32 MaxUnits;

    // Setters called by adapter; broadcast PropertyChanged
    void SetFerronite(float Value);
    // ...
};
```

**Anti-patterns (review-blocking):**

- `Tick`-based polling у widget (per UE) — instead, subscribe via VM `OnPropertyChanged`.
- Widget code calling `Actor->FindComponentByClass<...>()` — banned. Adapter does it; widget reads VM.
- Widget code calling `ASC->GetNumericAttribute(...)` — banned. Adapter mirrors to VM.
- Direct widget→server RPC calls — banned. Use PC handler injected via Common UI delegate.
- `AddToViewport` для modal screens — banned. Use `UCommonActivatableWidgetStack::AddWidget`.

### Selection / Information Routing Rules

Bottom-center Selection / Current Info:

1. Empty selection → hide info content; Action Grid idle / no unit commands.
2. Exactly one unit **or** one building → **single-entity mode** (icon, name, health, relevant stats).
   Inspect of a single target uses this same block; a separate overlapping InspectPanel is not canonical.
3. Multiple units → **group mode**: 10 icons per row, 3 rows, 30 visible slots. Each icon has a small
   health bar below it. Overflow/paging/aggregation beyond 30 is **TBD / UX DESIGN REQUIRED**.
   Do not silently cap gameplay selection to 30.
4. Mixed unit+building selection remains forbidden per GP-0202.

### Context Action Grid Population

Bottom-right panel. Not a Build Menu. Not orbital production.

**Unit Action Mode** (one unit or a unit group):

| Cell | Command | Status |
| --- | --- | --- |
| 1 | Move — choose destination | Existing command |
| 2 | Stop — halt current command | Existing command |
| 3 | Attack-Move — move toward point while engaging ("идти с атакой") | Existing `GP.Command.AttackMove` |
| 4 | Patrol — current/assigned point to target and back | **PLANNED / DESIGN TARGET**, not implemented |

Direct RMB target Attack remains a separate contextual gameplay behavior. Do not rename it Attack-Move.

Additional granted commands (Mine, Repair, future unit abilities) may occupy extra cells when the
selection actually grants them. Do not fully design ability slots yet.

**Building Action Mode** (one building selected): same panel switches to building actions.
Only **MainBase** owns orbital **PURCHASE**. Other buildings: contextual actions only; MVP may
still have no functional actions. Do not invent upgrades. Do not treat as local production.

**MainBase PURCHASE** (design / not implemented): replace grid with UNITS / BUILDINGS / DEFENSE
inside this panel. Message Strip sits above it. Bottom-center stays on MainBase info.
LAUNCH uses existing unit Confirm / building Purchase→READY→ghost / Wall Package buy.
A global Order Menu is superseded for production HUD. See
[`GP-Production-HUD-Layout-Spec`](../Development/Claude_Tasks/GP-Production-HUD-Layout-Spec.md).

Multi-select group: intersection of AllowedCommands across selected units.

### Match-State HUD Gating

| MatchState tag | HUD state |
| --- | --- |
| `GP.Match.State.Loading` | Splash overlay; HUD hidden. |
| `GP.Match.State.WaitingForPlayers` | Lobby overlay; HUD hidden. |
| `GP.Match.State.Playing` | Full HUD. |
| `GP.Match.State.Paused` (singleplayer only) | HUD visible + pause overlay. |
| `GP.Match.State.Finished` | HUD ghosted (read-only); EndOfMatch overlay. |

PC blocks input where appropriate (during Loading / WaitingForPlayers / Finished) via `IMC_GP_Camera`, `IMC_GP_Selection`, `IMC_GP_Commands` removal.

### Notification Stack — Notification Types (MVP)

| Tag | Trigger | Severity |
| --- | --- | --- |
| `GP.Notify.InsufficientOrbitalFerronite` | `GE_GP_SpendOrbital` fails / orbital drop order rejected (`EReason::InsufficientOrbital`) | Warning |
| `GP.Notify.UnitCapReached` | Orbital order rejected on `CurrentUnits >= MaxUnits` | Warning |
| `GP.Notify.StorageFull` | All MainBase containers Ready/Launching — Worker drop-off has nowhere to go | Warning |
| `GP.Notify.WorkerIdle` | `UGP_MiningComponent::OnIdleWithNoDeposit` | Info |
| `GP.Notify.BaseUnderAttack` | MainBase / LogisticsHub takes damage while not engaged within 10 s | Critical |
| `GP.Notify.MatchEndingSoon` | `MatchTimeRemaining <= 60 s` | Info (flash) |
| `GP.Notify.CommandRejected` | `Client_NotifyCommandRejected` | Warning (per-instance; deduplicate within 0.5 s) |

NotificationStack — local-only, no replication, queue cap 3 active. Toast duration via DataAsset (`DA_GP_NotificationConfig`) per tag.

### Local-Only State (HUD-owned)

UI legitimately owns:

- Camera transform mirror for minimap viewport rectangle.
- Modal mode flags: `bAttackModeActive` (A pressed), `bBuildModeActive` (B menu open), `bGhostActive` (ghost placement live).
- Notification queue.
- Settings panel state.
- Cached minimap snapshot.

Це не gameplay state, тому owned by UI безпечно.

### Replication Conditions Summary

| Attribute / Field | Condition | Reason |
| --- | --- | --- |
| `OrbitalFerronite`, `MaxUnits`, `CurrentUnits` | `COND_OwnerOnly` | Private resource / currency info; opponent не бачить твій spendable pool. |
| `FerroniteScore` | All clients (`COND_None`) | Score / delivery-quota race per GDD/08. |
| `FerroniteThreatValue` | All clients | Shared world state (swarm pressure). |
| `MatchTimeRemaining`, `MatchState` | All clients | Universal. |

### Minimap (layout reserved; function later)

- Bottom-left **square placeholder** is part of the approved HUD IA.
- Minimap function, FoW minimap layers, and click-to-pan are **not** part of the next visual HUD slice.
- Later: subsystem snapshot, camera viewport rectangle, LMB focus via `AGP_CameraPawn::FocusOnLocation`.

### Validation Checklist (Stop Condition)

- [x] UI does not own gameplay state — table above lists external sources, all reads go through VM; no widget writes.
- [x] UI shows required MVP decisions — resource pool, score, opponent score, unit cap, selection, commands, match timer, SWARM, minimap, match end, notifications.
- [x] UI avoids non-MVP panels — chat, settings, replay, social, tech tree — listed у §Out of MVP.
- [x] Selection state local-only, replicated state read-only confirmed.
- [x] Framework rules: Common UI + MVVM mandatory; server updates VMs only; widgets bind to VMs only.

### Open Questions

1. **Minimap snapshot rate:** 5 Hz placeholder. May need 10 Hz for unit tracking responsiveness. Defer to playtest.
2. **End-of-match camera:** cinematic blend to winner's MainBase or freeze on death? Reserve `AGP_CinematicCameraPawn` per GP-0201 Open Q #5.
3. **Notification deduplication window:** 0.5 s placeholder. DA-driven per tag.
4. **HUD opacity / safe zone:** TBD у visual / UX pass (GP-0701).
5. **Localization:** all `FText` через project string table. Setup deferred GP-0701.
6. **Accessibility:** color-blind palettes, larger fonts, HUD scale — GP-0701.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Empty selection | Bottom-center info empty; Action Grid idle. |
| 2 | Single worker selected | Single-entity mode; Action Grid in Unit Action Mode. |
| 3 | 24 workers | Group 10×3 grid with per-icon health bars; 24 of 30 slots filled. |
| 4 | Building selected | Single-entity building stats; Action Grid in Building Action Mode (may be empty). |
| 5 | Inspect enemy | Bottom-center shows that entity; no separate overlapping inspect slot. |
| 6 | Esc clear | Info and Action Grid return to empty/idle. |
| 7 | Resource readout flash | Drop-off → score `+50` flash animation, pool unchanged. |
| 8 | Opponent score | Opponent drop-off → opponent score updates within ≤ 1 s. |
| 9 | Cap reached | Production at max → toast "Unit cap reached". |
| 10 | Insufficient Ferronite | Attempt build з не вистачає коштів → toast + button denied. |
| 11 | Worker idle | Last deposit dies → toast "Idle worker". |
| 12 | Match timer last 60 s | Timer turns yellow; under 15 s — red. |
| 13 | Match end | Timer expires → EndOfMatch overlay; HUD inert. |
| 14 | Minimap click-to-pan | Click minimap → camera focuses world location. |
| 15 | Construction progress | Build site progress bar updates synchronously across clients. |
| 16 | Production queue replication | Build queue 5 workers → all 5 slots replicate to owner; opponent doesn't see queue. |
| 17 | No tick-poll | Profile session — no per-frame attribute queries у UI. All updates delegate-driven. |

### Out of MVP (HUD)

- Chat overlay.
- Replay HUD.
- Settings panel beyond minimal stub.
- Social / friend invites.
- Spectator HUD.
- Tutorial tooltips.
- Tech tree visualization.
- Localization runtime.
- Accessibility options panel.
- Custom HUD themes.

### MVVM Style Examples

Selection adapter — minimal example:

```cpp
void UGP_SelectionVMAdapter::Initialize(AGP_PlayerController* PC)
{
    OwnerPC = PC;
    if (UGP_SelectionComponent* Sel = PC->GetSelectionComponent())
    {
        Sel->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleSelectionChanged);
    }
    SelectionVM = NewObject<UGP_SelectionVM>(this);
    UMVVMSubsystem::Get(PC->GetLocalPlayer())->RegisterViewModel(SelectionVM, "Selection");
}

void UGP_SelectionVMAdapter::HandleSelectionChanged()
{
    const auto& Selected = OwnerPC->GetSelectionComponent()->GetSelectedUnits();
    SelectionVM->SetMode(ResolveModeForSelection(Selected));
    SelectionVM->SetSelectedUnitVMs(BuildUnitVMs(Selected));
    SelectionVM->SetAvailableCommandTags(IntersectAllowedCommands(Selected));
    // FieldNotify auto-broadcasts; widget rerenders.
}
```

Widget-side (BP or C++) binds через `MVVM View Binding`: future Selection/Info listens to
`SelectionVM` mode → single-entity vs 10×3 group; Context Action Grid listens to unit vs building mode.

Abbreviated adapter pattern (the implementation is `UGP_ResourceViewModelAdapter`):

```cpp
void UGP_ResourceVMAdapter::BindAttribute(UAbilitySystemComponent* ASC, FGameplayAttribute Attr)
{
    ASC->GetGameplayAttributeValueChangeDelegate(Attr).AddUObject(this, &ThisClass::OnAttributeChanged);
}

void UGP_ResourceVMAdapter::OnAttributeChanged(const FOnAttributeChangeData& Data)
{
    if (Data.Attribute == UGP_PlayerAttributeSet::GetFerroniteAttribute())
        ResourceVM->SetFerronite(Data.NewValue);
    else if (Data.Attribute == UGP_PlayerAttributeSet::GetFerroniteScoreAttribute())
        ResourceVM->SetOwnScore(Data.NewValue);
    // etc.
}
```

Widget never knows про ASC. Adapter is the only translation point.

## Detailed Feedback Matrix (GP-0402)

Stage — design only (per [`Claude_Tasks/GP-0402_Feedback_Pass`](../Development/Claude_Tasks/GP-0402_Feedback_Pass.md)). Це consolidates feedback rules з GP-0202/0203/0204/0301/0302/0303/0304 у single matrix.

### Hard Rules

1. **Significant action ⇒ ≥ 2 feedback channels** (visual + audio default; UI third де practical).
2. **Cosmetic ≠ truth.** Cosmetic feedback (decals, sounds, animations) — clientside / multicast unreliable. Не gameplay authority. Real state always — server-replicated attribute / tag.
3. **Cosmetic може випереджати truth** (predictive — pulse decal on RMB click). Cosmetic може помилятися (server rejects command — show red pulse). Truth wins завжди.
4. **Multicast cosmetic** — `NetMulticast Unreliable`. Не RPC-spam channel.
5. **Audio attenuation** — 3D audio для positional events (attack, drop-off, build complete); 2D для player-private feedback (Insufficient Ferronite, Cap Reached).
6. **VFX budget** — per-event impact emitter ≤ 2 active per actor; LOD via `Niagara LOD` or distance scaling.
7. **UI feedback (Notifications)** — push-based через `UGP_NotificationVM` (per §MVP HUD). Не tick-poll. Deduplicate within 0.5 s.
8. **Replication discipline.** Cosmetic-only events — server multicast unreliable. Game-state events — already replicate via GAS / RepNotify; client picks them up і fires local feedback through ViewModel changes.

### Channels Glossary

| Channel | Definition | Owner |
| --- | --- | --- |
| **V-Decal** | Ground decal spawned at world location, fades over time | Local PC (predictive) OR `NetMulticast Unreliable` |
| **V-VFX** | Niagara particle effect attached to actor / world | Multicast unreliable (replicated event) |
| **V-Mat** | Material parameter change на actor (highlight, damage flash, build glow) | Adapter sets VM, MID instance reacts on widget OR per-actor visual |
| **V-Mesh** | Mesh swap / hide / show (e.g., destruction rubble) | Replicated via standard actor lifecycle |
| **V-UI** | HUD widget update via ViewModel (number flash, color shift, panel toggle) | VM `Set*()` from adapter |
| **A-3D** | Positional 3D sound attached to source actor | `UGameplayStatics::SpawnSoundAtLocation` from multicast |
| **A-2D** | Non-positional sound (HUD click, notification chime) | Local PC trigger |
| **HUD-Toast** | `UGP_NotificationVM` push toast | PC `OnHUDNotification` multicast delegate (local) |
| **HUD-Flash** | VM property change with brief tween (e.g., `ScoreDelta` flash) | Adapter sets value with timestamp; widget animates |
| **HUD-Indicator** | Persistent VM-bound indicator (idle worker icon, cap reached red) | VM boolean / count |

### Action Matrix

| Action / Event | V-Decal | V-VFX | V-Mat | V-Mesh | V-UI | A-3D | A-2D | HUD-Toast | HUD-Flash | HUD-Indicator | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Selection** (add unit) | — | — | EmissiveBoost on unit | — | Selection/Info rebuild | — | Click | — | — | — | Local only; per GP-0202. |
| **Inspect enemy** | — | — | InspectBoost on target | — | Selection/Info shows that entity | — | Soft click | — | — | — | Local only. |
| **Marquee** | LMB drag rectangle | — | — | — | — | — | — | — | — | — | Local only. |
| **Move command issued** | Green pulse at click | — | SelectionRing flash 200 ms | — | — | — | CommandIssued SFX | — | — | — | Predictive cosmetic. Per GP-0203. |
| **Move command rejected** | Red pulse at click | — | — | — | — | — | CommandDenied SFX | Optional toast | — | — | `Client_NotifyCommandRejected` triggers. |
| **Unit starts moving** | — | — | — | — | — | EngineLoop start | — | — | — | — | Standard movement audio. |
| **Attack — fire** | — | MuzzleFlash + impact at target | — | — | — | WeaponFire 3D | — | — | — | — | Multicast unreliable; per GP-0204. |
| **Attack — hit damage** | — | HitImpact (sparks) at target | DamageFlash 100 ms on target | — | — | HitImpact 3D | — | — | — | — | Multicast on `OnHealthChanged` delta < 0. |
| **Attack — target killed** | — | DeathExplosion | — | RubbleMesh (delayed 0.5 s) | — | DeathSFX 3D | — | — | — | — | Standard `OnUnitDied`. |
| **Cooldown blocking attack** | — | — | — | — | — | — | — | — | — | Cooldown overlay on Attack-Move / attack-related Action Grid cell | VM read from `AttackCooldown` tag. |
| **Mining tick** | — | DrillSparks at deposit | — | — | — | DrillLoop 3D (looping) | — | — | — | — | Multicast unreliable, 1 Hz event. |
| **Deposit depleted** | — | DepletionPuff | — | Destroy actor (per GP-0303) | — | DepletionFizzle 3D | — | — | — | — | Server triggers destroy + multicast cosmetic. |
| **Cargo full** | — | — | CargoGlowOn worker | — | CargoBar VM `IsFull=true` | — | — | — | — | — | Visual gate. |
| **Drop-off** | — | DropOffParticles at base | — | — | ResourceVM Ferronite flash, FerroniteScore +N flash | — | DropOffClang 3D | — | Score +N flash | — | Per GP-0301/0303. |
| **Insufficient Ferronite spend** | — | — | — | — | Button denied flash | — | DenyChime 2D | "Insufficient Ferronite" | — | — | Per GP-0301 production / GP-0302 build. |
| **Unit cap reached on production** | — | — | — | — | Button denied flash | — | DenyChime 2D | "Unit cap reached" | — | Red tint on cap readout while CurrentUnits ≥ MaxUnits | Per GP-0304. |
| **Production queued** | — | — | — | — | ProductionQueue VM slot added | — | QueueAdd 2D | — | — | — | Per GP-0301. |
| **Production complete (spawn)** | — | SpawnPortal 0.5 s | — | — | ProductionQueue VM slot consumed | SpawnSFX 3D | — | — | — | — | Multicast on spawn. |
| **Construction site placed** | — | PlacementPuff | — | Ghost → site mesh swap | — | PlacementThud 3D | — | — | — | — | Per GP-0302/0304. |
| **Construction tick** | — | WeldingArc per active builder | — | Progress mesh state swap (segmented mesh) | ConstructionVM Progress01 update | WeldingLoop 3D | — | — | — | — | Per GP-0304 multi-worker. |
| **Construction complete** | — | CompletionFlash | — | Site mesh → finished mesh | UnitCap flash on `MaxUnits +5` | CompletionChime 3D + 2D | — | — | Cap +5 flash | — | Per GP-0304. |
| **Construction canceled** | — | CancelDissipate | — | Site destroyed | Refund flash on Ferronite | CancelSFX 3D | — | — | Ferronite +refund flash | — | Per GP-0304. |
| **Repair tick** | — | RepairSparks on building | — | — | — | RepairLoop 3D | — | — | — | — | Per GP-0301. |
| **Repair complete** | — | — | — | — | — | RepairDone 3D | — | — | — | — | Per GP-0301; cancel on full HP. |
| **Building damaged (any)** | — | DamageImpact | DamageFlash on building | — | — | DamageImpact 3D | — | — | — | — | Standard. |
| **Building critical HP (<25%)** | — | SmokeEmitter (persistent) | — | DamageMesh state swap | — | LowHPGroan 3D | — | — | — | — | Multicast on threshold crossing. |
| **Building destroyed** | — | DestructionExplosion | — | RubbleMesh swap | — | DestructionBlast 3D | — | "Building lost" if own + critical (MainBase) | — | — | Standard. |
| **MainBase under attack** | — | — | — | — | — | — | AttackedAlert 2D (own only) | "Main Base under attack" | — | Red HUD edge flash 2 s | Cooldown 10 s to avoid spam. Per §Notification rules. |
| **Worker idle (no deposit)** | — | — | — | — | — | — | IdleChime 2D | "Worker idle" | — | IdleWorker indicator (count) | Per GP-0302 / GP-0303. |
| **Score increment** | — | — | — | — | ResourceVM Score `+N` flash | — | ScoreTick 2D (volume-scaled by N) | — | Score +N flash | — | Subtle audio, не loud spam. |
| **Opponent score increment** | — | — | — | — | OpponentScore flash | — | — | — | OpponentScore flash | — | Quiet, не distracting. |
| **SWARM aggression level shift** | — | — | — | — | SwarmAggression VM update | — | — | At threshold | Indicator color shift | — | Toast тільки на threshold cross (Moderate → High → Critical). |
| **SWARM wave incoming** | — | — | — | — | Pulse on aggression indicator | — | WarningKlaxon 2D | "SWARM incoming" | — | — | Critical severity. |
| **Match timer 60 s remaining** | — | — | — | — | Timer turns yellow | — | TimerWarning 2D | "1 minute remaining" | — | — | One-shot. |
| **Match timer 15 s remaining** | — | — | — | — | Timer turns red, pulses | — | TimerCrunch 2D | — | Timer pulse | — | Pulse animation. |
| **Match end — win** | — | — | — | — | EndOfMatch overlay (Activatable) | VictoryFanfare 3D ambient | VictoryChime 2D | — | — | — | One-shot, blocks input. |
| **Match end — loss** | — | — | — | — | EndOfMatch overlay | — | DefeatChime 2D | — | — | — | One-shot. |
| **Command rejected (general)** | Red pulse at click | — | — | — | — | — | DenyChime 2D | Per-reason toast | — | — | Per `EGP_CommandRejectReason` (GP-0203/0204). |
| **Rally point set (base selected)** | Small green flag at point | — | — | — | — | — | RallySet 2D | — | — | — | Decal persists while base selected. |
| **Build ghost — valid placement** | Ghost mesh green tint | — | — | — | — | — | — | — | — | — | Local only. |
| **Build ghost — invalid placement** | Ghost mesh red tint | — | — | — | — | — | — | — | — | — | Local only. |

### Cosmetic vs Truth — Separation Audit

Кожен row матриці classified:

- **Cosmetic-only:** Decals, VFX, audio, material flash. Could lag, drop, або не fire — gameplay не залежить.
- **Truth-bound:** Mesh swap on destroy, VM property change on attribute, score flash on `FerroniteScore` change. State-driven, не event-driven.

**Anti-pattern checklist:**

- ❌ Click LMB → directly write `SelectedUnits` (truth-bound, OK because selection IS local-only — per GP-0202).
- ❌ Click LMB → directly add cosmetic decal and use it as command resolution (cosmetic acting as truth — banned).
- ✅ Click LMB → fire RPC → server validates → adapter mirrors result to VM → widget rerenders. Cosmetic decal is a transient overlay.
- ❌ Multicast every damage tick (RPC-spam) — instead, OnHealthChanged delegate fires local damage flash, with optional multicast for VFX bursts.
- ✅ Damage VFX multicast unreliable on quanta thresholds (e.g., every 50 HP lost) for visual interest; per-tick damage shown through health bar via attribute change.

### Multiplayer Notes

- **Listen server:** host sees same cosmetic as clients. No host-favoring (per TDD/03).
- **Late join / reconnect:** out of MVP. Cosmetic state не resyncs — late client sees current state, не historical.
- **Multicast unreliable cap:** soft target ≤ 30 multicasts/sec per server (combat scenes). Если перевищується — bundle через server tick OR drop cosmetic gracefully.
- **Opponent VFX visibility:** gated by 3-level FoW (per [`15_Fog_of_War`](15_Fog_of_War.md)) — cosmetic events fire only for entities in a player's actively-visible cells; drop-pod descent is an intentional all-visible telegraph. Sound-cluttering risk applies within visible areas.
- **Sound priority:** UI / Notification > Player-action > World ambient > Opponent-action. Use `USoundClass` mixer per category для ducking.

### DataAsset Surface (Audio / VFX References)

Per Data-Driven First — assets refs у data:

- `DA_GP_FeedbackBundle_Worker.uasset` — collects всі VFX/SFX refs for Worker (MiningSparks, DropOffClang, RepairLoop, DeathSFX).
- `DA_GP_FeedbackBundle_Building_LogisticsHub.uasset` — building-specific feedback assets (Drop-pod landing / deploy, Repair, Destruction).
- `DA_GP_FeedbackBundle_Resource_Ferronite.uasset` — Mining VFX color, Drop-off color, Score audio tone.
- `DA_GP_FeedbackBundle_Command.uasset` — green / red pulse decals, command-issued / denied chimes.
- `DA_GP_NotificationConfig.uasset` — per-notification toast duration, icon, severity color (per HUD spec).

Native code references `TSoftObjectPtr` для async loading; bundle is the only source of asset refs. Tuning values (durations, colors) — fields на bundle DA.

### Validation Checklist (Stop Condition)

- [x] Significant action has readable feedback — кожне gameplay-relevant event у матриці має ≥ 2 channels.
- [x] Feedback does not become gameplay authority — Cosmetic vs Truth audit, multicast unreliable rules.
- [x] UI/VFX/audio requirements scoped for MVP — Out of MVP listed below.
- [x] Multiplayer feedback rules documented — listen-server, multicast unreliable, sound priority.

### Open Questions

1. **DA-driven sound mixer ducking** — auto-duck opponent SFX by 6 dB? Defer to audio pass.
2. **VFX LOD distance thresholds** — Niagara LOD per system; default Editor settings adequate? Profile у GP-0401A.
3. **Toast queue cap (3 active):** maybe expand to 4 для combat-heavy moments? Recommend keep 3.
4. **Replay-friendly cosmetic logs:** if replay у post-MVP, multicast cosmetics необхідно записувати? Out of MVP.
5. **Hearing impaired alternative:** visual indicator для всіх audio-only events? Reserve для accessibility pass (GP-0701).
6. **MainBase under attack дебоунс:** 10 s confirmed; if combat extends 30 s — repeat alert? Recommend re-fire every 30 s while continuous damage.

### Playtest Scenarios

| # | Scenario | Pass Criteria |
| --- | --- | --- |
| 1 | Issue Move on grass | Green pulse + click sound within 100 ms. |
| 2 | Issue Move on cliff | Red pulse + deny chime + optional toast. |
| 3 | Attack burst | Multiple V-VFX + 3D audio per shot, no audio clipping. |
| 4 | Unit death | Mesh swap, particle, sound — all fire within one frame on server. |
| 5 | Drop-off | Particles at base + clang + score flash + Ferronite delta replicated. |
| 6 | Insufficient Ferronite | Toast appears once, не spam per click. |
| 7 | Unit cap reached | Red tint on cap readout persists; toast on first attempt. |
| 8 | Worker idle | Idle indicator increments; toast on transition empty→idle. |
| 9 | MainBase damage | Red HUD edge flash 2 s; cooldown 10 s on re-fire. |
| 10 | SWARM aggression cross threshold | Toast + indicator color shift. |
| 11 | Construction progress | Welding VFX scales with builder count; sound loops. |
| 12 | Construction cancel | Dissipate VFX + cancel sound + refund flash. |
| 13 | Match timer 60 s | Yellow tint flips; toast once. |
| 14 | Match end | Activatable overlay pushes; gameplay IMC removed. |
| 15 | Multicast budget | Combat with 30+ units; multicast rate ≤ 30/s server-side. |
| 16 | Cosmetic on disconnect | Player disconnects mid-attack; cosmetic continues on remaining client without errors. |

### Out of MVP (Feedback)

- Replay-aware cosmetic logging.
- Accessibility audio alternatives (visual cues for hearing impaired).
- Dynamic audio music layers (combat intensity).
- Voice lines (faction VO).
- Hit decals persistent (e.g., burn marks on buildings).
- Damage number floaters above units.
- Cinematic kill cam.
- Sound-positional rebalance per camera zoom.

## References

- UI / UX gameplay spec — [`../GDD/09_UI_UX`](../GDD/09_UI_UX.md).
- Selection state — [`04_RTS_Selection_And_Commands`](04_RTS_Selection_And_Commands.md) §"Detailed Selection Rules (GP-0202)".
- Resource state — [`07_Resource_Architecture`](07_Resource_Architecture.md).
- GAS attributes — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- Module ownership — [`01_Module_Architecture`](01_Module_Architecture.md).
- Camera API for minimap — [`11_RTS_Camera`](11_RTS_Camera.md).
- Main Base + Assembly Yard — [`06_Building_Architecture`](06_Building_Architecture.md).
- Worker mining feedback — [`05_Unit_Architecture`](05_Unit_Architecture.md).
- MVP HUD task — [`../Development/Claude_Tasks/GP-0401_MVP_HUD`](../Development/Claude_Tasks/GP-0401_MVP_HUD.md).
- Feedback Pass task — [`../Development/Claude_Tasks/GP-0402_Feedback_Pass`](../Development/Claude_Tasks/GP-0402_Feedback_Pass.md).
