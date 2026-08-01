# Contributing

## Scope
Цей документ визначає інженерні та архітектурні правила для роботи в репозиторії `pyronix_rowan_night`.

Правила покривають C++, Blueprints, asset organization, system boundaries і delivery thinking. Документ обов'язковий до прочитання перед будь-якою non-obvious зміною в `RN/` (legacy donor) або `RNC/` (active multiplayer-first).

Цей документ співпрацює з іншими canonical-доками і не дублює їх:

- `STYLE.md` — naming, language, content placement
- `TECHNOLOGIES.md` — обов'язковий стек і архітектурні наслідки
- `SYSTEMS.md` — реєстр підтверджених gameplay-систем і MVP-меж (RN legacy snapshot; RNC roadmap — у `PROJECT.MD`)
- `Confluence.md` — стандарт документації
- `DELIVERY_PACK_STANDARD.md` — 4-page execution pack з обов'язковими блоками Project Target / Multiplayer Scope / Authority Model / Migration Source
- `PROJECT.MD` — navigation entry point і AI index
- `Docs/Multiplayer/Overview.md` — team-facing high-level
- `Docs/Multiplayer/Rules.md` — engineering authority taxonomy + readiness checklist
- `Docs/Multiplayer/Session_And_Online.md` — session / Steam / listen server
- `Docs/Multiplayer/Inventory_And_Save.md` — inventory ownership і save model
- `Docs/Multiplayer/Mover_Network_Validation.md` — Mover risk
- `Docs/Architecture/Module_Layer_Architecture.md` — runtime module split
- `Docs/Architecture/GAS_Multiplayer.md` — GAS rules
- `Docs/Production/Save_Schema_Versioning.md` — save versioning planning

Кожне імплементаційне рішення повинно явно враховувати `CONTRIBUTING.md` + `STYLE.md` + `TECHNOLOGIES.md`, а для RNC задач — relevant документ у `/Docs/`. Жоден з цих документів не є опціональним.

## Project Direction

### Repo Scope
- `RNC/` — **active** UE 5.7 cooperative ARPG multiplayer-first. Default working area для нових gameplay/UI/content задач.
- `RN/` — **legacy donor / singleplayer prototype**. Frozen for production direction. Reference, donor source, archive of pre-pivot decisions.

### RN (Legacy) — Frozen Form
RN був single-player-first ARPG prototype. Зафіксована форма (canonical owner — `TECHNOLOGIES.md`, як snapshot):

- Unreal Engine 5.7
- Single-player
- Soulslike third-person camera
- Open world з World Partition
- First combat vertical slice — melee
- Стек: GAS, MetaHuman, Motion Matching, SpeedTree, Wwise, Common UI + MVVM, Mover

Нічого нового тут не імплементуємо без явного owner-направлення. Bugfixes у legacy demo допустимі. Архітектурні рефактори в RN — тільки якщо вони — підготовка до migration в RNC і явно так позначені.

### RNC (Active) — Multiplayer-First Cooperative ARPG
RNC — нова active production direction. Canonical owner напряму — `PROJECT.MD`. Коротко:

- Unreal Engine 5.7
- Cooperative ARPG; optional кооп, solo viable
- MVP target — 2 players; release direction — up to 4 players
- Conceptual references: Dark Souls / Elden Ring session cooperation, Monster Hunter session flow, Baldur's Gate 3 host-owned world; **не MMO**
- Listen server для MVP (host hosts і grає одночасно), Steam OSS primary platform, EOS deferred. Dedicated server **не у MVP** (architecture не блокує future evaluation).
- Host owns world state і world save; each player owns own profile (character + inventory + progression + Personal Player Quests).
- Per-player loot rolling (Monster Hunter style); inventory data lives у player profile, host validates pickup/drop/transfer transactions.
- GAS layer (ASC, attributes, abilities, GE-driven damage/effect pipeline) — у `RNGASRuntime`. Combat ACTOR-level (weapons, attack volumes, hit detection, encounter logic) — у `RNGameplayRuntime`. Combat **не** окремий module.
- `GameFeatures` pattern **не використовується** на поточному етапі. Lyra не використовується як архітектурний референс для module split (different production shape).
- Стек: GAS (з prediction/replication/correction), MetaHuman, Motion Matching, SpeedTree, Wwise, Common UI + MVVM, Mover (multiplayer validation pending — `Docs/Multiplayer/Mover_Network_Validation.md`)
- Окремий `RNSessionRuntime` layer для Steam OSS / session / invite / join flow. Cooperative gameplay glue (per-player loot rolling, drop-in policy enforcement, revive coop) — у `RNGameplayRuntime/Coop/`, не окремий module.

### Multiplayer Discipline (RNC)
RNC проектується як multiplayer-first з першого дня. Це обов'язкове архітектурне обмеження, не побажання.

- Кожна gameplay-система має explicit authority model: host/server-authoritative, player-owned (local profile), client-predicted, local-only, або cosmetic-only. Без неї система не вважається implementation-ready (детально — `Docs/Multiplayer/Rules.md`).
- Replication, RPCs, `UPROPERTY(Replicated)`, `OnRep_*`, `Server_*`/`Client_*`/`Multicast_*` UFUNCTION, `DOREPLIFETIME_*`, `HasAuthority()`, `GetNetMode()` — first-class у RNC коді. Використовуються дисципліновано згідно taxonomy.
- Host/server-authoritative за замовчуванням: combat damage resolution, health/stamina/core attributes, death state, revive state, enemy AI decisions, enemy aggro/target, **Global Host Quest** і world state, interactable state (doors/chests/containers — server-arbitrated FIFO), critical ability activation result, resource spending consequence, world save authoritative path, **inventory transaction validation** (pickup/drop/equip/consume transactions — host validates, але inventory data lives у player profile), **loot pickup transaction** (per-player loot rolling).
- Player-owned (local profile save): character progression, level, abilities, stats, equipment, inventory data, currency, **Personal Player Quest** progress, unlocks. **NOT fully MMO server-authoritative inventory.** Detail — `Docs/Multiplayer/Inventory_And_Save.md`.
- Client-predicted або local: movement input, camera, aim feel, animation anticipation, cosmetic VFX/SFX, UI feedback, input buffering, light local interaction prompt, ability startup feedback, hit reaction presentation (з server-confirmed authoritative result), local montage preview (з server confirmation).
- GAS у RNC — multiplayer контекст. Ability prediction, replication mode (Mixed/Minimal/Full), `FGameplayAbilityActorInfo` server/client logic, `Activate*Local*` vs `Activate*Server*` — частина дизайну нових ability/attribute класів (`Docs/Architecture/GAS_Multiplayer.md`).
- Mover у RNC — використовує `MoverNetworkPredictionLiaisonComponent` як backend liaison для prediction; це not "single-player illusion" — це network prediction для cooperative play, з pending validation pass (`Docs/Multiplayer/Mover_Network_Validation.md`).
- Не закладати MMO-only assumptions: persistent shared world, global economy, full-server simulation, large-scale multiplayer zones — поза скоупом RNC.
- Не закладати singleplayer-only flow у RNC код. RN-era заборона "no replication", "no OnRep_", "no Server_/Client_/Multicast_" — застосовується тільки до legacy RN snapshot, не до RNC.

### RN Singleplayer Discipline (Legacy Snapshot)
Для RN кодової бази RN-era правило зберігається як freeze: не вводити replication, RPCs, `OnRep_`, мережеві UFUNCTION-префікси в нові RN зміни (якщо вони взагалі робляться). Цей режим — для frozen donor стану; реактивація RN під multiplayer не планується.

