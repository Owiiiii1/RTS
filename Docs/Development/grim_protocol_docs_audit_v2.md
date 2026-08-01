# Grim Protocol — аудит документації після Orbital Delivery pivot

## Мета документа

Цей файл описує суперечності в актуальній документації Grim Protocol після переходу до архітектури **Orbital Delivery**.

Завдання: оновити документацію так, щоб у canonical docs не залишалося активних pre-pivot правил про локальне будівництво, production queues, Barracks / AssemblyYard, ProductionComponent / ConstructionComponent та стару модель Ferronite.

Це не завдання на зміну C++ / Blueprint коду.  
Це завдання тільки на приведення документації до єдиного актуального стану.

---

# Загальний висновок

У документації одночасно присутні дві різні архітектури:

## Стара pre-pivot модель

```text
Worker builds locally
ConstructionComponent
ProductionComponent
ConstructionSite
Barracks / AssemblyYard produces units
Ferronite as direct spendable local resource
```

## Нова post-pivot модель

```text
Orbital Delivery
Drop Pods
No local construction
No local production queues
Logistics Hub / Order Menu
MainBase containers
Planetary Ferronite → shipped to orbit
OrbitalFerronite as spendable currency
FerroniteScore as cumulative victory score
```

Нова модель уже зафіксована в `ADR_0009_Orbital_Delivery_Pillar.md`, але частина GDD/TDD/Development документів усе ще описує стару модель як активну. Це створює ризик, що майбутня реалізація буде зроблена за старою архітектурою.

---

# 1. Orbital Delivery прийнятий як канон, але старі production/construction правила все ще активні

## Поточна суперечність

У `ADR_0009_Orbital_Delivery_Pillar.md` зафіксовано:

```text
No local production.
No local construction.
UGP_ProductionComponent, UGP_ConstructionComponent, AGP_ConstructionSite removed.
All non-initial assets arrive via AGP_DropPod.
```

Але в інших документах усе ще є активні згадки:

```text
UGP_ProductionComponent
UGP_ConstructionComponent
AGP_ConstructionSite
Server_QueueProduction
Server_BuildAt
Assembly Yard
Barracks
AllowedProductions
BuildTime
GE_GP_SpendFerronite
```

Найкритичніший файл:

```text
Docs/TDD/13_Architecture_Proposal.md
```

У ньому все ще описано:

```text
GPRuntime owns Production, Construction
AGP_BuildingBase owns UGP_ProductionComponent / UGP_ConstructionComponent
AGP_ConstructionSite
UGP_ProductionComponent
UGP_ConstructionComponent
Server_BuildAt
Server_QueueProduction
Server_CancelProduction
Server_SetRallyPoint
```

При цьому сам документ позиціонується як implementation map / single source перед C++ роботою.

## Що потрібно зробити

`Docs/TDD/13_Architecture_Proposal.md` потрібно переписати під post-pivot модель.

З активної архітектури потрібно прибрати:

```text
ProductionComponent
ConstructionComponent
ConstructionSite
BuildMenuVM
ProductionVM
ConstructionVM
Server_BuildAt
Server_QueueProduction
Server_CancelProduction
SetRallyPoint
GE_GP_AddFerronite
GE_GP_SendToOrbit
GE_GP_SpendFerronite
AssemblyYard
Barracks
```

Замість цього в активній архітектурі мають бути:

```text
UGP_OrbitalDeliverySubsystem
AGP_DropPod
UGP_OrbitalDropDefinition
UGP_OrderMenuVM
UGP_ContainerVM / UGP_StorageVM
Server_RequestOrbitalDrop / Server_OrderDrop
GE_GP_SpendOrbital
GE_GP_AddOrbital
GE_GP_AddScore
LogisticsHub
MainBase containers
FerroniteScore
OrbitalFerronite
```

---

# 2. `TDD/06_Building_Architecture.md` змішує стару і нову архітектуру

## Поточна суперечність

На початку файлу описані:

```text
AGP_Barracks
UGP_ProductionComponent
UGP_ConstructionComponent
Construction Flow
Production Flow
```

