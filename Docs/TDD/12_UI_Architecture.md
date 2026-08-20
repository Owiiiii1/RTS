# UI Architecture

## Scope

Engineering-canonical UI architecture for GrimProtocol. Currently scoped до MVP HUD (per [`Claude_Tasks/GP-0401_MVP_HUD`](../Development/Claude_Tasks/GP-0401_MVP_HUD.md)). Broader UI architecture (theme system, localization, settings panel, accessibility, scaling) — pending GP-0701.

Декорація / стиль / лор — у [`../GDD/09_UI_UX`](../GDD/09_UI_UX.md). Цей TDD описує **state ownership, data binding, replication contracts і module separation**, не visual design.

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
| HUD root | `UCommonUserWidget` (BP child for layout) | Added to viewport by PC. |
| Activatable screen (OrderMenu, EndOfMatch, Pause, Lobby) | `UCommonActivatableWidget` | Pushed into `UCommonActivatableWidgetStack`. |
| Inline panels (SelectionPanel, ResourceReadout, MatchTimer, etc.) | `UCommonUserWidget` | Never raw `UUserWidget`. |
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
- `UGP_FoWWorldPresentationSubsystem` + native `UGP_FoWWorldOverlayWidget` are the first specialized
  world-presentation consumer. They bind directly to the trusted one-team LocalFoW mirror, not gameplay
  authority, and project bounded/coalesced Slate geometry behind normal HUD layers.
- This direct mirror binding is intentionally limited to the project-owned world renderer: camera
  reprojection is not a conventional HUD FieldNotify problem. Ordinary HUD/minimap widgets still
  consume ViewModels.
- The TEMP HUD remains unchanged until a production HUD is implemented and separately validated.

### MVVM Data Flow

```
Server-authoritative state                 Replication                ViewModel             Widget (View)
────────────────────────────               ──────────────             ─────────             ─────────────
UGP_PlayerAttributeSet.OrbitalFerronite ─► GAS attribute repl   ─►   UGP_ResourceVM       ◄─►  WBP_GP_HUD_ResourceReadout
UGP_SelectionComponent (local)       ─►    OnSelectionChanged    ─►   UGP_SelectionVM      ◄─►  WBP_GP_HUD_SelectionPanel
AGP_GameState.MatchTimeRemaining     ─►    RepNotify             ─►   UGP_MatchVM          ◄─►  WBP_GP_HUD_MatchTimer
UGP_OrbitalDeliverySubsystem.Catalog ─►    OnRep / delegate      ─►   UGP_OrderMenuVM      ◄─►  WBP_GP_HUD_OrderMenu
```

> Post-pivot (ADR-0009 + 2026-08-08 refinement): **no Build / Production queue UI**. Ordering surfaces:
> - **Unit Order** — manifest builder (slots / costs) → DropPod → MainBase Unit Drop Zone (no world reticle for normal units).
> - **Building Order** — Purchase → READY list → Deploy ghost (placement) → DropPod (no second spend).
> Shared DropPod presentation. `UGP_OrderMenuVM` / TEMP HUD may host both panels. Pre-pivot Production/Construction/BuildMenu VMs superseded.

Rules:

1. **VM populated by VM-owner adapter on local PC.** Each VM has a dedicated **adapter** (subobject on PC або dedicated subsystem) що:
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
| `UGP_ResourceVM` | `UGP_PlayerAttributeSet.{OrbitalFerronite, FerroniteScore, MaxUnits, CurrentUnits}` (own + opponent score) | `UGP_ResourceVMAdapter` (PC subsystem) | `WBP_GP_HUD_ResourceReadout` |
| `UGP_MatchVM` | `AGP_GameState.{MatchState, MatchTimeRemaining, FerroniteThreatValue, WinnerTeamId}` | `UGP_MatchVMAdapter` (PC subsystem) | `WBP_GP_HUD_MatchTimer`, `WBP_GP_HUD_SwarmThreat`, `WBP_GP_EndOfMatch` |
| `UGP_SelectionVM` | `UGP_SelectionComponent.{SelectedUnits, InspectedTarget}` (local PC) | `UGP_SelectionVMAdapter` | `WBP_GP_HUD_SelectionPanel`, `WBP_GP_HUD_InspectPanel`, `WBP_GP_HUD_CommandBar` |
| `UGP_OrderMenuVM` | `UGP_OrbitalDeliverySubsystem` drop catalog (`DA_GP_OrbitalDrop_*`), current `OrbitalFerronite`, current `CurrentUnits/MaxUnits` | `UGP_OrderMenuVMAdapter` (PC subsystem) | `WBP_GP_HUD_OrderMenu` |
| `UGP_CargoVM` | `UGP_CargoComponent.CurrentCargo` of single-selected worker | `UGP_CargoVMAdapter` | `WBP_GP_HUD_SelectionPanel` unit mode |
| `UGP_NotificationVM` | Local notification queue (PC pushes) | PC native | `WBP_GP_HUD_NotificationStack` |
| `UGP_MinimapVM` | `UGP_MinimapSubsystem` snapshot 5 Hz | Subsystem self | `WBP_GP_HUD_Minimap` |

