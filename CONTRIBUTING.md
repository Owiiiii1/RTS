# Contributing

## Scope
Цей документ визначає інженерні та архітектурні правила для GrimProtocol — інді RTS на Unreal Engine 5.8.1. Він обов'язковий для будь-якої non-trivial зміни в `GP/Source/` або `GP/Content/GrimProtocol/`.

Канонічні документи проєкту:

- `CONTRIBUTING.md` — engineering rules (цей файл)
- `STYLE.md` — naming, code style, asset placement
- `Docs/GDD/` — game design canonical
- `Docs/TDD/` — technical design canonical
- `Docs/Architecture_Decisions/` — ADRs
- `Docs/Development/` — operational rules (Git, naming, skills)

Кожне імплементаційне рішення повинно явно враховувати `CONTRIBUTING.md` + `STYLE.md` + relevant TDD-документ. Жоден з них не опціональний.

## Project Direction

GrimProtocol — multiplayer-first RTS для малої інді-команди. Engine — Unreal Engine 5.8.1. Префікс — `GP`. Content path — `/Game/GrimProtocol`.

Проєкт розробляється:
- одразу під multiplayer (Steam, server-authoritative)
- одразу під Gameplay Ability System
- одразу під data-driven pipeline (Data Assets + Gameplay Tags)
- з мінімізацією технічного боргу
- без enterprise overengineering
- без копіювання Lyra architecture

Детальніше — `Docs/GDD/00_Project_Overview.md` і `Docs/TDD/00_Technical_Overview.md`.

## Core Philosophy: Simple First

Головне правило проєкту. Порядок пріоритетів при будь-якій новій feature:

1. Working implementation
2. Gameplay loop integration
3. Multiplayer synchronization
4. Readable architecture

Тільки потім — abstraction, optimization, advanced systems.

Це означає:
- Жодних "future-proof" архітектур без реального use case.
- Жодних generic frameworks "на майбутнє".
- Жодних layers of indirection без production проблеми, яку вони вирішують.
- Якщо рішення можна реалізувати простіше — реалізовуємо простіше.

## Hard Bans (Review-Blocking)

Категорично заборонено в production коді:

- Lyra architecture, Lyra Experience System, Lyra-style modular gameplay injections.
- Enterprise-style Unreal architecture (manager-of-managers, abstract factory hell, ECS-like abstractions без production need).
- Generic gameplay framework без явного use case в MVP.
- Масовий subsystem splitting (новий subsystem створюється тільки коли він явно власник lifecycle ширше за один actor/component).
- Manager-класи без single responsibility і documented owner.
- Deep inheritance chains. Composition over inheritance — за замовчуванням.
- Дублювання gameplay state поза GAS (health/resources/cooldowns/modifiers — тільки через AttributeSets і GameplayEffects).
- Gameplay authority logic у Blueprint.
- Gameplay calculations у Blueprint (damage, cost, modifiers, validation).
- Масивна Blueprint gameplay logic.
- Multicast RPC abuse (multicast тільки для cosmetic events; gameplay — через replicated state і RepNotify).
- Direct client gameplay authority (client пише input intent, server validates і replicates result).
- Hardcoded gameplay balance в C++ (balance живе в Data Assets).

Порушення цих пунктів — review-blocking, навіть якщо код працює.

## Module Architecture

Тільки 3 runtime модулі. Новий module — не створюється без documented technical reason і apparent production bottleneck.

### `GPRuntime`
Основний gameplay runtime: GameMode, GameState, PlayerController, PlayerState, CameraPawn, units, buildings, resources, commands, match flow, replication, multiplayer gameplay.

### `GPGASRuntime`
GAS layer: ASC, AttributeSets, GameplayAbilities, GameplayEffects, GameplayTags, costs, cooldowns, modifiers, gameplay states.

### `GPUIRuntime`
UI layer: HUD, widgets, unit panels, selection panels, minimap, command buttons, match UI, player UI, presentation layer.

Залежності — суворо односторонні:

```
GPUIRuntime  ->  GPRuntime  ->  GPGASRuntime
                     |              ^
                     +--------------+
```