### RNC Accepted MVP Risks
Зафіксовано як known risks для current production stage. Це не bugs і не blockers — свідомі trade-offs. Детальний список — `PROJECT.MD` (Accepted MVP Risks) і `Docs/Multiplayer/Inventory_And_Save.md`.

Найважливіше для contributor-а:

- **Item duplication через save manipulation / rollback / rejoin edge cases.** Anti-dupe мінімальний у MVP. Cooperative ARPG між friends — exploit surface acceptable. Не блокувати RNC implementation tasks на anti-dupe полировкою. Main MVP goal — gameplay validation, не inventory integrity proof.
- **Trust-the-client equip validation у MVP** допустимий для edge-cases — full audit cryptographic profile signing — release direction option.
- **Host advantage у listen server** — accepted MVP trade-off.
- **No host migration** — host disconnect = session end. Documented у UX.
- **No drop-in mid-encounter** — controlled coop. Join only у safe gameplay state.

Якщо нова RNC task пропонує work на anti-dupe / cryptographic save integrity / host migration / mid-encounter join — escalate owner перед стартом scope. Це поза MVP за CTO decision.

### GameFeatures Plugin Pattern
**Не використовується у RNC на поточному production етапі.** Reasoning у `Docs/Architecture/Module_Layer_Architecture.md` (GameFeatures Pattern section). Reevaluate коли core gameplay matured. До того часу — pure runtime modules, content у `RNC/Content/RNC/` flat tree.

## RN → RNC Migration Rules

Будь-який перенос коду, asset, pattern або decision з `RN/` у `RNC/` — explicit migration з review, не copy-paste.

- Заборонено blindly copy-paste singleplayer-first логіки в RNC. Donor код проходить multiplayer readiness review (`Docs/Multiplayer/Rules.md`).
- Кожен migration step документується як migration note (в Confluence implementation page або в Jira description під відповідним header) з полями:
  - **Source files** — що переноситься (точні шляхи в `RN/`).
  - **Why** — навіщо це потрібне в RNC (не "тому що було в RN", а конкретна gameplay/scope ціль).
  - **Singleplayer assumptions found** — перелік знайдених SP-only assumptions (наприклад: `GetPlayerCharacter(0)`, local-only inventory mutation, gameplay state mutated from widget, missing authority check, missing replication for shared state).
  - **Multiplayer changes required** — що змінено під multiplayer (introduced server validation, added `UPROPERTY(Replicated)`, added `Server_*` UFUNCTION, replaced direct state mutation through authoritative path, etc.).
  - **Technical debt remaining** — що **не** виправлено в цьому migration і потребує окремої follow-up задачі.
  - **Validation steps** — як перевірити, що migration не зламала behaviour (functional test, listen server test з 2 players, latency simulation).
- Migration scope тримати малим: один cohesive system / clear module / clear domain. Один migration commit/PR — одна migrated piece з повним migration note. Не bundling кількох систем у один міграційний прохід.
- Donor посилання у RNC коді — заборонені. RN class names у `#include`, hardcoded `/Script/RN.*` strings, RN content paths у RNC коді — це hidden module coupling за існуючим правилом (див. `No Hidden Module Coupling`). Переносимо implementation, не string-references.

## Multiplayer / Authority Notes (Mandatory Block for RNC Tasks)

Кожна gameplay/UI/AI/inventory/combat/interaction/quest/save задача, що змінює RNC, обов'язково містить блок наступної форми у task body (Jira description, Confluence implementation page, PR description — той owner-локацій, що задача потребує):

```
Multiplayer / Authority Notes
- Target project: RN / RNC / Docs
- Is this new work for RNC: yes / no
- Is RN used only as donor/reference: yes / no
- Authority owner: <who owns this state — server, GameMode, GameState, replicated subsystem, etc.>
- Client request: <what the client sends to the server, or "no client request">
- Server validation: <what the server checks before applying mutation>
- Replicated state: <which UPROPERTY-fields replicate, replication condition>
- Local-only state: <state that stays on each client locally and never replicates>
- Predicted state: <state with client-side prediction + server correction, or "no prediction">
- Cosmetic-only state: <state that only affects presentation>
- UI dependency: <how UI reads this; does UI bind to ViewModel that binds to replicated state?>
- Save/load impact: <does this hit the save? host save or guest save? authoritative save path?>
- Late join behavior: <what happens when a player joins mid-session>
- Disconnect behavior: <what happens when a player drops>
- Listen server risk: <known issues at listen-server hosting>
- Dedicated server risk: <known issues at dedicated server (deferred from MVP)>
- 2-player MVP behavior: <expected behavior at MVP coop scope>
- 4-player release behavior: <expected behavior at release coop scope, if different from 2P>
```

Правила:

- Gameplay task без цього блоку — **не вважається implementation-ready**. Reviewer повертає task на доопрацювання scope.
- "Docs only" задачі (без коду) — блок не обов'язковий, але рекомендований для design pages, що описують gameplay flow.
- "RN legacy bugfix" задачі — блок не обов'язковий. Якщо bugfix реально стосується RN-only frozen snapshot.
- Поля можуть бути `n/a` тільки з explicit reasoning (наприклад `Replicated state: n/a — cosmetic VFX only`), не як пасивний skip.

## Mandatory Rules
Порушення цих правил — review-blocking.

- Перед будь-якою non-obvious зміною — перевірити `Architecture`, `Architecture Rules`, `Source Tree` і відповідну system page в `Docs/Confluence/` (для RN legacy). Для RNC — relevant документ у `/Docs/` (Multiplayer / Architecture / Production).
- GAS — authoritative gameplay framework для combat-facing runtime logic. Не будувати паралельний combat framework, що дублює відповідальності GAS.
- Mover — єдина approved locomotion engine для player і gameplay characters. Не вводити legacy `Character Movement Component` патерни в gameplay-pawnи (RN legacy snapshot; для RNC те саме — Mover з multiplayer validation per `Docs/Multiplayer/Mover_Network_Validation.md`).
- **RNC**: кожна gameplay-система має explicit authority/local state model. **RN (legacy)**: не вводити replication / RPC / network state у RN-only задачі — RN frozen single-player.
- Common UI — єдиний approved UI stack. `MVVM` — обов'язковий патерн для menu-архітектури.
- Widget Blueprints не використовують `Canvas Panel` як root або wrapper. Дозволено тільки коли віджет реально потребує абсолютного позиціонування або anchor-based screen placement. Default — найменша семантична панель: `Overlay`, `Border`, `Vertical Box`, `Horizontal Box`, `Size Box`, або CommonUI containers.
- Gameplay controllers і gameplay-state components не володіють Widget Blueprint класами, widget instances, viewport mounting або menu stack operations. UI mounting належить HUD/UI-layer owner module — `RNUIRuntime` (active RNC; також name match у RN legacy donor); gameplay-компоненти експонують state, events або view models.
- Для RNC: UI / widgets не мутують gameplay state напряму. Всі gameplay mutations — через authoritative path (input → controller → server request → server validation → mutation → replicated state → UI refresh).
- Production content створюється тільки в `RN/Content/RowanNight` (legacy) або `RNC/Content/RNC` (active). Корінь `*/Content` і `*/Content/Developers` — не production dependency.
- Не додавати нові source files, modules або структурні зміни без перевірки placement і архітектурного впливу.
- Перед додаванням нових modules, abstractions або великих структурних refactorів — явно описати overengineering risk і обґрунтувати, чому простіший варіант недостатній на поточному стейджі.
- Тимчасові hack-и, що суперечать цільовій архітектурі — неприйнятні без явного approval і явного позначення як test-only.

