# ADR-0002 — Data-Driven First

## Status
Accepted

## Context
RTS gameplay характеризується великою кількістю tunable parameters: unit costs, production times, damage, health, range, abilities, faction-specific tweaks. Якщо ці значення hardcoded у C++:
- Designer не може iterate без rebuild.
- Balance changes тригерять recompile / link cycle.
- Multiple unit types через subclasses → inheritance hell.
- A/B testing неможливе.

Альтернативи:
- Hardcoded numbers — швидко на старті, болюче довгостроково.
- INI config files — work, але рознесено, не editor-friendly.
- DataTable rows — middle ground, але обмежена per-row typed parameters.
- Data Assets (`UPrimaryDataAsset`) — editor-friendly, type-safe, з UObject inspector.

## Decision
**Data First. Logic Second.**

Будь-який gameplay object — unit, building, ability, command, resource, faction, research, production step, modifier — спочатку проектується як **Data Asset** (`UGP_*Definition : UPrimaryDataAsset`), потім пишеться gameplay logic.

### Що описується у Data Asset
- Identity (DisplayName, Icon, Mesh).
- Cost / production time.
- Attributes / starting values.
- Gameplay tags (type, faction, capability).
- Allowed commands.
- Granted abilities.
- Modifiers.
- UI metadata.

### Що **не** йде у Data Asset
- Виконуваний gameplay код. Behavior — у C++ classes/components, які читають Data Asset.
- Stateful runtime data. Data Asset — immutable config, не savestate.

### MVP DataAsset Inventory
- `UGP_UnitDefinition` — для AGP_UnitBase / AGP_MobileUnit.
- `UGP_BuildingDefinition` — для AGP_BuildingBase.
- `UGP_FactionDefinition` — для AGP_PlayerState faction binding.
- `UGP_ResourceDefinition` — resource type metadata.
- `UGP_CameraConfigDataAsset` — camera tuning.
- `UGP_AbilityDefinition` — ability designer-tunable config.
- `UGP_FeedbackBundle` — VFX / SFX asset refs per system.
- `UGP_NotificationConfig` — UI notification metadata.

## Soft References — Mandatory

**Hard rule:** усі **content references** у DataAssets і config — **тільки soft references**. Hard refs до gameplay content — review-blocking violation.

### Соft Reference Forms

| Reference type | Required wrapper |
| --- | --- |
| Data Asset → інший Data Asset | `TSoftObjectPtr<UGP_*Definition>` |
| Data Asset → Ability class | `TSoftClassPtr<UGameplayAbility>` |
| Data Asset → GameplayEffect class | `TSoftClassPtr<UGameplayEffect>` |
| Data Asset → StaticMesh / SkeletalMesh | `TSoftObjectPtr<UStaticMesh>` / `TSoftObjectPtr<USkeletalMesh>` |
| Data Asset → Material / MaterialInstance | `TSoftObjectPtr<UMaterialInterface>` |
| Data Asset → Texture | `TSoftObjectPtr<UTexture2D>` |
| Data Asset → SoundBase / SoundCue | `TSoftObjectPtr<USoundBase>` |
| Data Asset → Niagara System | `TSoftObjectPtr<UNiagaraSystem>` |
| Data Asset → AnimMontage / AnimSequence | `TSoftObjectPtr<UAnimMontage>` / `TSoftObjectPtr<UAnimSequence>` |
| Data Asset → Level (map name) | `TSoftObjectPtr<UWorld>` |
| Data Asset → DataTable | `TSoftObjectPtr<UDataTable>` |

### Loading — Asset Manager Only

Content loads через `UAssetManager`:

```cpp
// Single primary asset load
FPrimaryAssetId AssetId = UnitDef.GetPrimaryAssetId();
UAssetManager::Get().LoadPrimaryAsset(AssetId, /*Bundles=*/{}, FStreamableDelegate::CreateUObject(...));

// Bundle load via streamable
TArray<FSoftObjectPath> Paths;
for (const TSoftObjectPtr<UStaticMesh>& Mesh : Bundle->Meshes)
    Paths.Add(Mesh.ToSoftObjectPath());
TSharedPtr<FStreamableHandle> Handle =
    UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(...));
```