`GPGASRuntime` не залежить від `GPRuntime`. `GPRuntime` не залежить від `GPUIRuntime`. UI читає gameplay state через interfaces, не навпаки.

Детальніше — `Docs/TDD/01_Module_Architecture.md`.

## Multiplayer Discipline

Multiplayer-first. Це архітектурне обмеження, не побажання.

### Authority Model

- Server authoritative за замовчуванням: gameplay state, GAS state, resource state, unit/building spawning, match state, gameplay validation, command validation, win/lose conditions.
- Client пише input intent (selection, command requests, camera). Жодних client-side gameplay calculations.
- Replication, RPCs, `UPROPERTY(Replicated)`, `OnRep_*`, `Server_*`/`Client_*`/`Multicast_*`, `DOREPLIFETIME_*`, `HasAuthority()`, `GetNetMode()` — first-class у `GPRuntime` і `GPGASRuntime`.

### RPC Discipline

- `Server_*` — для player input intent (request command, request build, request research).
- `Client_*` — для targeted UI feedback або local cosmetic reactions.
- `Multicast_*` — тільки для cosmetic events (death VFX, attack montage, building explosion). Gameplay стан синхронізується replicated properties і GAS, не multicast'ами.
- Кожен новий RPC має пройти `Validate` (для server RPC), і має documented authority comment у header.

### MVP Multiplayer Scope

Steam matchmaking, 2 players, host/client, PvP, одна мапа, replicated gameplay loop. Dedicated server не в MVP, але архітектура не блокує його future evaluation.

Детальніше — `Docs/TDD/03_Multiplayer_Architecture.md`, `Docs/TDD/08_Steam_Matchmaking.md`.

## GAS Discipline

Gameplay Ability System — головне джерело gameplay synchronization. Усе gameplay state, що змінюється під час матчу, живе в GAS.

### Що йде в GAS

- Player resources, unit limits, global modifiers — `UGP_PlayerAttributeSet`.
- Unit health, armor, damage resistance, cooldowns, attack stats — `UGP_UnitAttributeSet`.
- Cooldowns, costs, durations — `UGameplayEffect`.
- Gameplay actions (build, research, scan, ability triggers) — `UGameplayAbility`.
- Buffs, debuffs, gameplay flags — `FGameplayTag` через `LooseGameplayTags` або `GameplayEffect`-applied tags.

### Що НЕ йде в GAS

- Movement (це Movement Component territory; GAS може гранатувати тег `GP.Unit.State.Moving`, але не керує траєкторією).
- Selection state (це local PlayerController concern).
- UI state (це Widget territory).
- Camera state (це CameraPawn local concern).

### GAS Rules

- ASC живе на `AGP_PlayerState` для player-scoped attributes (resources, unit cap).
- ASC живе на `AGP_UnitBase` для unit-scoped attributes (health, armor).
- AttributeSets — `UGP_*AttributeSet`, replicated, з `GAMEPLAYATTRIBUTE_VALUE_INITTER` boilerplate.
- Abilities — replicated, з explicit `EGameplayAbilityNetExecutionPolicy` (default `ServerInitiated` для command abilities, `LocalPredicted` тільки коли responsiveness критичний і prediction validated).
- Effects — replicated, з explicit replication mode (`Mixed` для player-driven gameplay, `Minimal` для AI-driven, `Full` тільки коли треба для co-op spectating).
- Tags — централізовано через `FGameplayTag` native registration. Жодних magic-string tags в коді.

Детальніше — `Docs/TDD/02_GAS_Architecture.md`.

## Data-Driven Philosophy

Data First. Logic Second.

Будь-який gameplay object — unit, building, ability, command, resource, faction, research, production step, modifier — спочатку проектується як **Data Asset**, потім пишеться gameplay logic.

### Що описується в Data Asset

- Identity: DisplayName, Icon, Mesh.
- Cost / production time.
- Attributes / starting values.
- Gameplay tags (type, faction, capability).
- Allowed commands.
- Granted abilities.
- Modifiers (build speed, mining speed, research speed).
- UI metadata.
- Faction ownership.
- Replication hints (relevancy, priority — якщо потрібно).