## Engineering Goals
Кожна контрибуція повинна рухати проєкт у бік:

- scalable architecture
- modular systems
- SOLID-oriented design
- maintainability over quick fixes
- clear separation of responsibilities
- low coupling between systems
- високої читабельності для програмістів і дизайнерів

SOLID має пріоритет над зручністю. Якщо вибір конфліктує з чистим розділенням відповідальностей, низьким coupling або підтримуваною архітектурою — треба обрати SOLID-варіант, навіть якщо він довший.

Оптимізуємо за довгострокове здоров'я проєкту. Швидка реалізація, що створює прихований tech debt, неприйнятна без явного project-owner approval як test-only.

## Mandatory Rules
Порушення цих правил — review-blocking.

- Перед будь-якою non-obvious зміною — перевірити `Architecture`, `Architecture Rules`, `Source Tree` і відповідну system page в `Docs/Confluence/`.
- GAS — authoritative gameplay framework для combat-facing runtime logic. Не будувати паралельний combat framework, що дублює відповідальності GAS.
- Mover — єдина approved locomotion engine для player і gameplay characters. Не вводити legacy `Character Movement Component` патерни в gameplay-pawnи.
- Не вводити replication, RPCs, network state handling, multiplayer flow control в gameplay-код (див. `Single-Player Discipline`).
- Common UI — єдиний approved UI stack. `MVVM` — обов'язковий патерн для menu-архітектури.
- Widget Blueprints не використовують `Canvas Panel` як root або wrapper. Дозволено тільки коли віджет реально потребує абсолютного позиціонування або anchor-based screen placement. Default — найменша семантична панель: `Overlay`, `Border`, `Vertical Box`, `Horizontal Box`, `Size Box`, або CommonUI containers.
- Gameplay controllers і gameplay-state components не володіють Widget Blueprint класами, widget instances, viewport mounting або menu stack operations. UI mounting належить HUD/UI-layer ownerам (`RNUIRuntime`); gameplay-компоненти експонують state, events або view models.
- Production content створюється тільки в `RN/Content/RowanNight`. Корінь `RN/Content` і `RN/Content/Developers` — не production dependency.
- Не додавати нові source files, modules або структурні зміни без перевірки placement і архітектурного впливу.
- Перед додаванням нових modules, abstractions або великих структурних refactorів — явно описати overengineering risk і обґрунтувати, чому простіший варіант недостатній на поточному стейджі.
- Тимчасові hack-и, що суперечать цільовій архітектурі — неприйнятні без явного approval і явного позначення як test-only.

## Core Contribution Rules

### Architecture Discipline
- При імплементації або рефакторингу системи треба обрати target architecture одразу і будувати прямо до неї. Не закладати свідомо неправильну ownership-модель з "потім почистимо".
- Якщо цільова архітектура actor-orchestrated, component-driven, manager-driven або subsystem-driven — треба поважати цей кордон з першого імплементаційного кроку, а не будувати тимчасову proxy-стіну, яку доведеться розбирати.

### Class Realization Pattern (Abstract C++ + Final Blueprint)

Core gameplay framework classes і core gameplay actors розшаровуються на два рівні:

1. **Abstract C++ base** — `UCLASS(Abstract)`. Owns reflected API: replication declarations, virtual hooks, RPC implementation, gameplay logic, lifecycle overrides. НЕ instantiable напряму. Module placement per `Docs/Architecture/Module_Layer_Architecture.md` (RNC) або існуюча RN module structure (legacy).
2. **Final Blueprint leaf** — concrete BP child of the C++ class. Owns designer-tunable surface: asset references (mesh, animation, materials, SFX, cues), tweaked default values, default class wiring (DefaultPawnClass / PlayerControllerClass / PlayerStateClass / GameStateClass на GameMode). Лежить у `RNC/Content/RNC/Core/Blueprints/<Domain>/BP_RN_*` (RNC) або `RN/Content/RowanNight/Core/Blueprints/<Domain>/BP_*` (RN legacy).
3. **Engine wiring** — `DefaultEngine.ini` (`GlobalDefaultGameMode`, `GlobalDefaultServerGameMode`, default pawn/PC/PS/GS class у GameMode BP) посилається на **BP path**, не на Abstract C++ class. Посилання на Abstract клас із ini призводить до spawn failure + log spam на startup.

#### Where the rule applies (mandatory)
- **GameMode base** — Abstract C++ + Final BP. Без BP-шару designer не може tune-ити default classes, який ramp-up для нової мапи / нового coop setup.
- **Base Character / base Pawn (gameplay)** — Abstract C++ + Final BP. Без BP-шару не можна назначити skeletal mesh, animation blueprint, default Mover config, audio cues — все designer-owned surface.
- **Base weapon actor / interactable actor / base enemy** (downstream tasks) — Abstract C++ + Final BP. Та сама причина.

#### Where the rule does NOT apply (allowed C++ direct reference)
- **GameInstance** — C++ direct reference у ini (engine convention; нема designer-tunable surface, що виправдовує BP layer на проектах нашого розміру).
- **AssetManager** — завжди C++ direct reference (engine convention).
- **PlayerController / PlayerState / GameState bases** — Abstract optional, BP leaf optional. Якщо клас не має designer-tunable defaults і використовується тільки як inheritance root для concrete C++ subclass — лишається non-Abstract concrete C++. BP wrapper додається лише коли реально потрібна designer-tunable surface.

#### Multi-layer variant
Між Abstract base і Final BP допустимий **concrete C++ subclass**, коли gameplay logic набирається в кілька зрізів (приклад RN legacy: `ARN_GameModeBase` Abstract → `ARN_GameModeGameplay` concrete → `BP_GameModeGameplay` final BP, ini → BP path). Це не обов'язково для кожної класи; додається коли логіка реально розшаровується.

#### Forbidden
- Abstract C++ class посилається з ini напряму як runtime instance (`GlobalDefaultGameMode=/Script/Module.AbstractClass`).
- Final BP leaf містить gameplay logic у node graph (Blueprint-encoded gameplay rules — anti-pattern; BP-leaf тримає тільки defaults і asset refs; gameplay logic — у C++).
- Designer-tunable defaults (asset references, gameplay tuning values) hard-coded у C++ constructor, коли BP-шар уже існує або заплановано.
- Concrete C++ class без BP wrapper там, де є designer-tunable surface (наприклад GameMode без BP — designer не може override default classes per-map).

#### Why
- Gameplay logic залишається у version-controlled C++ (review, refactor-tooling, compile-time перевірки).
- Designer iteration на assets швидка — без C++ compile cycle.
- Clear boundary: engineer-owned (logic, replication, authority) vs designer-owned (defaults, asset refs, per-context tweaks).
- BP-final-leaf — стабільний asset id у ini, який не пере-розпадається при C++ rename/refactor (BP path тримається класовою назвою asset-а, а не fully-qualified C++ symbol).

#### RN reference
`ARN_GameModeBase` (Abstract C++) → `ARN_GameModeGameplay` (concrete C++) → `BP_GameModeGameplay` у `/Game/RowanNight/Core/Blueprints/GameModes/` (Final BP) → ini посилається на BP. Це canonical приклад двошарової реалізації.