Lifetime — handles кешуються власником (наприклад, `UGP_MatchAssetLoader` subsystem або per-actor `Spawner`) до моменту, коли asset більше не потрібен. `Release` на match end / actor destroy.

### Anti-Patterns (Review-Blocking)

- ❌ `UPROPERTY() TObjectPtr<U..._Definition> X;` у DataAsset field. → Replace з `TSoftObjectPtr<U..._Definition>`.
- ❌ `UPROPERTY() TSubclassOf<UGameplayAbility> X;` у DataAsset field. → Replace з `TSoftClassPtr<UGameplayAbility>`.
- ❌ `UPROPERTY() TObjectPtr<UStaticMesh> X;` у DataAsset / config field. → Replace з `TSoftObjectPtr<UStaticMesh>`.
- ❌ `ConstructorHelpers::FObjectFinder<...>` у constructor для content. → Use Asset Manager + soft refs.
- ❌ `StaticLoadObject` у gameplay code для content. → Same fix.
- ❌ `LoadObject<...>` у tick / per-frame paths. → Async-load up-front, cache resolved pointer.

### Allowed Forms

- Engine-provided base classes (`AActor`, `UObject`, `UActorComponent`, `UAbilitySystemComponent`) — hard refs OK. Engine ships з ними.
- Runtime-resolved references **after** initial async load — cached `UObject*` / `UClass*` у component у acceptable hard form (transient pointer).
- `TSubclassOf<...>` у RPC payloads / runtime parameters — acceptable після того, як клас завантажений.

### Implementation Surface

- `UGP_MatchAssetLoader` (`UGameInstanceSubsystem` або owned by `AGP_GameState`) — preloads all relevant Faction / Map content during `LoadingMatch` state. Detailed contract — GP-0601.
- `AGP_GameMode::PreLoadMatch` ensures all `Faction.AllowedUnits`, `AllowedBuildings`, ability classes, mesh references resolved before transition to `Playing`.
- Late-loaded effects (post-MVP ability content, e.g., research-unlock abilities) — loaded on-demand з Streamable manager, cached for match duration.

## Consequences

### Positive
- Designer додає unit / building без C++ зміни.
- Balance iteration — editor-only, hot reload friendly.
- A/B testing — duplicate Data Asset з різними numbers.
- Чітке розділення data і logic.
- Зменшення коду: один `AGP_MobileUnit` C++ class замість 10 subclasses.
- **Повна content independence.** Жоден gameplay class не тягне з собою mesh / VFX / sound у memory на boot.
- Plugin / module boundaries — clean. Контент може жити в окремому plugin, gameplay не зламається.
- Memory footprint scaled by what's in play, не cooked references.

### Negative
- Edit-time validation slow (engine validates Data Asset on load, not on edit).
- Designer responsibility для balance — потребує designer-engineer комунікації.
- Async load — потрібен loading state UX (handled per GP-0501 LoadingMatch flow).

## Alternatives Considered
- **Hardcoded values + Blueprint defaults** — швидко, але плутаниця між BP default і Data Asset; refactor cost високий.
- **DataTable only** — обмежений per-row schema; не зручно для nested structures (tags, ability arrays).
- **INI / JSON external config** — non-editor-friendly; missing type safety.
- **Hard ObjectPtr refs з claimed simplicity** — поминається як trap: cooked package size cascade, plugin coupling, refactor pain. Rejected.

## References
- `/CONTRIBUTING.md` → Data-Driven Philosophy, Hard Bans (Hardcoded gameplay balance ban).
- `Docs/TDD/05_Unit_Architecture.md` → UGP_UnitDefinition schema.
- `Docs/TDD/06_Building_Architecture.md` → UGP_BuildingDefinition schema.
- `Docs/TDD/10_Data_Assets.md` → Asset ownership table.
- `Docs/TDD/01_Module_Architecture.md` → Asset Manager + match loader (GP-0601).