Нижче є `Post-Pivot Override`, який скасовує ці підходи.

Проблема: у canonical документах не варто залишати стару модель як активний текст із подальшим override. Старі секції краще винести в архів або чітко позначити як неактуальні.

## Що потрібно зробити

В активному файлі:

```text
Docs/TDD/06_Building_Architecture.md
```

залишити тільки актуальну post-pivot архітектуру будівель.

Старі production/construction секції перенести в:

```text
Docs/Archive/PrePivot_Building_Architecture.md
```

або позначити прямо в документі:

```md
# SUPERSEDED — DO NOT IMPLEMENT

This section describes the pre-orbital-delivery architecture.
It is archived for historical reference only.
Current canonical building architecture is Docs/TDD/06_Building_Architecture.md and ADR_0009.
```

---

# 3. Win/Lose conditions мають кілька різних версій

## Поточна суперечність

У `GDD/08_Win_Lose_Conditions.md` описано:

```text
Score-based victory only at 10:00.
Annihilation is out of MVP.
Resource threshold is out of MVP.
```

Але в `ADR_0009` і `GDD/10_Orbital_Delivery.md` уже є:

```text
delivery quota
MainBase destruction = annihilation / auto-loss
win when delivery quota reached OR highest Orbital Ferronite at timer expiry
```

У `GDD/12_Session_Tuning_And_Calibration.md` уже є:

```text
DeliveryQuotaFerroniteScore = 5000
bAnnihilationCountsAsWin = true
```

Це критично, тому що win/loss conditions впливають на:

```text
GameMode
GameState
PlayerState
GAS attributes
UI
match flow
balancing
```

## Рекомендована канонічна формула

```text
Primary win:
First player to reach DeliveryQuotaFerroniteScore wins immediately.

Fallback:
If 10:00 timer expires before quota, highest FerroniteScore wins.

Secondary loss:
If MainBase is destroyed, player loses immediately, if annihilation is enabled.

Score metric:
FerroniteScore = cumulative shipped ferronite.
OrbitalFerronite = spendable current currency, not victory score.
```

Важливо: `OrbitalFerronite` не повинен бути фінальним score, бо це spendable currency. Якщо використовувати його як score, гравець буде покараний за витрати.

Переможна метрика має бути:

```text
FerroniteScore = cumulative shipped value
```

---

# 4. Resource model частково оновлена, але старий Ferronite pool усе ще присутній

## Поточна суперечність

Новий канон у `GDD/06_Resources.md`:

```text
Planetary Ferronite = raw resource in containers, not spendable.
Orbital Ferronite = spendable currency.
FerroniteScore = cumulative shipped score.
```

Але в інших документах усе ще є:

```text
GE_GP_AddFerronite
GE_GP_SendToOrbit
GE_GP_SpendFerronite
Ferronite pool
MaxFerronite
spend Ferronite directly
drop-off instantly adds score
```

Це зустрічається, зокрема, в:

```text
GDD/02_Core_Gameplay_Loop.md
TDD/07_Resource_Architecture.md
TDD/13_Architecture_Proposal.md
First_Playable_Match.md
```

## Що потрібно зробити

Замінити старий ресурсний ланцюжок:

```text
Worker drop-off → AddFerronite → SendToOrbit → SpendFerronite
```

на новий:

```text
Worker drop-off → MainBase container fill
Ferronite physically stored at MainBase increases local swarm attraction / pressure
container full → launch state 2-3 sec
container shipped → GE_GP_AddOrbital + GE_GP_AddScore + decrease local stored Ferronite pressure
orbital order → GE_GP_SpendOrbital
```

Важливе уточнення по логіці агресії мобів:

```text
Swarm pressure must scale from Ferronite physically present at the base, not from FerroniteScore and not from OrbitalFerronite.
```

Тобто моби агряться на фероніт, який лежить у контейнерах / сховищі на базі. Чим більше сирого Planetary Ferronite накопичено на базі, тим більша хвиля або інтенсивність тиску.