### Hardcoding
- Не хардкодити gameplay setup, controls, asset references, tuning data або designer-owned configuration в коді, якщо це не explicitly approved test setup.
- Тимчасовий hardcoded test setup дозволений тільки з явним project-owner confirmation і має бути явно позначений як test-only.

### Function Focus
- Уникати copy-paste імплементацій. Якщо логіка починає повторюватися — extract.
- Тримати відповідальності вузькими і явними. Один клас, subsystem, asset або Blueprint = одна чітка ціль.
- Тримати функції екстремально сфокусованими. Одна функція = одна дія.
- Іменувати функції за тим, що вони реально роблять. Назва описує дію напряму.
- Уникати "function dumpsters", які валідують, запитують, мутують, маршрутизують і логують непов'язані concerns в одному body.
- Не перевантажувати функції змішаними відповідальностями для зручності.
- Getters тільки повертають дані.
- Setters тільки призначають або оновлюють state.
- Те саме single-purpose правило застосовується до validators, builders, loaders, dispatchers і handlers.

### Wrappers and Indirection
- Не додавати forwarding wrappers, які тільки передають виклик на рівень глибше без додавання ownership, policy, translation, validation або реального boundary.
- Не будувати wrapper chains виду `A -> B -> C`, де middle layer не додає поведінки і тільки збільшує review noise.
- В закритих системах з обов'язковими constructor-created subobjects треба resolve invariants одного разу і працювати з ними напряму, а не повторно re-querry-ити їх через шари getterів.
- Не lazy re-cache-ити стабільні constructor-created subobjects знову і знову. Якщо owner гарантує lifetime — resolve один раз у правильній lifecycle-точці і використовувати напряму.

### Magic Numbers
- Runtime tuning values і authored defaults повинні мати імена. Не розкидати unexplained numeric literals в motion, physics, interaction, geometry або combat коді.
- Identity і math literals (`0.0f`, `1.0f`, `-1.0f`) допустимі тільки коли вони реально universal math/engine values. Domain-specific значення повинні мати named constants.

### Comments and Logging Discipline
- Коментарі — обов'язкові, не опційні, для project-коду, що визначає API, ownership, intent, constraints або non-obvious runtime behavior.
- Не залишати reflected fields і public gameplay API без коментарів.
- Кожен `UCLASS`, `USTRUCT`, важливий internal struct, delegate, `UFUNCTION` і `UPROPERTY` повинен мати короткий коментар, що пояснює призначення.
- Missing declaration comments на reflected API — incomplete implementation, не polish task.
- Новий або змінений reflected API у PR без `/** ... */` declaration comment є review-blocking finding на конкретному changed line. Reviewer не повинен зводити такі порушення тільки до summary, якщо кожен метод/поле потребує окремого локального fix.
- Code-facing comments у `.h` і `.cpp` пишуться англійською. Українські C++ comments, commented-out debug code або explanatory comments у source/header файлах є review finding за `STYLE.md`, навіть якщо логіка коду працює.
- Logging — частина implementation contract. Нові gameplay-системи і runtime flows вводять scoped log category і логи з самого початку. Якщо логів немає — задача не завершена.
- Не створювати God objects, God classes, God functions.

## C++ Standards

- Дотримуватися Unreal Engine конвенцій для class layout, reflection macros і module organization.
- Імплементація — в `.cpp`. Headers — declarations і мінімальна inline-логіка.
- Базові типи — Unreal/Epic best practices. `int` і `int32` обидва прийнятні, якщо використані intentionally.
- `nullptr`, ніколи `NULL`.
- `check()`, `ensure()`, `ensureMsgf()` — тільки коли вони justified і виражають реальний invariant або recoverable validation path.
- Не silence-ити unused-parameter warnings через `(void)Parameter;`. Видалити ім'я параметра, видалити мертвий параметр, або видалити мертвий function path.
- Не пропускати імена параметрів у declarations і definitions. Кожен function parameter має explicit описове ім'я, навіть коли intentionally unused.
- Якщо параметр intentionally unused через interface, delegate, override або engine callback contract — тримати ім'я explicit з префіксом `Unused`.
- Не приховувати очевидні required-object assumptions за повторюваними defensive `nullptr` guards. Invariants — intentionally і там, де вони щось реально комунікують.
- Не вводити local proxy variables або helper functions, які тільки restate already-owned object без зменшення складності.
- Не wrap-ати single utility call всередині component або actor method, якщо він не додає реальну domain policy, validation, lifecycle meaning або ownership value.
- Не створювати owner-subobject access detours виду `Owner -> getter -> wrapper -> helper`, коли caller вже володіє або вже resolved стабільний об'єкт.
- Includes — мінімальні. Forward declarations — preferred.
- Не залишати commented-out code в commit-ах.
- Не додавати dead code "на потім".
- Не дозволяти `.cpp` або `.h` файлам рости в oversized monoliths. Якщо файл стає некомфортним — split за відповідальністю на менші focused files, helpers, components, managers або типи.
- Якщо запитана зміна штовхає файл у нездоровий ріст — явно попередити про це, **до** того як це стане 10k+ або 100k+ char проблемою.
- Unreal-style declaration comments консистентно. `/** ... */` для типів, reflected fields, delegates і функцій оголошених у headers.
- Коментарі пояснюють purpose і responsibility, не syntax.
- Public методи документують дію, expected inputs (коли non-obvious) і важливу output/return семантику.
- Reflected fields документують що value контролює або зберігає, особливо коли поле designer-facing або впливає на runtime feel.
- Private state коментується, коли його роль не очевидна з імені.
- Self-review: явно перевірити, що нові reflected declarations прокоментовані і нові runtime systems містять meaningful logs.
- PR review: для кожного changed C++ hunk явно пройти `STYLE.md` language rules і цей `Comments and Logging Discipline` section. Пропущені `UFUNCTION`/`UPROPERTY` comments і non-English C++ comments треба залишати inline на конкретних рядках.

### RN Naming Reminders
- C++ project-domain класи зберігають `RN_`-style naming там, де це вже частина codebase (наприклад `RN_PlayerCharacter_Mover`, `URN_AbilitySystemComponent`, `URN_ViewModelPlayerHUD`).
- Module API macros (`RN_API`, `RNABILITYRUNTIME_API`, `RNUIRUNTIME_API`, `RNAUDIORUNTIME_API`) — для всього public-facing class/struct/function exposure між модулями.

## Smart Pointer Policy

Використовувати ownership-aware pointer types замість default raw pointer звичок.

- `TObjectPtr<>` — для reflected `UObject` references, що зберігаються в `UPROPERTY`.
- `TWeakObjectPtr<>` — для non-owning references на `UObject` instances, що можуть стати invalid.
- `TSoftObjectPtr<>` і `TSoftClassPtr<>` — для asset references, що не повинні форсити eager loading.
- `TUniquePtr<>` — для exclusive ownership non-`UObject` runtime helpers.
- `TSharedPtr<>` і `TSharedRef<>` — для shared ownership non-`UObject` типів, коли цей ownership-model реально потрібен.
- Уникати raw owning pointers.
- Raw pointers допустимі тільки для short-lived, non-owning access, де Unreal API їх вже очікує.
- Не redundantly initializing `TObjectPtr` через `= nullptr`. `TObjectPtr` default-null уже, тому явна null-ініціалізація — шум.
- Не використовувати `TObjectPtr` members у worker-класах, щоб mirror-ити actor-owned scene objects або orchestration dependencies. Якщо orchestrator володіє об'єктом — передавати reference або context у worker action, а не давати worker накопичувати hidden access до actor graph.