VMs created у PC `BeginPlay` / `OnPossess`, registered у `UMVVMSubsystem` per local user.

### Input Routing — Common UI

- Project Settings → CommonUI Input Routing enabled.
- `AGP_PlayerController` extends project `APlayerController` (not Lyra `CommonPlayerController`) and integrates Common UI action routing (`UCommonUIActionRouterBase` / LocalPlayer Common UI services) without `CommonGame`.
- Action sets: `CommonUI.Default`, `CommonUI.OrderMenu`, `CommonUI.EndOfMatch` — switch on activatable widget activation.
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
7. **Common UI activation stack** для будь-якого modal (OrderMenu, EndOfMatch, Pause). Не raw `AddToViewport` для screens.

Specialized exception: the hit-test-invisible FoW world overlay is a native viewport presentation
adapter, not an interactive HUD screen. It may read the trusted local mirror directly for bounded
camera reprojection, but cannot read authority or mutate gameplay. The paired
`UGP_LocalFoWUnitPresentationSubsystem` is likewise a native world-presentation adapter: UnitBase
actors lifecycle-register, LocalFoW revisions push immediate reevaluation, and a bounded 10 Hz
registered-list pass catches movement across a static visibility edge. It only composes local
primitive/health/combat presentation and never changes actor replication or gameplay state.

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
| SWARM threat (pressure) | `AGP_GameState.FerroniteThreatValue` | `OnRep_FerroniteThreatValue` | All clients |
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

```
WBP_GP_HUD_Match (root, attached to PlayerController via ClientHUDClass)
├── WBP_GP_HUD_TopBar
│   ├── WBP_GP_HUD_MatchTimer
│   ├── WBP_GP_HUD_ResourceReadout         (Ferronite + Score + OpponentScore + Cap)
│   └── WBP_GP_HUD_SwarmAggression
├── WBP_GP_HUD_Minimap (top-right corner, mandatory MVP)
├── WBP_GP_HUD_SelectionPanel               (bottom-left; multi-mode: unit/group/building/site)
├── WBP_GP_HUD_InspectPanel                 (separate slot from SelectionPanel; never overlaps)
├── WBP_GP_HUD_CommandBar                   (bottom-center; gated by selection content)
├── WBP_GP_HUD_OrderMenu                    (modal overlay — orbital Order Menu, opened at Logistics Hub / hotkey; closed by Esc/RMB)
├── WBP_GP_HUD_DropReticle                  (visual layer for orbital drop-target reticle)
├── WBP_GP_HUD_NotificationStack            (transient toasts: "Cap reached", "Idle worker", "Insufficient Ferronite")
└── WBP_GP_HUD_EndOfMatch                   (hidden until match end)
```

Native HUD root class — `UGP_HUDWidget : UUserWidget`. BP child = `WBP_GP_HUD_Match`. PC у `BeginPlay` creates and adds to viewport (z=10).

### MVVM Binding Contract

Per widget — bind ViewModel via `UMVVMSubsystem`. Adapter populates VM.

| Widget | ViewModel (FieldNotify props) | Adapter subscribes to |
| --- | --- | --- |
| `WBP_GP_HUD_MatchTimer` | `UGP_MatchVM.{MatchTimeRemaining, FormattedTime, TimerColorTag}` | `AGP_GameState.OnMatchTimeChanged` (per-second push) |
| `WBP_GP_HUD_ResourceReadout` | `UGP_ResourceVM.{OrbitalFerronite, OwnScore, OpponentScore, CurrentUnits, MaxUnits, ScoreDelta}` | Own ASC attribute change delegates; remote PlayerState ASC for opponent score |
| `WBP_GP_HUD_SwarmThreat` | `UGP_MatchVM.{FerroniteThreatValue, ThreatStateTag}` | `AGP_GameState.OnFerroniteThreatChanged` |
| `WBP_GP_HUD_SelectionPanel` | `UGP_SelectionVM.{Mode, SelectedUnitVMs[], SingleUnitVM, BuildingVM}` | `UGP_SelectionComponent.OnSelectionChanged` (local) |
| `WBP_GP_HUD_InspectPanel` | `UGP_SelectionVM.{InspectedVM}` | Same delegate (InspectedTarget field) |
| `WBP_GP_HUD_CommandBar` | `UGP_SelectionVM.{AvailableCommandTags, DisabledCommandTags}` | Same delegate + cooldown attribute changes на selection |
| `WBP_GP_HUD_OrderMenu` | `UGP_OrderMenuVM.{AvailableDrops[], CanAffordPerEntry[]}` | Adapter listens to `OrbitalFerronite` changes + `UGP_OrbitalDeliverySubsystem` catalog |
| `WBP_GP_HUD_Minimap` | `UGP_MinimapVM.{ActorBlips[], LocalViewportRect}` | `UGP_MinimapSubsystem` snapshot tick |
| `WBP_GP_HUD_NotificationStack` | `UGP_NotificationVM.{ActiveToasts[]}` | PC `OnHUDNotification` multicast |
| `WBP_GP_EndOfMatch` (Activatable) | `UGP_MatchVM.{MatchState, WinnerTeamId, OwnScore, OpponentScore, Duration}` | `AGP_GameState.OnMatchStateChanged` |