### Що НЕ йде в Data Asset

- Виконуваний gameplay код. Behavior — в C++ classes/components, які читають Data Asset.
- Stateful runtime data. Data Asset — immutable config, не savestate.

### Folder Convention

- `/Game/GrimProtocol/DataAssets/Units/DA_GP_Unit_*.uasset`
- `/Game/GrimProtocol/DataAssets/Buildings/DA_GP_Building_*.uasset`
- `/Game/GrimProtocol/DataAssets/Abilities/DA_GP_Ability_*.uasset`
- `/Game/GrimProtocol/DataAssets/Resources/DA_GP_Resource_*.uasset`
- `/Game/GrimProtocol/DataAssets/Factions/DA_GP_Faction_*.uasset`

Детальніше — `Docs/TDD/05_Unit_Architecture.md`, `Docs/TDD/06_Building_Architecture.md`, `Docs/TDD/07_Resource_Architecture.md`.

## Gameplay Tag Philosophy

Gameplay Tags — основа gameplay state. Усе state-driven через теги: match state, unit type, building type, resource state, command state, buffs, debuffs, production state, movement state, faction categories, ability permissions, construction states, targeting states.

### Конвенції

- Кореневий неймспейс: `GP.*`.
- Native registration через `UGameplayTagsManager` у статичній `FGPGameplayTags` структурі. Magic-strings заборонені.
- Інженерна реєстрація — в `GPGASRuntime`.
- Designer-facing tags — через `.ini` (`Config/Tags/GP_GameplayTags.ini`).

### Baseline таксономія (MVP)

```
GP.Match.State.{Loading, WaitingForPlayers, Playing, Paused, Finished}
GP.Unit.Type.{Worker, SalvageWalker, Combat, Support, Building}
GP.Command.{Move, Stop, Attack, AttackMove, Mine, Repair, Sell, Demolish, OrderDrop, CancelOrder}
GP.Team.{Neutral, Player.One, Player.Two}
```

Authoritative full taxonomy — [`Docs/TDD/09_Gameplay_Tags.md`](Docs/TDD/09_Gameplay_Tags.md). Deprecated pre-pivot commands (`Build`, `QueueProduction`, `CancelProduction`, etc.) must not be used in new code.

Розширення — через ADR або TDD update, не ad-hoc.

## Blueprint Philosophy

Blueprint — це **не** основний gameplay layer.

### Blueprint допустимий

- UI assembly (`WBP_GP_*`).
- Editor workflow (Editor utilities, asset actions).
- Presentation (cosmetic-only Actors, decorative VFX glue).
- Lightweight visual scripting в level Blueprints для one-off level events.
- BP-нащадки abstract C++ класів (тонкі: meshes, audio, particle binding).
- Швидкий tuning через DataAsset-instance overrides у редакторі.

### Blueprint заборонений

- Gameplay authority logic.
- Gameplay calculations (damage, cost, validation, modifiers).
- GAS ability logic (тільки thin BP children якщо абсолютно треба).
- Replication setup і RPC declarations.
- Critical gameplay systems (match flow, command dispatch, resource transactions).

Правило: якщо Blueprint логіка має `If HasAuthority` — вона має бути в C++.

## Component-First Philosophy

Composition over inheritance. Спільна поведінка виноситься в `UActorComponent`-и, не в базові класи.

### Component-кандидати (baseline)

- `UGP_SelectionComponent` (PlayerController)
- `UGP_CommandComponent` (PlayerController)
- `UGP_PlayerUIComponent` (PlayerController)
- `UGP_UnitMovementComponent` (MobileUnit)
- `UGP_MiningComponent` (Worker)
- `UGP_CombatComponent` (Combat units)
- `UGP_TargetingComponent` (Combat units)
- `UGP_ProductionComponent` (Building)
- `UGP_ConstructionComponent` (Building)
- `UGP_StorageComponent` (Building)
- `UGP_RepairComponent` (Building / Engineer unit)

Базові класи — тонкі. Поведінка — через компоненти.

## File Size and Responsibility