Pointer type обирається за lifetime, ownership і load behavior, не за personal preference.

## No Hard References

Hard references уникаємо, якщо немає documented intentional reason.

- Не hard-reference-ити heavy assets з core gameplay класів (combat actorів, ASC, attribute setів, mover pawn-у, base UI controller-ів).
- Не створювати always-loaded chains через careless Blueprint defaults або direct C++ asset references.
- Префер soft references для контенту, що може бути streamed, swapped, configured або loaded on demand.
- Перевіряти dependency cost з урахуванням open-world streaming і MetaHuman + SpeedTree обсягів — особливо для enemies, locations, items, UI screens, procedural content.

Якщо hard reference необхідний — задокументувати чому це безпечно і чому soft reference не підходить.

## Data-Driven First

Проєкт використовує data-driven підхід всюди, де це практично.

- Префер `PrimaryDataAsset`, `DataAsset`, `DataTable` і config-driven setup замість hardcoded gameplay values.
- Designer-tunable дані — в assets, не в розкиданих constantах.
- `RN_AssetManager` — central orchestration layer для primary assets і controlled loading flows. Не bypass-ити цю стратегію локальними `LoadObject<>` calls у gameplay-коді.
- Системи проектуються так, щоб дизайнери могли створювати нові items, enemies, abilities, dialogue lines, quest steps, UI screens **без C++ rewrites**.
- Runtime logic відокремлений від content definitions.
- Уникати embedding content identity напряму в gameplay code, коли asset-driven lookup viable.
- Input setup, action assets, mapping contexts і designer-tunable gameplay feel — data-driven by default. RN-власні `IMC_Character` і `DA_InputConfig_Gameplay` — owner цієї конфігурації.
- GAS ability grants — data-driven через ability sets (DataAssets), не hardcoded `GiveAbility` calls в gameplay коді.

Очікувані patterns:

- items визначаються через assets або таблиці
- procedural content pools збираються з data assets або registries
- spawnable content discoverable через asset pipeline
- balance values живуть у data, не в code branches
- ability sets, attribute defaults, gameplay effect cooldowns/costs — у DataAssets

## Enums, Structs, and Shared Types

Shared типи організовуються передбачувано.

- Enums, що використовуються в кількох класах, живуть у dedicated header files з суфіксом `Types`.
- Префер файли виду `RN_CombatTypes.h`, `RN_InventoryTypes.h`, `RN_LocomotionTypes.h`, `RN_DialogueTypes.h`.
- Не ховати project-wide enums глибоко в unrelated class headers.
- Shared structs, handles, IDs і descriptors також групуються в purpose-driven types-headers, коли це покращує reuse і clarity.
- Type definitions групуються за domain, не за convenience.

Це правило існує для зменшення circular dependencies, покращення discoverability і тримання shared gameplay vocabulary консистентним.

## Reuse and Utility Code

Повторювана логіка extract-иться, не копіюється.

- Якщо behavior повторюється в кількох системах — переноситься в reusable abstraction.
- Префер маленькі well-named helpers замість дубльованих code blocks.
- Тримати utility code cohesive. Не дампити unrelated helpers в один гігантський файл.
- Уникати "misc" і "god utility" patternів.
- Не створювати extra файли, helper headers, wrappers або micro-abstractions для tiny technical conveniences, коли код cleanly належить в existing nearby файлі.
- Уникати file-structure clutter. Splitити файли тільки коли separation має реальну архітектурну цінність, не тому що tiny helper технічно може жити в іншому місці.
- Коли система splitиться на components — закінчити ownership split. Не залишати один component як disguised God object, що все ще володіє runtime state і decision flow усіх інших.
- Перед додаванням helper-а — спитати, чи він зменшує реальну складність, чи тільки приховує direct call. Якщо тільки приховує — не додавати.

## GAS-First Rules

GAS — owner combat-facing runtime logic. Це обов'язково; не build-ати паралельну combat framework.

- Characters, що беруть участь у combat, експонують `RN_AbilitySystemComponent` через clear ownership model.
- Shared combat state протікає через `Attributes`, `GameplayEffects` і `GameplayTags`. Не розкидати ad-hoc booleans (`bIsAttacking`, `bCanDodge`, `bIsInCombat`) по unrelated класам.
- Ability grants — data-driven через ability sets (DataAssets). Не hardcode-ити `GiveAbility` calls.
- Melee, interaction, dodge, hit reaction, death gating і progression hooks проектуються щоб співіснувати з GAS, не bypass-ити його.
- Якщо системі потрібна combat-state awareness — префер `GameplayTag` queries або attribute reads, а не brittle class-to-class assumptions.
- Attribute Sets — domain-focused (`RN_HealthAttributeSet`, `RN_StaminaAttributeSet`). Не накопичувати unrelated stats в один monster attribute set.
- GameplayTags — registered through `Config/Tags`, не через runtime string literals.
- Ability cooldowns і costs — через `GameplayEffect` cooldown/cost classes, не custom timer logic в ability body.
- **RN (legacy)** контекст: не закладати prediction logic, replication mode polling, server/client RPC structure в нові RN ability/attribute класи — RN frozen single-player.
- **RNC (active)** контекст: prediction, replication mode (Mixed / Minimal / Full), `FGameplayAbilityActorInfo` server/client logic, `CanActivateAbility` server-side gating, `Activate*Local*` vs server-confirmed result — частина проектного контракту. Детально — `Docs/Architecture/GAS_Multiplayer.md`.

## Mover Locomotion Rules

`Mover` — runtime locomotion engine. Поважати його pipeline.

- Player і gameplay character locomotion проходить через `Mover` components (`CharacterMoverComponent` + `MoverNetworkPredictionLiaisonComponent` backend) — **не** через legacy `Character Movement Component`.
- `MoverNetworkPredictionLiaisonComponent` — backend liaison Mover-стека для network prediction. У **RN (legacy)** воно фактично не задіюється — single-player snapshot. У **RNC (active)** воно — реальний network prediction layer для cooperative gameplay, з pending validation pass (`Docs/Multiplayer/Mover_Network_Validation.md`). Не reintroduce-ити RN-era "no replication code" rule у RNC через цей компонент: replication для locomotion у RNC — legitimate і очікувана.
- Locomotion intent і gait — через input producer structs (project-defined custom inputs поряд з `CharacterDefaultInputs`), не через ad-hoc state на pawn-і.
- Animation, camera і gameplay-системи, яким треба locomotion state, читають його з `Mover` (`_PostSim` data), не з pre-sim authoring values.
- Movement modes (`OnGround`, `InAir`, `Sliding`, `Traversing`, `Falling`) — project-mapped через `MovementModeMap`, щоб gameplay code користувався стабільними enum semantics незалежно від `Mover` mode-name strings.
- Project-owned input layer (`URN_InputConfig`, `ARN_PlayerControllerGameplay`) пушить intent в pawn-local input state; pawn `ProduceInput` flow транслює це в `Mover` input structs.
- `RN_PlayerCharacter_Mover` — canonical mover-driven pawn для gameplay.
- Donor `SandboxCharacter_Mover` і `IMC_Sandbox` — reference only (див. `RN/MIGRATION_SandboxCharacter_Mover.md`). Не копіювати їх patterns без явної project-owner команди.
- Тримати чисті boundaries: `input → pawn input state → Mover input structs → Mover simulation → _PostSim consumers`.

## UI Ownership and Common UI + MVVM