**FieldNotify properties (приклад):**

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

### Selection Routing Rules

`SelectionPanel` mode determination, in order:

1. `SelectedUnits.IsEmpty() && !InspectedTarget.IsValid()` → hide panel + disable CommandBar.
2. `SelectedUnits.IsEmpty() && InspectedTarget.IsValid()` → InspectPanel shown, CommandBar disabled.
3. `SelectedUnits.Num() == 1`:
   - Tag `Selection.Type.Unit` → SelectionPanel "unit detail" mode.
   - Tag `Selection.Type.Building` → SelectionPanel "building detail" mode. (Post-pivot: no production-queue / construction-site sub-modes — assets arrive operational via orbital drop; ordering happens in the Order Menu, not the selection panel.)
4. `SelectedUnits.Num() > 1`:
   - All have `Selection.Type.Unit` → SelectionPanel "group" mode (portraits grouped by UnitDefinition).
   - Mixed types — forbidden per GP-0202 (replaced on click).

### CommandBar Population

Per `SelectedUnits[0].UnitDefinition.AllowedCommands`:

| Command tag present | Button shown |
| --- | --- |
| `GP.Command.Move` | "Move" |
| `GP.Command.Stop` | "Stop" |
| `GP.Command.Attack` | "Attack" |
| `GP.Command.AttackMove` | "A-Move" |
| `GP.Command.Mine` | "Mine" |
| `GP.Command.Repair` | "Repair" (Worker repair stays in MVP) |

Multi-select group: intersection of AllowedCommands across all selected units.

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

### Minimap (MVP — No FoW)

- All units / buildings visible to all players (no fog of war in MVP).
- Subsystem `UGP_MinimapSubsystem` snapshot 5 Hz: actor → 2D position + team color + type icon.
- Camera viewport rectangle drawn from local camera transform.
- Click-to-pan: minimap LMB → `AGP_CameraPawn::FocusOnLocation(world_pos)`.

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
| 1 | Empty selection | Click ground → SelectionPanel hidden, CommandBar disabled. |
| 2 | Single worker selected | Panel shows worker detail, cargo bar live, command bar enabled. |
| 3 | 24 workers | Group mode, portraits grouped, count displayed, single command bar. |
| 4 | Building selected | Building panel з production queue widget. |
| 5 | Inspect enemy | InspectPanel shows HP, SelectionPanel unchanged. |
| 6 | Esc clear | Both panels hidden, command bar disabled. |
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

Widget-side (BP or C++) binds через `MVVM View Binding`: `WBP_GP_HUD_SelectionPanel` listens to `SelectionVM.Mode` → conditional visibility of detail / group / building / construction sub-panels.

ResourceVM adapter — Ferronite attribute mirror:

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
| **Selection** (add unit) | — | — | EmissiveBoost on unit | — | SelectionPanel rebuild | — | Click | — | — | — | Local only; per GP-0202. |
| **Inspect enemy** | — | — | InspectBoost on target | — | InspectPanel show | — | Soft click | — | — | — | Local only. |
| **Marquee** | LMB drag rectangle | — | — | — | — | — | — | — | — | — | Local only. |
| **Move command issued** | Green pulse at click | — | SelectionRing flash 200 ms | — | — | — | CommandIssued SFX | — | — | — | Predictive cosmetic. Per GP-0203. |
| **Move command rejected** | Red pulse at click | — | — | — | — | — | CommandDenied SFX | Optional toast | — | — | `Client_NotifyCommandRejected` triggers. |
| **Unit starts moving** | — | — | — | — | — | EngineLoop start | — | — | — | — | Standard movement audio. |
| **Attack — fire** | — | MuzzleFlash + impact at target | — | — | — | WeaponFire 3D | — | — | — | — | Multicast unreliable; per GP-0204. |
| **Attack — hit damage** | — | HitImpact (sparks) at target | DamageFlash 100 ms on target | — | — | HitImpact 3D | — | — | — | — | Multicast on `OnHealthChanged` delta < 0. |
| **Attack — target killed** | — | DeathExplosion | — | RubbleMesh (delayed 0.5 s) | — | DeathSFX 3D | — | — | — | — | Standard `OnUnitDied`. |
| **Cooldown blocking attack** | — | — | — | — | — | — | — | — | — | Cooldown overlay on CommandBar Attack button | VM read from `AttackCooldown` tag. |
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