- Soft cap: ~1000 рядків на файл.
- Якщо файл росте — переглянути responsibilities. Винести behavior у component, data у Data Asset, helper logic у utility namespace.
- Одна class declaration на один header, один class implementation на один cpp, з винятками тільки для очевидно related minor structs/enums.
- Definitions — у `.cpp`. Inline у `.h` — тільки для trivial accessors і templates.
- Forward declaration переважно поверх `#include` у headers, повний include — у cpp.

## RTS Input Pipeline

PlayerController — input orchestrator, не gameplay unit. Гравець грає за `AGP_CameraPawn`.

```
Enhanced Input
   |
   v
AGP_PlayerController
   |
   v
UGP_SelectionComponent (local) --+
   |                              |
   v                              v
UGP_CommandComponent  ----->  Server_RequestCommand (RPC)
                                   |
                                   v
                          AGP_GameMode / AGP_PlayerState validation
                                   |
                                   v
                          Unit command execution (GAS-driven)
                                   |
                                   v
                          Replicated state -> Clients
```

Юніти **ніколи** не слухають Enhanced Input напряму.

Детальніше — `Docs/TDD/04_RTS_Selection_And_Commands.md`.

## Pull Request Discipline

Кожен PR:

1. Touch-набір файлів обмежений однією логічно завершеною зміною. Не змішувати refactor + new feature + style cleanup в одному PR.
2. Має description з полями:
   - **Problem** — що вирішуємо
   - **Solution** — як реалізували
   - **Authority impact** — що змінилося в multiplayer authority model (або "none")
   - **Data impact** — які Data Assets / Tags / Attributes змінено (або "none")
   - **Risks / Edge cases**
3. Не ламає Definition of Done жодної з existing систем.
4. Має linked Jira ticket (за відсутності — створити трекер-issue заздалегідь).
5. Усі нові C++ класи проходять `STYLE.md` naming check.
6. Не вводить нові magic-string tags, hardcoded balance, manager classes, або subsystems без ADR.

## Feature Validation Checklist

Перед додаванням нової механіки в roadmap (не в код) — відповісти на checklist:

1. Чи потрібна для MVP?
2. Яку gameplay проблему вирішує?
3. Чи підсилює core gameplay loop?
4. Чи можна реалізувати простіше?
5. Multiplayer complexity?
6. GAS complexity?
7. UI complexity?
8. Production cost?
9. Data-driven?
10. Які Gameplay Tags потрібні?
11. Які AttributeSets потрібні?
12. Які Data Assets потрібні?
13. Чи створює scope creep?
14. Чи можна відкласти після MVP?

Якщо не проходить — `Docs/GDD/Backlog/` або `Docs/GDD/Out_Of_Scope/`.

## Skills

AI skills тримаються у `SKILLS/`. Активні skills:

- `SKILLS/UnrealEngine5-Skills/` — git submodule, https://github.com/UnrealXu/UnrealEngine5-Skills.git
- `SKILLS/game-design-framework/` — local skill, обов'язковий для будь-якого нового gameplay механізму перед роботою над кодом.

Будь-яка нова gameplay механіка проходить через `game-design-framework` skill і оцінюється по checklist вище.

Setup інструкції — `Docs/Development/External_Skills.md`.

## Language Rules

- Код, identifier-и, log categories, log messages, asset names, gameplay-facing strings — англійською.
- Внутрішня технічна документація і body text — українською.
- Markdown headings (`#`, `##`, `###`) — англійською.
- Player-facing strings — через `FText` + localization tables (EN source).
- Жодних українських коментарів у `.h`/`.cpp` production файлах.

## Definition of Done (per feature)

Feature вважається готовою тільки коли:

1. C++ implementation під owner-модулем.
2. Data Assets для всіх tunable значень.
3. Gameplay Tags зареєстровані native (якщо потрібні).
4. Replication setup явний (`DOREPLIFETIME_*`, RepNotify, RPC validation).
5. Authority model документований у header класу-власника.
6. PR description заповнений (Problem / Solution / Authority / Data / Risks).
7. Basic playtesting passed (single-player + 2-player Steam listen session).
8. Жодного нового hard-bans порушення.
9. Документація оновлена (відповідний TDD-файл + ADR якщо архітектурне рішення).
