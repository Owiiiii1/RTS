# ADR-0007 — Building-As-Pawn Architecture

## Status

`Draft` — pending owner approval.

> **Post-pivot note ([ADR-0009](ADR_0009_Orbital_Delivery_Pillar.md) Orbital Delivery).** The building-as-pawn decision below STANDS. But examples that reference local production/construction — `UGP_ProductionComponent`, `UGP_ConstructionComponent`, construction sites, `AGP_Barracks` — are **superseded by ADR-0009**: there is no local production/construction. Buildings arrive operational via orbital drop pods; `AGP_MainBase` carries `UGP_StorageComponent` (containers), `AGP_LogisticsHub` grants unit-cap/container bonuses. Read the class examples below as illustrating the inheritance pattern only, not the canonical building set.

## Context

GrimProtocol — UE 5.8.1 RTS, де gameplay objects поділяються на два broad-категорії:

- **Units** — mobile entities (workers, troopers, mob grunts) з health, attack, movement, GAS.
- **Buildings** — static entities (Main Base, Barracks, Defensive Turret, Ferronite Deposit) з health, production / construction / storage / defensive поведінкою.

Класична UE архітектура часто розділяє ці категорії на:

- `AGP_UnitBase : APawn` (mobile, з MovementComponent).
- `AGP_BuildingBase : AActor` (static, без movement).

Це створює дублювання інфраструктури: ASC ownership, health AttributeSet, gameplay tags, death state machine, destruction replication, targeting interaction. Кожна сторона ієрархії має власну boilerplate, але всі будівлі і юніти у GrimProtocol concept-wise — це **щось з HP, що можна виділити, атакувати, знищити**, і яке є first-class GAS actor.

Owner на 2026-05-16 явно вказав направлення: будівля — це pawn з базою юніта, що наслідується далі як building з ability building/upgrade.

## Decision

Усі gameplay-active entities з health, ASC, gameplay tags, і destruction lifecycle наслідуються від спільного абстрактного класу **`AGP_UnitBase : APawn`**.

```
AGP_UnitBase                  // shared: ASC, UGP_UnitAttributeSet, GAS tags, death state, destruction replication
   ├── AGP_MobileUnit         // adds: UGP_MovementComponent, mobile-specific input handling
   │     ├── AGP_Worker       // (BP child) attaches UGP_MiningComponent
   │     └── AGP_Trooper      // (BP child) attaches UGP_CombatComponent, UGP_TargetingComponent
   │
   └── AGP_BuildingBase       // adds: static placement constraints, build/destroy visual hooks
         ├── AGP_MainBase      // (via DA + thin BP) UGP_StorageComponent (containers) + drop-off / launch hooks
         ├── AGP_LogisticsHub  // grants +MaxUnits / +container cap; arrives via orbital drop
         ├── AGP_Turret        // UGP_TargetingComponent + UGP_CombatComponent (DefensiveTurret / WallTurret)
         └── AGP_ResourceNode  // (Ferronite Deposit) — exposes mining interface
```

Building-specific behavior — через **UActorComponents** на `AGP_BuildingBase` children, не через deep inheritance. Composition over inheritance залишається rule (per [`/CONTRIBUTING.md`](../../CONTRIBUTING.md) Component-First Philosophy).

Якщо клас потребує movement — наслідуйся від `AGP_MobileUnit`. Якщо static — від `AGP_BuildingBase`. Це **єдиний** дозволений split у MVP.

## Consequences

### Positive

- **Reuse інфраструктури:** ASC ownership, health AttributeSet, gameplay tags, replication setup, death state, destruction logic — реалізовані одного разу у `AGP_UnitBase`, не дублюються.
- **GAS-uniform behavior:** будівлі автоматично receive GameplayEffects, GameplayAbilities, GameplayTags як юніти. Mob aggro може цілити будівлі і юніти за єдиним interface.
- **Targeting uniformity:** `UGP_TargetingComponent` (на Trooper, Turret, AI) працює з усіма entities через `IGP_Targetable` interface на `AGP_UnitBase`, без окремої логіки для buildings.
- **Damage / death uniformity:** один code path для damage application, death state, destruction replication. Менше edge cases.
- **Component composition rule працює без винятків:** building-specific behavior — у components на `AGP_BuildingBase` children. Базові класи залишаються тонкими.

### Negative / Trade-Offs

