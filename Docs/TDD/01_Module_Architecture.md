# Module Architecture

## Three Runtime Modules

```
GP/Source/
  GPRuntime/
  GPGASRuntime/
  GPUIRuntime/
```

Жодного editor-only modules для project gameplay code. Editor-only utilities — у GPRuntime з `bIsEditor` guard або у minor `GPEditor` module (створюється тільки при появі real editor utilities).

## Module Responsibilities

### GPRuntime

Owns:
- `AGP_GameMode`, `AGP_GameState`.
- `AGP_PlayerController`, `AGP_PlayerState`, `AGP_CameraPawn`.
- `AGP_UnitBase`, `AGP_MobileUnit`, `AGP_BuildingBase`.
- `UGP_*Component` (Selection, Command, Movement, Mining, Combat, Targeting, Production, Construction, Storage, etc.).
- `UGP_UnitDefinition`, `UGP_BuildingDefinition`, `UGP_FactionDefinition` Data Assets.
- `EGP_*` enums для gameplay state machines (`EGP_MatchState`, тощо).

### GPGASRuntime

Owns:
- `UGP_PlayerAttributeSet`, `UGP_UnitAttributeSet` (та інші, що з'являться).
- `UGP_GameplayAbility_*` C++ ability implementations (build, research, scan, etc.).
- `UGP_GameplayEffect_*` C++ effect implementations (de facto rare — більшість GE через UAsset).
- `UGP_AbilitySystemComponent` (custom ASC, якщо потрібно — у MVP можливо стандартний `UAbilitySystemComponent`; рішення фіналізується у [`02_GAS_Architecture`](02_GAS_Architecture.md)).
- `FGPGameplayTags` (singleton struct з native tag registration).
- GAS-specific calculations (UGP_ModMagnitudeCalculation children, UGP_GameplayEffectExecutionCalculation children).

### GPUIRuntime

Owns:
- `AGP_HUD` (якщо використовується HUD layer).
- `UGP_HUDLayout` (root widget controller).
- `UGP_*Widget` C++ base classes для widgets.
- `WBP_GP_*` BP widget children — живуть у `/Game/GrimProtocol/UI/`, але їхні C++ parents — у GPUIRuntime.
- UI ViewModels / model adapters (read replicated state, expose to widgets).

## Dependencies

### Build.cs Rules

```
GPGASRuntime.Build.cs:
  PublicDependencyModuleNames: Core, CoreUObject, Engine, GameplayAbilities, GameplayTags, GameplayTasks
  PrivateDependencyModuleNames: (no GPRuntime, no GPUIRuntime — leaf)

GPRuntime.Build.cs:
  PublicDependencyModuleNames: Core, CoreUObject, Engine, GPGASRuntime, GameplayAbilities, GameplayTags, EnhancedInput, OnlineSubsystem, OnlineSubsystemUtils
  PrivateDependencyModuleNames: NetCore, AIModule (required for AGP_AIController MVP singleplayer opponent)

GPUIRuntime.Build.cs:
  PublicDependencyModuleNames: Core, CoreUObject, Engine, UMG, Slate, SlateCore, GPRuntime
  PrivateDependencyModuleNames: GPGASRuntime (read attributes for UI bindings)
```

### Hard Rules

- `GPGASRuntime` MUST NOT include `GPRuntime` headers.
- `GPRuntime` MUST NOT include `GPUIRuntime` headers.
- `GPUIRuntime` CAN include `GPRuntime` і `GPGASRuntime` headers (UI читає gameplay state).
- Cross-module communication гравітує через:
  - Interfaces (declared у lower-level module, implemented у higher-level).
  - Delegates (broadcast у `GPRuntime`, listened у `GPUIRuntime`).
  - Replicated state (`GPRuntime` writes, `GPUIRuntime` reads via property access).

### What Cross-Module Calls Look Like

UI reading player resources:

```cpp
// GPUIRuntime widget (in C++ parent of WBP_GP_HUD_ResourceReadout)
const AGP_PlayerState* PS = GetOwningPlayerState<AGP_PlayerState>();  // GPRuntime type
const UGP_PlayerAttributeSet* AS = PS ? PS->GetPlayerAttributeSet() : nullptr;  // GPGASRuntime type
const float Resource = AS ? AS->GetResource() : 0.f;
```

Widget bind to attribute change:

```cpp
// On widget initialize
UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
ASC->GetGameplayAttributeValueChangeDelegate(UGP_PlayerAttributeSet::GetResourceAttribute())
   .AddUObject(this, &UMyWidget::OnResourceChanged);
```

## Module Boot Sequence

Module `StartupModule()`:
- `GPGASRuntime`: register native `FGPGameplayTags`, register attribute set lookup helpers.
- `GPRuntime`: nothing heavy у `StartupModule`; ECS-like global registries — avoided per pillars.
- `GPUIRuntime`: register Slate styles / fonts (мінімально).

Hot reload friendly: жодних static singletons з complex state. Tag registry — re-registers safely.

## Loading Phase

`.uproject` modules — усі `Default` LoadingPhase. GAS plugin вантажиться через plugin manifest (engine handles).

```json
"Modules": [
  { "Name": "GPGASRuntime", "Type": "Runtime", "LoadingPhase": "Default" },
  { "Name": "GPRuntime",    "Type": "Runtime", "LoadingPhase": "Default" },
  { "Name": "GPUIRuntime",  "Type": "Runtime", "LoadingPhase": "Default" }
]
```

(Editor module `GPEditor`, якщо коли-небудь з'явиться, — `PostEngineInit` або `Default` у Type:Editor.)

## Target.cs

- `GP.Target.cs` — game target, includes усі 3 modules.
- `GPEditor.Target.cs` — editor target, includes всі 3 + (optional) `GPEditor` module.
- `GPServer.Target.cs` — TBD. Dedicated server target — не у MVP, але добавляється тривіально (`Type = TargetType.Server`, dependencies як у Game).

## References

- ADR на module split — [`../Architecture_Decisions/ADR_0005_No_Lyra`](../Architecture_Decisions/ADR_0005_No_Lyra.md) (rationale, чому не Lyra game features).
- GAS placement — [`02_GAS_Architecture`](02_GAS_Architecture.md).
- UI access patterns — `STYLE.md` Widget Style section + this doc Cross-Module Calls section.