Коли контейнер запускається на орбіту, локальний запас фероніта на базі зменшується, тому swarm pressure має спадати.

Рекомендована модель:

```text
PlanetaryFerroniteStoredAtBase = local threat magnet
OrbitalFerronite = spendable safe currency in orbit
FerroniteScore = cumulative shipped victory score
```

Наслідок:

```text
Worker drop-off increases PlanetaryFerroniteStoredAtBase.
Higher PlanetaryFerroniteStoredAtBase increases swarm pressure.
Container launch decreases PlanetaryFerroniteStoredAtBase.
Container launch increases OrbitalFerronite and FerroniteScore.
Sending Ferronite to orbit reduces local mob pressure.
Spending OrbitalFerronite does not affect swarm pressure.
FerroniteScore does not affect swarm pressure directly.
```

У `UGP_PlayerAttributeSet` актуальна модель має бути приблизно така:

```text
OrbitalFerronite      OwnerOnly
FerroniteScore        Visible to all
CurrentUnits          OwnerOnly
MaxUnits              OwnerOnly
```

`Ferronite / MaxFerronite` як player spendable pool потрібно прибрати з активної документації або явно позначити як pre-pivot deprecated.

Якщо потрібен окремий gameplay/stat параметр для тиску рою, він має відображати саме локально накопичений фероніт на базі, наприклад:

```text
PlanetaryFerroniteStoredAtBase
FerroniteThreatValue
BaseFerroniteStockpile
```

Назву потрібно вибрати одну й використовувати її послідовно в GDD/TDD.

---

# 5. AI opponent: ADR уже вибрав `AAIController`, але інші документи все ще містять `TBD`

## Поточна суперечність

`ADR_0008_AI_Opponent_AAIController.md` уже прийняв рішення:

```text
AGP_AIPlayerController : AAIController
AI does NOT use Server_RequestCommand RPC.
AI invokes server-side command helpers directly.
```

Але `TDD/03_Multiplayer_Architecture.md` усе ще пише:

```text
inherits from AGP_PlayerController or AAIController
AI uses same Server_RequestCommand infrastructure
```

`TDD/13_Architecture_Proposal.md` також містить:

```text
AGP_AIPlayerController | AGP_PlayerController або AAIController — TBD ADR
```

`GDD/03_Factions.md` теж залишає це як TDD decision.

## Що потрібно зробити

Усюди привести до одного рішення:

```text
AI controller derives from AAIController.
AI does not use client RPC.
AI invokes server-side command execution helpers directly.
AI has normal AGP_PlayerState with ASC.
```

Окремо варто переглянути назву:

```text
AGP_AIPlayerController : AAIController
```

Ця назва може вводити в оману, тому що клас не є `PlayerController`.

Кращі варіанти:

```text
AGP_OpponentAIController
```

або:

```text
AGP_AIController
```

Якщо назву `AGP_AIPlayerController` залишити, у документації потрібно явно пояснити, що це не PlayerController, а AAIController-derived controller.

---

# 6. Gameplay Tags registry застарів

## Поточна суперечність

`TDD/09_Gameplay_Tags.md` містить старі теги:

```text
GP.Building.Type.Barracks
GP.Resource.Primary
GP.Command.Build
```

А в інших документах уже використовуються:

```text
GP.Building.Type.LogisticsHub
GP.Command.OrderDrop
GP.State.PodInFlight
GP.Resource.Type.Ferronite
GP.Unit.Type.Defender
```

Також у `TDD/13` усе ще присутні старі теги:

```text
GP.Command.QueueProduction
GP.Command.CancelProduction
GP.Command.SetRallyPoint
GP.Building.Type.AssemblyYard
```

## Що потрібно зробити

`TDD/09_Gameplay_Tags.md` має стати єдиним актуальним registry-документом.

Мінімально потрібно додати або підтвердити:

```text
GP.Command.OrderDrop
GP.Command.CancelDrop / GP.Command.CancelOrder якщо потрібно
GP.Drop.Type.Unit
GP.Drop.Type.Building
GP.Drop.Type.Wall
GP.State.PodInFlight
GP.Building.Type.LogisticsHub
GP.Building.Type.DefensiveTurret
GP.Building.Type.Wall
GP.Building.Type.WallTurret
GP.Building.Type.FerroniteDeposit
GP.Match.WinReason.DeliveryQuota
GP.Match.WinReason.TimerScore
GP.Match.WinReason.Annihilation
GP.Match.WinReason.OpponentDisconnect
GP.Resource.Type.Ferronite
```

З active MVP потрібно прибрати або перенести в deprecated:

```text
GP.Command.Build
GP.Command.QueueProduction
GP.Command.CancelProduction
GP.Command.SetRallyPoint
GP.Building.Type.Barracks
GP.Building.Type.AssemblyYard
```

Можна залишити окрему секцію:

```text
Deprecated / Pre-Pivot Tags — do not use in new code
```

---

# 7. Unit docs усе ще кажуть, що Worker будує будівлі

## Поточна суперечність

`GDD/04_Units.md` містить:

```text
Worker role: mining, construction
AllowedCommands: Move, Stop, Mine, Build
GrantedAbilities: Build
Behavior: initiates construction
```

`TDD/05_Unit_Architecture.md` містить секцію:

```text
Build Mode (Modal Ghost Preview)
UGP_GA_Build
AGP_GhostBuilding
Server_BuildAt
GE_GP_SpendFerronite
```

Це конфліктує з `ADR_0009`, де Worker більше не будує локально.

## Що потрібно зробити

Worker має бути описаний так:

```text
Worker:
- mines Ferronite;
- carries raw Ferronite to MainBase containers;
- can repair, if repair remains in MVP;
- does not build local structures;
- does not produce;
- does not own Build ability.
```

Якщо repair залишається в MVP, потрібно окремо зафіксувати:

```text
GP.Command.Repair
Repair ability
repair target rules
repair cost, if any
```

Якщо repair не входить у MVP, його потрібно перенести в backlog.

---

# 8. UI architecture вимагає Common UI + MVVM, але project setup цього не підтверджує

## Поточна суперечність

У `TDD/12_UI_Architecture.md` написано, що обов’язкові:

```text
CommonUI
CommonGame
CommonInput
ModelViewViewModel
```

Але в `GP.uproject` зараз активні тільки:

```text
GameplayAbilities
EnhancedInput
OnlineSubsystem
OnlineSubsystemUtils
OnlineSubsystemSteam
```

І в `GPUIRuntime.Build.cs` немає явних залежностей на:

```text
CommonUI
CommonInput
ModelViewViewModel
CommonGame
```

Це може привести до того, що документація вимагатиме створення `UCommonUserWidget` / `UMVVMViewModelBase`, але проект не збереться.

## Що потрібно зробити

Потрібно вибрати один із двох варіантів.

### Варіант A — Common UI + MVVM є каноном

Тоді оновити:

```text
GP/GP.uproject
GP/Source/GPUIRuntime/GPUIRuntime.Build.cs
```

і додати потрібні dependencies/plugins.

Також варто перевірити, чи `GPRuntime.Build.cs` не має зайвої залежності від `UMG`, якщо `GPRuntime` не повинен залежати від UI.

### Варіант B — Common UI + MVVM не є hard requirement

Тоді в `TDD/12_UI_Architecture.md` потрібно змінити формулювання з hard requirement на recommended approach або future improvement.

---

# 9. `Claude_Work_Plan.md` застарів

## Поточна суперечність

У `Docs/Development/Claude_Work_Plan.md` у First Playable Target усе ще є:

```text
Barracks
Barracks costs 1 resource
Barracks increases max unit count
```

Це pre-pivot MVP.

## Що потрібно зробити

Оновити First Playable Target під поточну модель:

```text
Logistics Hub
Orbital Delivery
Container system
Order Menu
Drop Pod
OrbitalFerronite
FerroniteScore
DeliveryQuotaFerroniteScore
```

---

# 10. Назви деяких task-файлів залишилися pre-pivot

