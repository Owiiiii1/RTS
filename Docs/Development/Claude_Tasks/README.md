# Claude Tasks — Execution Guide

Це єдиний entry point для AI-агента, що працює над GrimProtocol. Якщо ти агент і відкрив цей файл — **прочитай його повністю один раз**, далі дій по cursor. Цей файл містить inline всі hard rules, тому слідувати linked файлам не обов'язково крім випадків, коли треба deep reference.

---

## Cursor — NEXT Task

> Стан станом на 2026-08-20. Перед кожним новим slice агент **зобов'язаний** перевірити cursor: [`../DOCUMENTATION_INDEX.md`](../DOCUMENTATION_INDEX.md), [`../AI_Project_Log.md`](../AI_Project_Log.md), and the current [`MVP roadmap reconciliation`](../MVP_Roadmap_Reconciliation_Post_Building_Vitals.md). Validation selection: [`../Risk_Based_Development_Workflow.md`](../Risk_Based_Development_Workflow.md).

**Поточна фаза:** post-vitals factual MVP roadmap. The architecture/config cleanup phase is closed.
**Current: MVP_ROADMAP_RECONCILIATION_POST_VITALS_READY_FOR_REVIEW**.

**Status snapshot:**

| Area | Status |
|---|---|
| GP-S01 … GP-S42A | **DONE / on `main`** |
| **Unit Drop Nested Readiness** | **DONE / on `main`** @ `283297012c1cefe162028a7ba4166c02a81230cc` |
| **Cleanup Slice A** Settings visibility truth | **DONE / on `main`** @ `f38e803771261c60d865949c693a52a73fbcedb2` |
| **Cleanup Slice B** Dead overlap setting removal | **DONE / on `main`** @ `967e6ea3a5b81ddc1a2c19c4bfe292f5ef989507` |
| **Cleanup Slice C** Unit numeric compatibility | **DONE / on `main`** @ `47a220b480e455f1cf5dfb6ca0613c13cf760a53` |
| **Cleanup Slice D** Unit payload compatibility | **DONE / on `main`** @ `75b13fc193531170eb3d4c1eaf9ee3f736d1d160` |
| **Cleanup Slice G** Delivery timing ownership | **DONE / on `main`** @ `d2c1abcfcf4fe2f61ae00793294c0cc31919cd65` |
| **Building procurement + payload ownership** | **FINALIZED READY FOR MERGE** — [`GP-Building-Procurement-Payload-Ownership.md`](GP-Building-Procurement-Payload-Ownership.md) — **NOT MERGED** |
| **Cleanup Slice H — Building vitals definition ownership** | **DONE / on `main`** @ `b7e391a636749173c445f7994a41daf3c18ba902` |
| **Post-vitals MVP roadmap reconciliation** | **READY FOR REVIEW** — [`MVP_Roadmap_Reconciliation_Post_Building_Vitals.md`](../MVP_Roadmap_Reconciliation_Post_Building_Vitals.md) — **NOT MERGED** |

**Exactly one NEXT production capability after this docs review:** three-state per-team **Fog of War runtime foundation**.

Do **not** start footprint/geometry cleanup; it is deferred pending building construction redesign.
Do **not** implement SWARM now. SWARM is separate from the RTS AI Opponent and is
**MVP — FINAL IMPLEMENTATION STAGE; DESIGN REVIEW REQUIRED BEFORE IMPLEMENTATION**.

### Drift Warning

Cursor must match [`../DOCUMENTATION_INDEX.md`](../DOCUMENTATION_INDEX.md) + `AI_Project_Log.md`. One slice = one PR.

---

## Execution Protocol (Hard Rules)

Це **non-negotiable** правила для виконання задач. Порушення = review-blocking.

1. **One task at a time.** Не відкривати другу задачу, доки поточна не має виконаний `Stop Condition`.
2. **Read full task file before acting.** Goal, Scope, Out of Scope, Required Skill Pass, Acceptance Criteria, Stop Condition — все.
3. **No code without `Code Allowed: Yes`.** Design tasks (`GP-NNNN`) — НІКОЛИ не пишуть C++ gameplay code. Code slices (`GP-S##`) — пишуть тільки після approval попереднього slice.
4. **Stop on Stop Condition.** Не переходити автоматично до наступного. Зачекати explicit approval.
5. **Pillar Compliance Rule.** Будь-яка gameplay-facing task проходить `gp-mechanics-validator` проти `GDD/01_Game_Pillars.md`. Pillar Violation verdict — review-blocking.
6. **Pillar 8 — 5-Question MVP Gate.** Якщо slice вводить нову mechanic — 5-question gate з [`../Slice_Template.md`](../Slice_Template.md) §Pillar 8. "Ні" хоча б на декілька → slice rejected.
7. **Open Questions → stop.** Якщо task відкриває нові питання поза scope — фіксувати у `Open Questions`, **зупинятись**, не вгадувати.
8. **One slice = one PR.** Не bundling.