- **APawn baseline overhead:** будівлі — це APawn, не AActor. APawn має додаткові members (controller binding, input handling slots). Більшість з них unused для buildings. У UE 5.8.1 overhead per APawn instance — низький, але присутній.
- **Conceptual confusion:** "Building is a Pawn" може заплутати нових contributors, що очікують AActor для статичних обʼєктів. Mitigation — explicit ADR (цей документ) + commentary у `AGP_BuildingBase.h`.
- **Controller risk:** APawn типово має `Controller` (PlayerController або AIController). Buildings не повинні мати controller. Required boilerplate — `AGP_BuildingBase` явно блокує controller possession у constructor.
- **Inheritance depth:** є 2 рівні (UnitBase → BuildingBase → конкретний building). Кожен level має чітку responsibility. Не дозволено додавати 3-й рівень inheritance — нова building variant — це новий sibling клас + components, не subclass of subclass.

### Edge Cases

- **Ferronite Deposit як AGP_BuildingBase:** ferronite deposit логічно — environment, не "building" у player sense. Architecturally — це AGP_BuildingBase child з spawn-on-map-load constraint (не buildable). UI може показувати deposit окремо від player buildings.
- **Building deployment (post-pivot):** немає construction site / construction phase. Будівля прибуває operational з orbital drop pod (`AGP_DropPod`) і одразу активна на landing point. Pre-pivot "constructing" state та `UGP_ConstructionComponent` — superseded per [ADR-0009](ADR_0009_Orbital_Delivery_Pillar.md).
- **Mob як AGP_MobileUnit:** mob units — той самий path, що і player units. Це конзистентно і дозволяє єдиний combat resolution.

## Alternatives Considered

| Alternative | Why Rejected |
| --- | --- |
| `AGP_BuildingBase : AActor` (separate from UnitBase) | Дублює ASC ownership boilerplate, health AttributeSet, GAS integration, replication setup. Два code paths для damage / death — джерело bugs. |
| Composition over inheritance — `AGP_GameplayActor` (single class з усіма components) | Втрачає type safety, ускладнює BP child creation, конфліктує з UE editor workflow (`APawn`-specific functionality у Place Actors panel, Possess flow, тощо). |
| `AGP_BuildingBase : AInfo` | AInfo — для non-rendering actors. Будівлі потребують mesh, collision, replication of transform. AInfo не підходить. |
| Pure interface-based (no inheritance, IGameplayEntity на arbitrary AActor) | Без shared base — нема місця для shared health logic, ASC ownership, destruction lifecycle. Дублювання у кожному implementor. |

## Implementation Constraints

- `AGP_BuildingBase` blocks controller possession: `bCanBeDamaged = true`, `AutoPossessAI = EAutoPossessAI::Disabled`, `bDontUseInheritedPawnRotation = true`.
- `AGP_BuildingBase` static collision і mesh setup — у constructor.
- Building-specific behavior **тільки** через `UActorComponent`:
  - `UGP_StorageComponent` — accept ferronite drop-off (containers) + launch-to-orbit pipeline.
  - `UGP_TargetingComponent` + `UGP_CombatComponent` — turret defensive behavior.
- Подальші sibling classes (`AGP_MainBase`, `AGP_LogisticsHub`, `AGP_Turret`, `AGP_ResourceNode`) — тонкі, тільки composition різниця. Жодної унікальної логіки у override-ах.
- **Removed (per [ADR-0009](ADR_0009_Orbital_Delivery_Pillar.md)):** `UGP_ConstructionComponent` (build progress) і `UGP_ProductionComponent` (produce units / sub-buildings) — локального виробництва/будівництва немає; усі assets прибувають orbital drop-ом.

## References

- Implementation TDD — [`../TDD/05_Unit_Architecture`](../TDD/05_Unit_Architecture.md), [`../TDD/06_Building_Architecture`](../TDD/06_Building_Architecture.md).
- Component-First rule — [`/CONTRIBUTING.md`](../../CONTRIBUTING.md) → Component-First Philosophy.
- Game pillars (Component-Driven Behavior #5) — [`../GDD/01_Game_Pillars`](../GDD/01_Game_Pillars.md).
- Building roster — [`../GDD/05_Buildings`](../GDD/05_Buildings.md).
- Unit roster — [`../GDD/04_Units`](../GDD/04_Units.md).