Common UI — required UI framework. MVVM — required pattern для menu-архітектури. UI ownership — найчастіше порушувана зона; правила нижче review-blocking.

### Layer Ownership
- UI mounting (viewport push, layer routing, menu stack operations) — у HUD/UI layer (`RNUIRuntime`).
- Gameplay controllers (`ARN_PlayerControllerGameplay`) і gameplay components не володіють Widget Blueprint класами, widget instances, mount/unmount logic, menu stack operations.
- Gameplay code експонує state, events або view models — і тільки.
- ViewModels (`RN_ViewModelPlayerHUD` і подібні) — owner menu-facing state і transformation logic. Widgets — focused на presentation і interaction wiring.
- Якщо менюшний widget починає накопичувати domain logic, save orchestration або gameplay-state querying — це ownership violation, реструктурувати у ViewModel або supporting system.

### Widget Layout
- Widget Blueprints не використовують `Canvas Panel` як root або wrapper. Default — найменша семантична панель: `Overlay`, `Border`, `Vertical Box`, `Horizontal Box`, `Size Box` або CommonUI containers.
- `Canvas Panel` дозволено тільки коли widget реально потребує абсолютного позиціонування, anchor-based screen placement або explicit layered composition.

### Forbidden
- Не вводити паралельний UI framework, ad-hoc screen stacks або widget-management patterns, що конкурують з Common UI.
- Не будувати menu без MVVM або пушити menu state і transformation logic напряму в widget code.
- Не розкидати UI ownership по випадковим gameplay actorам, коли той самий flow має жити в coherent UI layer.

## Composition, Components, and System Boundaries

Префер composition-first архітектуру.

- Використовувати Unreal component model для збору gameplay behavior з focused pieces.
- Тримати actor classes тонкими, коли behavior може жити в dedicated components.
- Managers і subsystems — для lifecycle-aware coordination, registration, caching і cross-system orchestration (наприклад `RN_AssetManager`, `RN_GameInstanceBase`).
- Не пушити global responsibilities в один gameplay actor тільки тому що до нього легко достукатись.
- Тримати чіткі boundaries між components, managers, subsystems і feature logic.
- Components, прикріплені до того ж actor, не знають один про одного напряму. Owning actor — orchestration root, що координує focused worker components.
- Якщо component потребує policy, settings або іншого component-results — маршрутизувати координацію через owning orchestrator, не створювати direct component-to-component knowledge.
- Actor-level bridge methods допустимі тільки коли вони встановлюють реальний orchestration boundary. Не reintroduce-ити wrapper noise, що bypass-ить це правило без додавання ownership value.
- Worker components не cache-ять broad копії actor-owned scene graph або resolved settings для convenience. Якщо worker потребує scene objects або execution parameters — orchestrator передає цей context у action.

Pattern usage:

- managers координують domain responsibility
- subsystems володіють engine-level або game-level service lifecycles
- components інкапсулюють attachable behavior

Не використовувати ці patterns як виправдання для централізації unrelated logic.

## Patterns and Construction

Use proven patterns коли вони покращують clarity, scaling, testability.

- Префер factories для controlled object/data construction, коли creation logic non-trivial.
- Префер orchestrators для multi-step flows, що координують кілька дрібних services.
- Префер constructors і initialization paths, що чітко виражають required dependencies.
- Templates — там, де вони реально знімають duplication і зберігають readability.
- Patterns обираються для зменшення coupling і покращення maintainability, не для додавання ceremony.

Practical:

- factory знімає creation duplication — використовувати
- orchestrator прояснює flow ownership — використовувати
- template робить код важче читати — не форсити

## Worker, Orchestrator, and Utility Rules

Високорівневе правило для систем, які split-яться на orchestration, execution і helper layers.

### Core Principles
- Orchestrator — єдиний layer, що контролює workflow.
- Orchestrator відповідає за execution order, validation, sequencing і worker coordination.
- Orchestrator викликає workers і збирає їх results.
- Orchestrator не імплементує low-level execution logic.
- Workers — isolated execution units.
- Workers виконують одну відповідальність.
- Workers можуть мати domain logic і internal runtime state.
- Workers не контролюють overall system flow.
- Workers не знають про orchestrator.
- Workers не знають про інших workers.

### Communication Rules
- Worker output — return values, delegates, events або interfaces.
- Ніколи не маршрутизувати worker communication через direct references назад до orchestrator.
- Ніколи не дозволяти прямі виклики `Worker -> Orchestrator` або `Worker -> Worker`.
- Якщо одному worker-у потрібен result іншого — orchestrator координує цей handoff.

### Utility Rules
- Utilities — для reusable stateless helper logic.
- Utilities надають static helper functions, math helpers, validation helpers або pure helper libraries.
- Utilities — stateless.
- Utilities не містять gameplay flow.
- Utilities не містять domain ownership або hidden system dependencies.

### Forbidden Patterns
- Worker викликає іншого worker напряму.
- Worker дістає orchestrator через `GetOwner`, global lookup, subsystem lookup, manager lookup або equivalent hidden dependency.
- Utility типи містять gameplay flow, runtime orchestration або owned state.
- Змішування orchestration і execution responsibilities в одному класі.

### Required Architecture Shape
`Orchestrator -> Workers -> Utilities`

- Workers — independent і unaware один одного.
- Utilities — stateless і unaware system flow.

### Unreal Mapping
- Orchestrators — actors, managers, subsystems або інші explicit flow-owning класи.
- Workers — focused components, handlers, action units, attachable execution pieces.
- Utilities — Blueprint function libraries, static helpers, math helpers, pure domain helper типи.

Goals:

- low coupling
- high modularity
- replaceable systems
- predictable execution flow

## Module Boundaries

Runtime modules мають чіткі owner-domains. Не розмивати їх. RN legacy modules і RNC active modules розмежовуються per project; правила нижче — owner для RN legacy. RNC module layer canonical — `Docs/Architecture/Module_Layer_Architecture.md`.

### `RN`
Owner core gameplay-моделі: `RN_PlayerControllerGameplay`, `RN_PlayerCharacter_Mover`, `RN_GameInstanceBase`, `RN_AssetManager`, base game mode logic. Цей модуль — orchestration layer. Не накопичувати тут UI mounting, GAS-internal механіку або audio dispatch.

### `RNAbilityRuntime`
Owner GAS layer: `RN_AbilitySystemComponent`, attribute sets (`RN_HealthAttributeSet`, `RN_StaminaAttributeSet`), gameplay abilities (`RN_GA_Jump`, `RN_GA_Sprint`, `RN_GA_MeleeAttack`), ability sets, ability action router. Не плодити тут UI, audio, або не-GAS gameplay actorів.

### `RNUIRuntime`
Owner Common UI + MVVM layer: ViewModels (`RN_ViewModelPlayerHUD`), HUD widgets (`RN_WidgetGameplayHUDRoot`, `RN_WidgetGameplayVitals`), in-game menu (`RN_WidgetInGameMenu`). Виключно UI ownership і presentation orchestration. Не leakати сюди GAS-логіку, gameplay actor reach-через-singleton, save-flow.

### `RNAudioRuntime`
Owner audio layer, готовий під Wwise-bound dispatch. Не вводити сюди UI, gameplay actor logic або combat-rules.

### Cross-Module Rules
- Кожна `_API` macro застосовується до публічних типів cross-module surface.
- Module dependencies в `Build.cs` — мінімальні і явні. Не додавати dependency "про всяк випадок".
- Якщо клас не має очевидного owner-модуля — це сигнал, що класифікація системи незавершена. Зупинитись і обговорити placement з project-owner.