---

## Inline Hard Rules Summary

Зведено з `CONTRIBUTING.md`, `STYLE.md`, `Coding_Rules.md`, прийнятих ADR і memory rules. Список **не вичерпний**, але покриває ~90% review-blocking порушень. Для повного контексту — слідуй посиланням у [References](#references).

### Architecture Hard Bans (review-blocking)

- Жодного Lyra / Lyra Experience System / Lyra-style modular gameplay injections.
- Жодних enterprise-style abstractions (manager-of-managers, abstract factory hell, ECS-style без production need).
- Жодних generic frameworks "на майбутнє".
- Жодного додаткового runtime module поза `GPRuntime` / `GPGASRuntime` / `GPUIRuntime` без ADR.
- Жодного нового subsystem без ADR (виняток — задокументовані `UGP_SessionSubsystem`, `UGP_MatchAssetLoader`).
- Жодних manager-класів без single responsibility і documented owner.
- Жодних deep inheritance chains. Composition over inheritance — за замовчуванням.
- Жодного дублювання gameplay state поза GAS (health / resources / cooldowns / modifiers — тільки через AttributeSets і GameplayEffects).
- Жодного hardcoded gameplay balance у C++ (balance живе у `UDataAsset`).

### Module Dependency Graph (одностороннє)

```
GPUIRuntime  →  GPRuntime  →  GPGASRuntime
                   ↑              ↓
                   └──────────────┘
```

- `GPGASRuntime` НЕ залежить від `GPRuntime`.
- `GPRuntime` НЕ залежить від `GPUIRuntime`.
- UI читає gameplay state тільки через ViewModels / interfaces.

### Multiplayer / Authority

- **Server-authoritative за замовчуванням.** Gameplay state, GAS state, resources, spawning, match state, validation, win/lose — на сервері.
- Client пише input intent (selection, command requests, camera). Жодних client-side gameplay calculations.
- `Server_*` RPC — input intent, обов'язково `WithValidation`.
- `Client_*` RPC — targeted UI feedback або cosmetic.
- `Multicast_*` — **тільки cosmetic** (death VFX, montage). Gameplay state — replicated properties + GAS, не multicast.
- Кожен `UPROPERTY(Replicated)` — у `GetLifetimeReplicatedProps` з explicit `DOREPLIFETIME_*` condition.
- RepNotify functions — `OnRep_<PropertyName>`.
- У header кожного RPC — single-line authority comment.

### GAS Discipline

- **У GAS живе:** resources, unit cap, modifiers (`UGP_PlayerAttributeSet`); health, armor, damage, cooldowns (`UGP_UnitAttributeSet`); cooldowns / costs / durations (`UGameplayEffect`); build / research / scan / abilities (`UGameplayAbility`); buffs / debuffs / flags (`FGameplayTag`).
- **НЕ у GAS:** movement (Movement Component), selection (local PlayerController), UI state (Widget), camera (CameraPawn).
- ASC на `AGP_PlayerState` для player-scoped attributes; ASC на `AGP_UnitBase` для unit-scoped attributes.
- AttributeSets replicated, з `GAMEPLAYATTRIBUTE_VALUE_INITTER` boilerplate.
- Abilities — explicit `EGameplayAbilityNetExecutionPolicy` (default `ServerInitiated`; `LocalPredicted` тільки коли responsiveness критичний і prediction validated).
- Effects — explicit replication mode (`Mixed` для player-driven, `Minimal` для AI-driven, `Full` тільки коли потрібно для co-op spectating).

### Tags

- Root namespace: `GP.*`.
- Native registration через `UGameplayTagsManager` у `FGPGameplayTags` singleton struct.
- **Жодних magic-strings.** `FGameplayTag::RequestGameplayTag(FName(TEXT("GP.Command.Move")))` у hot paths = review-blocking. Завжди `FGPGameplayTags::Get().Command_Move`.
- Designer-facing tags — через `.ini` (`Config/Tags/GP_GameplayTags.ini`), інженерні — native у `GPGASRuntime`.

### Data-Driven

- Будь-який gameplay object (unit / building / ability / command / resource / faction / research) спочатку проектується як `UDataAsset`, потім пишеться logic.
- Identity, Cost, Attributes, Tags, Allowed commands, Granted abilities, Modifiers, UI metadata, Faction ownership — у Data Asset.
- Stateful runtime data — НЕ у Data Asset (asset = immutable config).
- DataAsset placement: `/Game/GrimProtocol/DataAssets/<Domain>/DA_GP_<Domain>_<Specifier>.uasset`.

### Soft Refs (mandatory)

- **Усі контентні референси** (DataAssets, abilities, effects, meshes, textures) — `TSoftObjectPtr<T>` / `TSoftClassPtr<T>`.
- Завантаження — через Asset Manager async.
- Hard refs у review — блокер.
- CI grep: `grep -rEn 'TObjectPtr<U.*_Definition>|TSubclassOf<UGameplay(Ability|Effect)>' GP/Source/` → 0 outside `Transient` cached resolvers.

### UI Framework (Common UI + MVVM mandatory)

- Common UI + MVVM (`ModelViewViewModel`) — обов'язково.
- **Server updates ViewModels only.** Widgets bind to VMs, ніколи не запитують gameplay state напряму.
- CI grep: `grep -rEn '(GetNumericAttribute|FindComponentByClass)' GP/Source/GPUIRuntime/` → 0.
- `AddToViewport` дозволено тільки для HUD root.

### Blueprint Philosophy

**BP дозволений:** UI assembly (`WBP_GP_*`), Editor utilities, presentation actors, BP-нащадки abstract C++ класів (thin), DataAsset tuning у редакторі.

**BP заборонений:** gameplay authority logic, gameplay calculations (damage / cost / validation / modifiers), GAS ability implementation, replication setup / RPC declarations, critical gameplay systems (match flow, command dispatch, resource transactions).

Правило: якщо у Blueprint є `If HasAuthority` — це треба переписати на C++.

### Naming Quick Reference

| Element | Prefix | Example |
|---|---|---|
| Actor / Pawn / Character | `AGP_*` | `AGP_PlayerController` |
| UObject / Component | `UGP_*` (`UGP_*Component`) | `UGP_SelectionComponent` |
| Interface | `IGP_*` + `UGP_*` | `IGP_Selectable` + `UGP_Selectable` |
| Struct | `FGP_*` | `FGP_CommandRequest` |
| Enum | `EGP_*` | `EGP_MatchState` |
| Subsystem | `UGP_*Subsystem` | `UGP_SessionSubsystem` |
| Data Asset | `DA_GP_<Domain>_*` | `DA_GP_Unit_Worker` |
| Widget BP | `WBP_GP_*` | `WBP_GP_HUD_Match` |
| BP child | `BP_GP_*` | `BP_GP_Worker` |
| Log Category | `LogGP*` | `LogGP`, `LogGPGAS`, `LogGPNet`, `LogGPUI` |
| GameplayTag root | `GP.*` | `GP.Command.Move` |

Member vars: `PascalCase` без префіксу (`Health`, `MaxHealth`). Boolean: `bPascalCase` (`bIsAlive`). Macros: `SCREAMING_SNAKE_CASE`. Усі identifier-и англійською. Повна таблиця — [`../Naming_Conventions.md`](../Naming_Conventions.md).

### File / Code Discipline

- `#pragma once` обов'язково.
- `*.generated.h` — завжди останнім include.
- Forward declare у `.h`, повний `#include` у `.cpp`.
- `Engine.h`, `EngineUtils.h`, `UnrealEd.h` — заборонені у runtime headers.
- One class declaration per `.h`, one class implementation per `.cpp`.
- `UPROPERTY` без `Category` — review-blocking. Format: `"GP|<Domain>"`.
- `EditAnywhere` тільки коли реально треба per-instance edit; інакше `EditDefaultsOnly`.
- `BlueprintReadOnly` за замовчуванням.
- `TObjectPtr<T>` для UPROPERTY pointers (UE5). Raw `T*` заборонено для UPROPERTY у new code.
- Soft cap: ~1000 рядків на файл. Component > 500 рядків — split.

### Language

- Код / identifier-и / asset names / logs / commit messages / EN source localization — англійською.
- Body text technical docs — українською.
- Markdown headings — англійською.
- Жодних українських коментарів у production `.h` / `.cpp`.

---

## Skill Routing Matrix

Що запускати для якого типу задачі. Скілзи з [`../../SKILLS/README.md`](../../SKILLS/README.md).

### Design Task (`GP-NNNN_*.md`)

| Тип design task | Skills (у порядку) |
|---|---|
| Нова RTS mechanic / command / loop / unit / building / resource | `game-design-framework` → `gp-mechanics-validator` → `documentation-knowledge-manager` (для запису у GDD/TDD) |
| Pillar / scope / win condition / meta-feature design | `game-design-framework` → `gp-mechanics-validator` |
| Documentation sync / navigation / cross-ref / rename | `documentation-knowledge-manager` |
| Architecture proposal / module / Build.cs | `ue5-architecture` → `documentation-knowledge-manager` (для запису у TDD / ADR) |

### Code Slice (`GP-S##_*.md`)

| Slice domain | Skills (у порядку) |
|---|---|
| Module scaffolds / Build.cs | `ue5-architecture` |
| GameplayTags / AttributeSets / ASC / Abilities / Effects / MMC | `ue5-cpp-gameplay` (для C++ написання) + sanity check на GAS rules вище |
| GameMode / GameState / Match flow / Asset loader | `ue5-cpp-gameplay` + `ue5-save-load-replication` |
| CameraPawn / PlayerController / Enhanced Input | `ue5-cpp-gameplay` + `ue5-blueprint-workflow` (якщо input bindings) |
| Selection / Commands / Movement / Combat | `ue5-cpp-gameplay` + `ue5-save-load-replication` (replication / RPC) |
| Worker / Resources / Mining / Container System | `ue5-cpp-gameplay` + `gp-mechanics-validator` (на gameplay fit) |
| Buildings / Orbital Drops / Wall / Build Grid | `ue5-cpp-gameplay` + `ue5-world-interaction` (interaction / overlap) |
| UI Foundation / FoW / HUD / Widgets | `ue5-ui-umg-slate` (з MVVM constraint inline) |
| Steam matchmaking / Listen server | `ue5-save-load-replication` |
| Debug / regression / log triage | `ue5-debug-validation` → routing до domain skill за fault type |
| Performance / pre-package | `ue5-performance-packaging` |

### Mandatory Per Slice

- `gp-mechanics-validator` — для будь-якого slice, що вводить або змінює gameplay mechanic.
- Pillar 8 5-question gate (з [`../Slice_Template.md`](../Slice_Template.md)).
- CI grep gates (hard-ref, magic-string-tag, widget→ASC, balance, replication completeness).

---

## Task Type Contracts

### Design Task (`GP-NNNN`)

Pre-code spec work. **Code Allowed: No.** Output = section у TDD/GDD/ADR.

Required sections у task-файлі:

- Goal, Inputs, Code Allowed (No), Scope, Required Skill Pass, Deliverables, Validation, Stop Condition.
- Output (filled by agent after completion: links до created spec sections, key decisions, code-task follow-ups).

### Code-Implementation Slice (`GP-S##`)

Per-slice C++ writing. **Code Allowed: Yes** після approval попереднього slice. Структура — [`../Slice_Template.md`](../Slice_Template.md):

- Slice Group, Code Allowed, Depends On, Goal, Scope, Out of Scope, Required Skill Pass, Files Touched, Acceptance Criteria (з Pillar 8 5-question gate), Playtest / Validation Note, Risks / Edge Cases, Linked, Stop Condition.

**Granularity guardrails:**

| Property | Target |
|---|---|
| C++ classes added | ≤ 3 per slice |
| Files touched | ≤ 10 |
| Diff size | ≤ ~1000 LOC |
| Time on slice | ≤ 1 day (≤ 3 days для UI / Steam) |

---

## Pipelines

### Pipeline A — Design новой mechanic (idea → spec)

1. Owner формулює ідею.
2. `game-design-framework` — фільтр на player experience (5-component Relevance check).
3. `gp-mechanics-validator` — фільтр на GP pillars + production cost.
4. Якщо verdict позитивний → `documentation-knowledge-manager` записує у GDD / TDD / ADR.
5. Materialize відповідний code slice (`GP-S##`) у backlog. **Stop.**

### Pipeline B — Виконання code slice

1. Прочитати `GP-S##_*.md` повністю + Stop Condition попереднього slice.
2. Verify `Depends On` slice merged.
3. Перевірити `Required Skill Pass` (запустити skill mentally / або apply checklists inline).
4. Виконати тільки `Scope`. Не touching `Out of Scope`.
5. Створити branch `feature/gp-s##-<short-desc>`.
6. Atomic commits (один логічний крок = один commit).
7. Перед push: compile editor build, run PIE 2-player smoke session.
8. CI grep gates locally (hard-ref / magic-string / widget→ASC).
9. PR description per [`../Git_Workflow.md`](../Git_Workflow.md) template + slice augmentation.
10. **Stop. Await approval.**

### Pipeline C — Documentation update / sync

1. `documentation-knowledge-manager` — audit + targeted update plan.
2. Apply edits per plan.
3. Якщо документується нова mechanic — додатково запустити `gp-mechanics-validator`.
4. Update cross-refs GDD ↔ TDD ↔ ADR ↔ Development.

### Pipeline D — Bug / regression

1. `ue5-debug-validation` — minimal repro, log triage, fault domain classification.
2. Route до domain skill (C++ / BP / UI / replication / PCG / packaging).
3. Fix у власному `fix/<jira-ticket>-<short-desc>` branch.
4. Якщо fix впливає на acceptance criteria попередньої slice — додати follow-up task у backlog.

---

## Decision Tree — "що робити далі"

```
Що треба зробити?
├── Нова gameplay idea?
│   └── Pipeline A (design → spec → backlog)
├── Реалізувати наступний code slice?
│   └── Перевірити Cursor → Pipeline B
├── Оновити docs?
│   └── Pipeline C
├── Bug / regression?
│   └── Pipeline D
└── Не зрозуміло scope?
    └── Open Questions у task file → STOP → owner clarification
```

---

## Backlog Index

### Phase 0 — Orientation (done)

- [GP-0001 Read Canonical Rules](GP-0001_Read_Canonical_Rules.md)
- [GP-0002 Validate Documentation Map](GP-0002_Validate_Documentation_Map.md)

### Phase 1 — First Playable Design (done)

- [GP-0101 First Playable Match](GP-0101_First_Playable_Match.md)
- [GP-0102 Core Gameplay Loop](GP-0102_Core_Gameplay_Loop.md)

### Phase 2 — Player Control Systems (done)

- [GP-0201 RTS Camera](GP-0201_RTS_Camera.md) → `TDD/11_RTS_Camera`
- [GP-0202 Selection](GP-0202_Selection.md) → `TDD/04` §Selection Rules
- [GP-0203 Move Command](GP-0203_Move_Command.md) → `TDD/04` §Move Command Rules
- [GP-0204 Attack Command](GP-0204_Attack_Command.md) → `TDD/04` §Attack Command Rules

### Phase 3 — MVP Economy & Entities (done)

- [GP-0301 Main Base](GP-0301_Main_Base.md) → `TDD/06` + orbital building architecture
- [GP-0302 Worker Unit](GP-0302_Worker_Unit.md) → `TDD/05`
- [GP-0303 Resource Primary](GP-0303_Resource_Primary.md) → `TDD/07` + Container System
- [GP-0304 Logistics Hub (renamed)](GP-0304_Barracks.md) → `TDD/06` + Post-Pivot (rename pending GP-0802)
- [GP-0305 Wall](GP-0305_Wall.md) → `TDD/06` §Build Grid + Wall System (acquisition superseded by GP-0305R)
- [GP-0305R Wall Package Reconciliation](GP-0305R_Wall_Package_Reconciliation.md) → current Wall acquisition/deployment canon
- [GP-0306 AI Opponent (design)](GP-0306_AI_Opponent.md) → ADR-0008 + GP-S54..S56
- [GP-0307 Sell + Demolish](GP-0307_Sell_Demolish.md) → `TDD/06` §Sell + Demolish

### Phase 4 — UI & Feedback (done)

- [GP-0401 MVP HUD](GP-0401_MVP_HUD.md) → `TDD/12_UI_Architecture`
- [GP-0402 Feedback Pass](GP-0402_Feedback_Pass.md) → `TDD/12` §Feedback Matrix

### Phase 5 — Multiplayer MVP (done)

- [GP-0501 Steam Matchmaking MVP](GP-0501_Steam_Matchmaking_MVP.md) → `TDD/08`

### Phase 6 — Approval & Implementation Planning (done)

- [GP-0601 Architecture Proposal](GP-0601_Architecture_Proposal.md) → `TDD/13_Architecture_Proposal`
- [GP-0602 Implementation Slices](GP-0602_Implementation_Slices.md) → [`../Slice_Template.md`](../Slice_Template.md)

### Phase 6A — Foundation Code Slices (Slice 1) — DONE

- [GP-S01 Module Scaffolds](GP-S01_Module_Scaffolds.md) — **DONE**
- [GP-S02 Native Gameplay Tags](GP-S02_Native_Gameplay_Tags.md) — **DONE**
- [GP-S03 Attribute Sets](GP-S03_Attribute_Sets.md) — **DONE**
- [GP-S04 AbilitySystemComponent Subclass](GP-S04_AbilitySystemComponent_Subclass.md) — **DONE**
- [GP-S05 Damage Calculation MMC](GP-S05_Damage_Calculation_MMC.md) — **DONE**

### Phase 6A2 — Match Flow Code Slices (Slice 2)

- [GP-S06 Game State](GP-S06_Game_State.md) — **DONE**
- [GP-S07 Game Mode](GP-S07_Game_Mode.md) — **DONE**
- [GP-S08 Player Controller](GP-S08_Player_Controller.md) — **DONE**
- [GP-S09 Player State](GP-S09_Player_State.md) — **DONE**
- [GP-S10 Match Asset Loader](GP-S10_Match_Asset_Loader.md) — **DONE**
- [GP-S11 Lobby State](GP-S11_Lobby_State.md) — **DONE**
- [GP-S12 Camera Config Data Asset](GP-S12_Camera_Config_Data_Asset.md) — **DONE**
- GP-S13+ — not materialized

### Phase 6B — AI Opponent Code Slices (Slice 10, materialized, code not started)

- [GP-S54 AI PlayerController](GP-S54_AI_PlayerController.md)
- [GP-S55 AI Behavior DataAsset](GP-S55_AI_Behavior_Definition.md)
- [GP-S56 AI State Implementations](GP-S56_AI_State_Implementations.md)

### Phase 6C — Slices 2-9, 11-13 (materialize on demand)

Per `TDD/13_Architecture_Proposal.md` §Implementation Order. Materialize коли попередня slice merged:

- Slice 2 — Match Flow + Asset Loader (S06-S11)
- Slice 3 — Camera (S12-S15)
- Slice 4 — Selection + Smart Commands (S16-S19)
- Slice 5 — Movement (S20-S22)
- Slice 6 — Worker + Resources (S23-S28)
- Slice 7 — Combat (S29-S33)
- Slice 8 — Buildings + Orbital Drops + Wall + Grid (S34-S46)
- Slice 9 — UI Foundation + FoW (S47-S53)
- Slice 11 — Feedback Pass (S57-S60)
- Slice 12 — Steam MVP (S61-S64)
- Slice 13 — Match End + Polish (S65-S67)

### Phase 7 — Post-MVP / Pivot Cascade (scope defined, files pending)

Перелік у [`../Claude_Task_Backlog.md`](../Claude_Task_Backlog.md) §Phase 7.

---

## When Stuck

1. **Scope unclear?** → не вгадувати, написати `Open Questions` у task file, **Stop**, owner розрулює.
2. **Acceptance criterion не виконується?** → не "make green by disabling test", діагностувати root cause, fix або escalate.
3. **CI grep fails?** → це не false positive, це сигнал rules violation. Refactor.
4. **Slice росте за granularity?** → split slice. Зайва робота → окремий ticket.
5. **Pillar Compliance verdict = Violation?** → mechanic decompose або defer post-MVP. Не "просто implement quickly".

---

## References

- Engineering canon: [`/CONTRIBUTING.md`](../../../CONTRIBUTING.md), [`/STYLE.md`](../../../STYLE.md).
- Operational rules: [`../Coding_Rules.md`](../Coding_Rules.md), [`../Naming_Conventions.md`](../Naming_Conventions.md), [`../Git_Workflow.md`](../Git_Workflow.md), [`../External_Skills.md`](../External_Skills.md).
- Slice contract: [`../Slice_Template.md`](../Slice_Template.md).
- Backlog (detailed): [`../Claude_Task_Backlog.md`](../Claude_Task_Backlog.md).
- Stage-level plan: [`../Claude_Work_Plan.md`](../Claude_Work_Plan.md).
- Pillars: [`../../GDD/01_Game_Pillars.md`](../../GDD/01_Game_Pillars.md).
- Architecture Proposal: [`../../TDD/13_Architecture_Proposal.md`](../../TDD/13_Architecture_Proposal.md).
- ADRs: [`../../Architecture_Decisions/`](../../Architecture_Decisions/).
- Skills index: [`../../../SKILLS/README.md`](../../../SKILLS/README.md).