## Поточна проблема

Наприклад:

```text
GP-0304_Barracks.md
GP-S54_AI_PlayerController.md
```

Навіть якщо всередині є pivot notes, назви файлів усе ще підштовхують до старої моделі.

## Що потрібно зробити

Рекомендовано перейменувати або явно позначити:

```text
GP-0304_Logistics_Hub.md
GP-S54_AI_Controller.md
```

Якщо старі файли залишаються для історії, зверху потрібно додати:

```md
# SUPERSEDED — DO NOT IMPLEMENT
```

---

# 11. Проблемні формулювання в документації

## Поточна проблема

Деякі документи використовують формулювання:

```text
canonical
code-ready specification
single source before C++ work
MVP Production Loop
MVP Construction Loop
```

але всередині цих секцій усе ще є pre-pivot логіка.

Це створює високий ризик неправильного трактування.

## Що потрібно зробити

Якщо секція або документ застаріли, вони не повинні мати статус canonical/code-ready.

Потрібно:

1. або переписати документ під post-pivot модель;
2. або перенести його в `Docs/Archive/`;
3. або додати зверху чітке попередження:

```md
# SUPERSEDED — DO NOT IMPLEMENT

This document is pre-pivot and must not be used as implementation source.
Use ADR_0009 and current TDD/GDD files instead.
```

---

# Пріоритет виправлень

## Пріоритет 1 — `Docs/TDD/13_Architecture_Proposal.md`

Це головна implementation map. Поки вона містить стару архітектуру, її не можна використовувати як source of truth для реалізації.

Потрібно видалити або заархівувати:

```text
ProductionComponent
ConstructionComponent
ConstructionSite
BuildMenuVM
ProductionVM
ConstructionVM
Server_BuildAt
Server_QueueProduction
Server_CancelProduction
SetRallyPoint
GE_GP_AddFerronite
GE_GP_SendToOrbit
GE_GP_SpendFerronite
AssemblyYard
Barracks
```

І замінити на:

```text
OrbitalDeliverySubsystem
DropPod
OrbitalDropDefinition
OrderMenuVM
Container/Storage VM
BuildGridSubsystem
GE_GP_AddOrbital
GE_GP_AddScore
GE_GP_SpendOrbital
LogisticsHub
MainBase containers
```

---

## Пріоритет 2 — `Docs/GDD/08_Win_Lose_Conditions.md`

Потрібно зафіксувати:

```text
DeliveryQuotaFerroniteScore
timer fallback
annihilation / MainBase destruction
tie-break rules
score source = FerroniteScore
OrbitalFerronite is not victory score
```

---

## Пріоритет 3 — `Docs/TDD/06_Building_Architecture.md`

Потрібно прибрати активні production/construction flow секції.

Активний файл має описувати тільки:

```text
MainBase
LogisticsHub
DropPod-delivered buildings
containers
building placement rules
no local construction
no production queues
```

---

## Пріоритет 4 — `Docs/TDD/09_Gameplay_Tags.md` і `Docs/TDD/10_Data_Assets.md`

Ці документи мають бути синхронізовані з новою моделлю.

Потрібно оновити:

```text
Gameplay Tags
Data Asset types
OrbitalDropDefinition
BuildingDefinition
UnitDefinition
resource/cost fields
score fields
delivery fields
```

---

## Пріоритет 5 — `Docs/GDD/02`, `Docs/GDD/04`, `Docs/GDD/06`, `Docs/GDD/07`, `First_Playable_Match.md`

Там потрібно прибрати старі loop-описи й привести MVP flow до Orbital Delivery.

---

# Рекомендоване формулювання задачі на оновлення документації