### No Hidden Module Coupling
Заборонено використовувати hardcoded class paths, asset paths, module script paths або string-based references у C++ коді для обходу compile-time залежностей між runtime модулями.

Заборонені приклади:

- `FSoftClassPath(TEXT("/Script/SomeModule.SomeClass"))`
- `StaticLoadClass(...)` з hardcoded path
- `LoadObject(...)` з hardcoded path
- `TryLoadClass(...)` з hardcoded path
- `ConstructorHelpers::FClassFinder` з прямим path у системному C++ коді
- будь-які string path references на конкретні класи з іншого runtime-модуля

**Чому**: такі рішення ховають залежність, ламаються при rename/refactor, не дають compile-time перевірки, створюють неявний runtime coupling і маскують архітектурну проблему. Якщо модуль `A` пише string-path на клас з модуля `B` — це означає що `A` усе одно знає про `B`, тільки робить це невидимо для UBT, code search, IDE-навігації і refactor-tooling. Це гірше за чесний `Build.cs`-dependency, бо проблему не видно у dependency-графі.

**Правильний підхід**:

- залежності між runtime модулями мають проходити через інтерфейси, абстрактні базові класи, data/config layer або composition root
- конкретні класи з інших модулів призначаються через Blueprint defaults, Project Settings, DataAsset, Primary Asset, Game Feature Data або map override
- якщо модуль не має залежати від іншого модуля, він не повинен знати його конкретні class names, module names або asset paths навіть через string
- C++ код модуля містить тільки власні типи, engine types або типи з явно дозволених lower-level/shared модулів

**Soft references — окреме правило**: `TSoftClassPtr`/`TSoftObjectPtr` допустимі тільки як `UPROPERTY` у data/config layer (DataAssets, settings, GameMode UPROPERTY-полях editable у Blueprint), Primary Asset references або equivalent designer-tunable конфігурації. Hardcoded soft path constants у C++ для обходу модульних залежностей — заборонені нарівні з hard string paths.

**Композиція замість path-strings**: якщо C++ потребує "знати" про клас із вищого/паралельного модуля — це сигнал, що або (а) клас treба перенести у lower-level модуль, або (б) розв'язати залежність через інтерфейс/UPROPERTY/composition root, або (в) делегувати призначення на Blueprint/config layer без participation C++.

## Blueprint Rules

- Blueprints — readable, modular, production-ready.
- Уникати гігантських event graph-ів і spaghetti logic.
- Extract повторювану Blueprint логіку у функції, components, libraries або C++ support-класи.
- Коментарі — пояснюють intent, не очевидну node behavior.
- Не створювати приховані hard-reference chains через Blueprint defaults.
- Префер Blueprint класи, що споживають data assets, замість Blueprint, що encode-ить gameplay rules в node graph.
- Widget Blueprints дотримуються UI Ownership rules (див. вище): no `Canvas Panel` root, no gameplay logic, no direct gameplay-state querying.

## Logging and Diagnostics

Logging — частина implementation contract.

Пріоритет логів:

- startup і shutdown paths
- procedural generation (PCG, runtime spawn flows)
- item creation і item resolution
- asset loading і asset lookup (особливо `RN_AssetManager` flows)
- save/load flows
- failure paths і fallback branches
- GAS ability activation, cooldown, cost denial paths
- Mover input → simulation → `_PostSim` boundary, особливо при міграції зі donor pawn-у

Logging характеристики:

- meaningful
- scoped (project-wide log category convention `LogRN`, `LogRNAbility`, `LogRNUI`, `LogRNAudio` і feature-specific subcategories)
- легко filter-абельне
- без spam-у в tight loops, якщо явно не gated debug-категорією

Implementation contract:

- якщо новий gameplay flow має initialization, registration, fallback behavior, missing dependency cases або invalid setup handling — логи додаються одразу
- не postpone логи в "later cleanup pass"
- якщо логів немає — задача не завершена

## Review Checklist

Перед submit перевірити (загальне для RN + RNC):

- зміна слідує SOLID principles
- не введено duplicate logic
- не додано unnecessary hard references
- data-driven configuration використана де practical
- shared enums винесені в `*_<Domain>Types.h` коли reused
- рішення не bypass-ить asset manager стратегію (`RN_AssetManager` для RN; RNC asset manager — окремий або підкласований за `Docs/Architecture/Module_Layer_Architecture.md`)
- pointer ownership explicit і appropriate
- коментарі пояснюють intent там, де код non-obvious
- логи існують для важливих runtime і failure paths
- GAS — використовується для combat-facing logic; нема паралельного combat framework
- Mover — використовується для gameplay locomotion; немає reintroduction CMC patterns
- UI ownership — gameplay не володіє widgets; `Canvas Panel` root відсутній
- Common UI + MVVM правильно розшарували state і presentation
- placement коду в правильному модулі: RN legacy donor — `RN`, `RNAbilityRuntime`, `RNUIRuntime`, `RNAudioRuntime`; RNC active — `RNCoreRuntime`, `RNGASRuntime`, `RNGameplayRuntime`, `RNUIRuntime`, `RNSessionRuntime` (per `Docs/Architecture/Module_Layer_Architecture.md`)
- production content в правильному root (`RN/Content/RowanNight` для RN; `RNC/Content/RNC` для RNC), не в корені `*/Content` або `*/Content/Developers`
- код зрозумілий наступному розробнику без додаткового verbal handoff

RN-specific (legacy snapshot):

- RN single-player discipline збережена (немає replication, RPCs, network flow у RN-only задачі)
- немає реінтродукції multiplayer flow у legacy RN snapshot без owner approval

RNC-specific (active multiplayer-first):

- Multiplayer / Authority Notes block присутній у task body
- кожна нова gameplay-система має explicit authority model (host-auth / player-owned / client-predicted / local-only / cosmetic-only)
- replicated state має `/** ... */` пояснення intent, lifetime, mutator
- UI / widget не мутує gameplay state напряму; mutation через authoritative path (host transaction для pickup/drop/transfer/equip; local profile дозволяє slot move, split, equip from owned items)
- combat damage / Global Host Quest / world state / interactable state (doors/chests) — host-authoritative
- inventory / equipment / character progression / Personal Player Quests — player-owned (local profile), replicate тільки what gameplay needs
- loot — per-player rolling (не shared); host validates pickup transaction
- enemy AI decisions — server-side
- shared world race conditions (одночасна interaction з door/chest) — оброблені server-arbitrated FIFO
- save/load impact описаний (host world save vs player profile save; trigger event)
- late join / disconnect / host leave behavior описаний (або явно `n/a` з reasoning); drop-in policy дотримана (no join during boss/cinematic/transition)
- listen server / dedicated server risks залогувані (dedicated server **не у MVP**)

## Preferred Workflow

### New Gameplay Feature
1. визначити domain-типи чисто
2. вирішити що живе в коді, а що в data
3. експонувати tunable data через assets або таблиці
4. тримати runtime logic модульним
5. уникати hard-coupling feature code до конкретних assets
6. extract повторювану helper logic рано
7. документувати assumptions через коментарі і логи

### Pre-Implementation
Перед стартом будь-якої non-trivial задачі:

1. прочитати `CLAUDE.md`, `STYLE.md`, `CONTRIBUTING.md`, `TECHNOLOGIES.md`, `SYSTEMS.md` (relevant секції)
2. прочитати relevant RN-Skill (`rn-rules-check`, `rn-docs-sync`)
3. прочитати relevant Confluence system page в `Docs/Confluence/`
4. проаналізувати поточний state коду, existing structure і prepared foundation
5. представити implementation options або proposed direction до написання коду
6. чекати project-owner confirmation перед переходом з analysis у implementation, якщо задача стосується архітектури, structure, file creation або non-trivial design рішень

### Git Commits
1. specific commit message, що відображає реально виконану роботу
2. detailed description у commit body, коли зміна non-trivial
3. summary main implementation areas, important behavioral changes, notable constraints або follow-up context
4. не використовувати vague messages або empty descriptions для meaningful gameplay/architecture/content-setup змін

### Source File Operations
Додавання, видалення, перейменування або переміщення Unreal C++ source files:

1. одразу після source-file зміни — regenerate Visual Studio project files
2. дефолтний і обов'язковий метод — `Scripts/GenerateProjectFiles.bat`, якщо project-owner явно не схвалив інший
3. перевірити, що нові файли видні в solution після regeneration
4. не припускати, що Visual Studio підхопить module changes автоматично

Створення нового C++ header або `.cpp`, або зміна директорії existing C++ source — завжди потребує `Scripts/GenerateProjectFiles.bat`.

### File Placement
Перед додаванням нових project files, classes, folders або modules:

1. перевірити intended placement проти `CONTRIBUTING.md`, `STYLE.md` і Unreal best practices
2. представити proposed location і reasoning project-owner-у
3. чекати explicit confirmation перед generating files або введенням нової source folder structure
4. не приймати структурні рішення unilaterally, навіть якщо proposal здається standard або low risk

### Documentation Creation
Перед створенням нових `.md` файлів, guides, notes, reports або project docs:

1. представити purpose, target location і expected contents project-owner-у
2. чекати explicit confirmation перед створенням файлу
3. не генерувати документацію проактивно тільки тому що це здається корисним
4. ставитись до doc creation так само як до структурних project changes — approval first, file second

Confluence-facing documentation слідує `Confluence.md` (UA body + EN headings, page types, заборона donor refs, no junior/senior classification) і, якщо це execution-facing canonical page, role-split delivery pack за `DELIVERY_PACK_STANDARD.md`.

### Repository Automation
Helper-скрипти і build automation:

1. `.bat` — default і preferred script format
2. не вводити PowerShell-скрипти, якщо project-owner явно не схвалив виняток
3. execution logs — в `Logs/<ScriptName>/<Timestamp>.log` (down to seconds)
4. log writing — частина script contract, не optional debug feature
5. явно warn-ити project-owner-а, коли script run failed, partial або produced warnings, що впливають на expected result

### Build Execution Policy
Full project/game builds are explicit-request only.

1. Do not run `Scripts/BuildProject.bat`, `Scripts/BuildDesignProject.bat`, packaging, cooking, staging, archiving, or `RunUAT BuildCookRun` proactively.
2. Run full build/package/cook flows only when the project owner explicitly asks for that build.
3. For C++ compile validation after code changes or merge-conflict resolution, prefer the lightweight editor target compile: `RNEditor Win64 Development` via Unreal `Build.bat`, or Visual Studio `Development Editor|Win64`.
4. Before running any compile/build command, state whether it is a lightweight editor compile or a full game build.

## What to Avoid

- hardcoded gameplay data, що повинна бути designer-owned
- direct asset references у foundational systems
- duplicated switch logic across класів
- giant all-knowing manager classes
- giant all-knowing actors
- mixed responsibilities всередині одного component
- utility files, що накопичують unrelated helpers
- enums, заховані в unrelated headers
- overloaded функції, що одночасно fetch, decide, mutate і broadcast
- getters або setters з прихованими side effects
- файли, що ростуть замість splitу за відповідальністю
- fragile fixes, що залежать від load order luck
- contributions, що працюють сьогодні і блокують scaling завтра
- паралельний combat framework, що дублює GAS
- legacy `Character Movement Component` patterns в gameplay pawn-ах (Mover-only)
- паралельний UI stack або ad-hoc screen management поза Common UI
- menu без MVVM або з gameplay logic в widgets
- `Canvas Panel` як widget root без реальної потреби
- gameplay controllers/components, що володіють widgets або менюшним стеком
- production контент у корені `*/Content` або `*/Content/Developers`
- посилання на donor (`UnrealShoBlia`, `rowanNight_OLD`, `DesignProject/`) у production-коді або Confluence-документації

RN-specific (legacy):

- replication, RPCs, network state, multiplayer flow control у нових RN-only змінах (RN frozen single-player)
- реінтродукція multiplayer code в RN snapshot без owner approval

RNC-specific (active multiplayer-first):

- singleplayer-only assumptions у RNC коді (`GetPlayerCharacter(0)` як authority, local-only damage resolution, UI direct gameplay state mutation, local-only enemy decisions, local-only world state mutation)
- inventory pickup/drop/transfer/equip без host transaction validation
- shared loot table з FIFO arbitration замість per-player loot rolling
- inventory implemented як fully MMO server-authoritative replicated copy (RNC inventory — largely local profile-driven; replicate тільки what gameplay needs)
- RPC-стилеві імена у public surface (`Server_PerformAttack` як public API замість authoritative method з RPC implementation behind)
- MMO-style assumptions (persistent shared world, global economy, full-server simulation для непотрібних дрібниць, large-scale shared zones, account-bound backend progression)
- blind copy-paste з RN у RNC без migration note і multiplayer review
- session/online layer torkає combat/AI/inventory/quest logic напряму (порушення modular layer architecture)
- two players можуть зіткнутись у race condition на shared world interactable (одна door, один chest) без server arbitration FIFO
- save flow без явного host/guest ownership model (host world save vs player profile save)
- replicated UPROPERTY без `/** ... */` коментаря intent + lifetime + mutator
- separate `RNCombatRuntime` module з parallel combat pipeline (combat ACTOR-level — у `RNGameplayRuntime`, GE-driven damage/effect helpers — у `RNGASRuntime`; combat не окремий module)
- `GameFeatures` pattern вводиться на ранній стадії (не використовується currently — Lyra не використовується як референс для архітектури)
- Lyra-style module decomposition, GameFeature-oriented split або marketplace-first plugin architecture для RNC MVP — не приймаються (different production shape; rule 4 у `Docs/Architecture/Module_Layer_Architecture.md`)
- Створення нового runtime module без доведеного production friction (substantial code volume + compile isolation win + stable API surface + team friction reports)
- drop-in join during boss / cinematic / critical transition (controlled coop policy — join тільки у safe gameplay state)
- dedicated server-only assumptions (dedicated **не у MVP**; architecture не блокує future support але active dev під listen server)

## Final Note

Контрибуції повинні відчуватись intentional, reusable і ready для зростаючого production codebase.

Якщо вибір між:

- швидкою реалізацією і стабільною системою
- локальним фіксом і reusable abstraction
- hard reference і data-driven lookup
- ad-hoc UI mount і Common UI + MVVM flow
- custom combat boolean і GAS attribute/tag
- legacy CMC pattern і Mover input flow
- client-side mutation і authoritative path (server validation + replicated state + UI refresh)
- blind copy-paste з RN і RNC migration з multiplayer review
- "буде швидше якщо local-only" і server-authoritative coop-safe implementation

— завжди обирати той варіант, що тримає проєкт maintainable at scale і безпечний у cooperative multiplayer.