```text
Task:
Audit and update Grim Protocol documentation after the Orbital Delivery pivot.

Goal:
Remove contradictions between the old local production/construction model and the new Orbital Delivery model.

Scope:
Documentation only.
Do not modify C++ code.
Do not modify Blueprint assets.
Do not create new gameplay systems.

Canonical current direction:
- No local production.
- No local construction.
- No Barracks / AssemblyYard production flow.
- No ProductionComponent.
- No ConstructionComponent.
- No ConstructionSite.
- All non-initial units/buildings/walls arrive through Orbital Delivery.
- Player orders assets through Logistics Hub / Order Menu.
- Assets arrive through Drop Pods.
- Worker mines Ferronite and delivers raw Ferronite to MainBase containers.
- Full containers launch to orbit.
- Ferronite physically stored at MainBase increases local swarm pressure.
- Launching containers to orbit decreases local swarm pressure.
- Shipped Ferronite increases OrbitalFerronite and FerroniteScore.
- OrbitalFerronite is spendable safe currency in orbit.
- FerroniteScore is cumulative victory score and does not directly increase swarm pressure.
- First player to reach DeliveryQuotaFerroniteScore wins.
- If match timer expires first, highest FerroniteScore wins.
- MainBase destruction causes immediate loss if annihilation is enabled.

Files to inspect and update first:
- Docs/TDD/13_Architecture_Proposal.md
- Docs/TDD/06_Building_Architecture.md
- Docs/GDD/08_Win_Lose_Conditions.md
- Docs/TDD/09_Gameplay_Tags.md
- Docs/TDD/10_Data_Assets.md
- Docs/GDD/02_Core_Gameplay_Loop.md
- Docs/GDD/04_Units.md
- Docs/GDD/06_Resources.md
- Docs/GDD/07_Buildings.md
- Docs/Development/Claude_Work_Plan.md
- Docs/Development/First_Playable_Match.md

Required changes:
1. Move obsolete pre-pivot sections to Docs/Archive/ or mark them clearly as SUPERSEDED — DO NOT IMPLEMENT.
2. Ensure active docs describe only the Orbital Delivery architecture.
3. Remove active references to Barracks, AssemblyYard, ProductionComponent, ConstructionComponent, ConstructionSite, Server_BuildAt, Server_QueueProduction, and SpendFerronite as the main production/building flow.
4. Update gameplay tags to include OrderDrop, DropPod, LogisticsHub, FerroniteScore, DeliveryQuota, TimerScore, and Annihilation win/loss reasons.
5. Update resource documentation to distinguish Planetary Ferronite, OrbitalFerronite, and FerroniteScore.
6. Update Worker documentation so Worker does not build local structures.
7. Update AI controller documentation so AI derives from AAIController and does not use client RPC.
8. Update win/loss conditions to use DeliveryQuotaFerroniteScore, timer fallback, and MainBase destruction loss.
9. If UI documentation requires CommonUI/MVVM, either mark it as a required project setup step or downgrade it from hard requirement.
10. Add a short warning at the top of archived or superseded files: SUPERSEDED — DO NOT IMPLEMENT.

Before editing documentation:
- list all files that will be modified;
- summarize the contradictions found;
- confirm that only documentation will be changed.

After editing documentation:
- summarize every modified file;
- explain which old rules were removed or archived;
- mention any unresolved ambiguity.
```

---

# Очікуваний результат

Після оновлення документації має бути один чіткий canonical direction:

```text
Grim Protocol MVP is Orbital Delivery based.

Players do not locally build or produce units.
Workers mine and deliver raw Ferronite to MainBase containers.
Ferronite stored at MainBase attracts mobs and increases swarm pressure.
Containers launch Ferronite to orbit.
Launching Ferronite to orbit reduces local swarm pressure.
Shipped Ferronite increases OrbitalFerronite and FerroniteScore.
OrbitalFerronite is spent on orbital orders.
Units, buildings, and walls arrive through Drop Pods.
The player wins by reaching DeliveryQuotaFerroniteScore, or by highest FerroniteScore at timer expiry.
MainBase destruction can cause immediate loss.
```

У canonical GDD/TDD/Development документах не повинно залишитися активних pre-pivot правил, які дозволяють:

```text
Barracks production
AssemblyYard production
local worker construction
ConstructionComponent
ProductionComponent
ConstructionSite
Server_BuildAt
Server_QueueProduction
Ferronite as direct local spendable pool
```
