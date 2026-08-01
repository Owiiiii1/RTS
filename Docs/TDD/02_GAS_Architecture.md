# GAS Architecture

## Core Principle

GAS — головне джерело **gameplay state** і **gameplay synchronization**. Усе, що міняється під час матчу і потребує network sync, проходить через GAS, якщо немає документованої причини інакше.

## ASC Ownership

### Player-Scoped ASC

`AGP_PlayerState` — owns player ASC.

- `UAbilitySystemComponent* AbilitySystemComponent` (standard UE class у MVP; custom subclass — деferred).
- Player ASC живе на PlayerState, бо PlayerState replicates до всіх clients і living довше за PlayerController (особливо при travel / respawn).
- Replication mode: `Mixed` (Mixed — predicted abilities + full state для owner; інші clients бачать тільки final state).

Attributes на player ASC — `UGP_PlayerAttributeSet`.

### Unit-Scoped ASC

`AGP_UnitBase` — owns unit ASC.

- `UAbilitySystemComponent* AbilitySystemComponent`.
- Replication mode: `Minimal` (only attributes + tags reflect to clients; ability execution server-side).

Attributes на unit ASC — `UGP_UnitAttributeSet`.

### Why Two ASCs

- Player-level economy і unit-cap — concern of player, не units.
- Unit health / damage — concern of unit, не player.
- Розділення спрощує scoping abilities і uniformity attribute lookup (`Resource` always lives on PlayerState, `Health` always lives on Unit).

## AttributeSets

### UGP_PlayerAttributeSet

```cpp
DECLARE_ATTRIBUTE_CAPTUREDEF(Resource);
DECLARE_ATTRIBUTE_CAPTUREDEF(MaxResource);
DECLARE_ATTRIBUTE_CAPTUREDEF(CurrentUnits);
DECLARE_ATTRIBUTE_CAPTUREDEF(MaxUnits);
DECLARE_ATTRIBUTE_CAPTUREDEF(GlobalUnitLimit);
DECLARE_ATTRIBUTE_CAPTUREDEF(BuildSpeedModifier);
DECLARE_ATTRIBUTE_CAPTUREDEF(MiningSpeedModifier);
DECLARE_ATTRIBUTE_CAPTUREDEF(RepairSpeedModifier);
DECLARE_ATTRIBUTE_CAPTUREDEF(ResearchSpeedModifier);
```

- `Resource`, `CurrentUnits` — current values, modified by GE.
- `MaxResource`, `MaxUnits`, `GlobalUnitLimit` — caps.
- `*SpeedModifier` — multiplier attributes (1.0 default).

Усі replicated (`UPROPERTY(ReplicatedUsing=OnRep_*)`), boilerplate `GAMEPLAYATTRIBUTE_VALUE_INITTER`.

### UGP_UnitAttributeSet

```cpp
DECLARE_ATTRIBUTE_CAPTUREDEF(Health);
DECLARE_ATTRIBUTE_CAPTUREDEF(MaxHealth);
DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
DECLARE_ATTRIBUTE_CAPTUREDEF(MaxArmor);
DECLARE_ATTRIBUTE_CAPTUREDEF(DamageResistance);
DECLARE_ATTRIBUTE_CAPTUREDEF(AttackCooldown);
DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalChance);
DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalMultiplier);
DECLARE_ATTRIBUTE_CAPTUREDEF(CaptureProgress);
```

- `Health`, `MaxHealth` — primary survivability.
- `Armor`, `MaxArmor` — physical mitigation.
- `DamageResistance` — multiplier (0..1, 1 = full immune).
- `AttackCooldown` — base cooldown.
- `CriticalChance`, `CriticalMultiplier` — для basic crit math.
- `CaptureProgress` — future capture mechanic; у MVP скоріш unused, але pre-allocated.

## Gameplay Effects

### Spend Effects (Orbital)

- `GE_GP_SpendOrbital` — Instant, Modifier: `OrbitalFerronite -= DropDef.Cost`. Застосовується server-side при прийнятому orbital drop order (per [ADR-0009](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md) / [`14_Orbital_Delivery`](14_Orbital_Delivery.md)). Один універсальний spend effect; вартість приходить через SetByCaller з `UGP_OrbitalDropDefinition.Cost`.
- `GE_GP_Cost_RepairTick` — Instant, Modifier: `OrbitalFerronite -= TickCost`. Worker repair tick (repair лишається у MVP).

Усі created як `UGameplayEffect` UAsset (BP-only data), captured attribute `OrbitalFerronite`. **Removed (pre-pivot):** `GE_GP_Cost_Resource_Worker/Trooper/Barracks` (локальний build/produce spend через single `Resource` pool) — локального виробництва/будівництва більше немає.

### Income Effects (Container Launch)

- `GE_GP_AddOrbital` — Instant, Modifier: `OrbitalFerronite += Volume × OrbitalConversionRate`. Fires коли container ships to orbit (LAUNCH), не на mining/drop-off.
- `GE_GP_AddScore` — Instant, Modifier: `FerroniteScore += Volume × ScoreConversionRate`. Same trigger; `FerroniteScore` — cumulative shipped victory score.

Worker drop-off на MainBase = `UGP_StorageComponent` container fill (Storage state mutation, **не** player-attribute GE), і піднімає `AGP_GameState.FerroniteThreatValue`. Container launch на орбіту знижує `FerroniteThreatValue`. **Removed (pre-pivot):** `GE_GP_Income_Standard` (income on mining tick).

### Unit Cap Effects

- `GE_GP_UnitCap_Plus5` — Duration: Infinite, Modifier: `MaxUnits += 5`. Applied коли **Logistics Hub** прибуває orbital drop-ом і стає operational; removed при destroy. (Pre-pivot: був прив'язаний до construction-complete Assembly Yard / Barracks.)

### Damage Effects

- `GE_GP_Damage_Basic` — Instant, magnitude calculated by `UGP_DamageCalculation`. Reads `Damage` from source, `Armor` і `DamageResistance` from target, outputs `Health` modifier.

### Cooldown Effects

- `GE_GP_Cooldown_Attack` — Duration: derived from `AttackCooldown` attribute. Grants tag `GP.Unit.State.AttackCooldown`.

## Gameplay Abilities

### Repair Ability

- `UGP_GameplayAbility_Repair` (C++ class у `GPGASRuntime/Public/Abilities/`).
- Granted via `DA_GP_Unit_Worker.GrantedAbilities` to Worker ASC.
- Activation: server-initiated (player issues `GP.Command.Repair` → CommandComponent routes to GA activation на own-team damaged target у range).
- Cost: `GE_GP_Cost_RepairTick` per tick (`OrbitalFerronite -= TickCost`; cost — TBD placeholder у DataAsset).
- Effect: heal target (`Health +=` per tick) поки target не full HP, не вийде з range, або команду не скасовано.

> **No Build / No Produce ability.** Per [ADR-0009 Orbital Delivery](../Architecture_Decisions/ADR_0009_Orbital_Delivery_Pillar.md), Worker не будує і не виробляє; усі non-initial units/buildings/walls прибувають з орбіти через `Server_RequestOrbitalDrop` → `UGP_OrbitalDeliverySubsystem` → `AGP_DropPod`. Removed pre-pivot abilities: `UGP_GameplayAbility_Build`, `UGP_GameplayAbility_ProduceUnit` (разом з `UGP_ConstructionComponent` / `UGP_ProductionComponent`).

### Attack Ability (Possibly не GA у MVP)

Атаки можуть бути реалізовані як **behavior tick** у `UGP_CombatComponent` (server-side), а не як GA. Це простіше у MVP. GE_GP_Damage_Basic застосовується напряму через ASC->ApplyGameplayEffectToTarget.

Фіналізація — playtest pass.

## Gameplay Tags

### Registration

`FGPGameplayTags` — singleton struct у `GPGASRuntime/Public/Tags/GPGameplayTags.h`.

```cpp
struct FGPGameplayTags
{
    static const FGPGameplayTags& Get();
    static void Register();

    // Match state
    FGameplayTag Match_State_Loading;
    FGameplayTag Match_State_WaitingForPlayers;
    FGameplayTag Match_State_Playing;
    FGameplayTag Match_State_Paused;
    FGameplayTag Match_State_Spectating;
    FGameplayTag Match_State_Finished;

    // Unit types
    FGameplayTag Unit_Type_Worker;
    FGameplayTag Unit_Type_Combat;
    FGameplayTag Unit_Type_Support;
    FGameplayTag Unit_Type_Building;

    // Commands
    FGameplayTag Command_Move;
    FGameplayTag Command_Attack;
    FGameplayTag Command_Mine;
    FGameplayTag Command_Build;
    FGameplayTag Command_Patrol;
    FGameplayTag Command_Stop;

    // Abilities
    FGameplayTag Ability_Build;
    FGameplayTag Ability_Research;
    FGameplayTag Ability_Scan;
    FGameplayTag Ability_LaunchResource;

    // Teams
    FGameplayTag Team_Neutral;
    FGameplayTag Team_Player_One;
    FGameplayTag Team_Player_Two;

private:
    void RegisterTags(UGameplayTagsManager& Manager);
};
```

- Registered у `FGPGASRuntimeModule::StartupModule`.
- Жодних magic-string tags в C++ коді — always `FGPGameplayTags::Get().Command_Move` (або analogue).

### Tag Hierarchy

```
GP.Match.State.*
GP.Unit.Type.*
GP.Unit.State.*  (e.g., GP.Unit.State.Moving, GP.Unit.State.Attacking, GP.Unit.State.Dead)
GP.Command.*
GP.Ability.*
GP.Resource.Type.*
GP.Resource.Node
GP.Team.*
GP.Building.Role.*
```

Розширення — через ADR або TDD update.

## Replication Discipline

### What replicates

- Усі attributes у `UGP_PlayerAttributeSet`, `UGP_UnitAttributeSet`.
- Active gameplay tags на ASC (handled by GAS automatically для `Mixed`/`Full` modes).
- Active GameplayEffects (через GAS replication).

### What does NOT replicate

- Ability prediction state (Mixed mode handles owner-only prediction).
- Cosmetic tags applied through `AddLooseGameplayTag` — server-only (use sparingly).

### Replication Mode Choice

- Player ASC: `Mixed` (player owns the most read-write surface).
- Unit ASC: `Minimal` (clients read final state, don't need prediction для simple RTS units у MVP).

## GAS Implementation Order (MVP)

1. Register `FGPGameplayTags`.
2. Implement `UGP_PlayerAttributeSet` (Resource, MaxUnits, CurrentUnits — мінімум для playable).
3. Implement `UGP_UnitAttributeSet` (Health, MaxHealth — мінімум для playable).
4. Create `GE_GP_Cost_Resource_*` UAssets.
5. Create `GE_GP_Income_Standard` UAsset.
6. Create `GE_GP_Damage_Basic` UAsset з `UGP_DamageCalculation`.
7. Implement `UGP_GameplayAbility_Build`.
8. Wire ASC у `AGP_PlayerState` і `AGP_UnitBase`.
9. Wire ability grants з `DA_GP_*.GrantedAbilities` на BeginPlay (server-only).

## References

- AttributeSet pattern — UE GAS sample, але з GrimProtocol naming.
- Cross-module access — [`01_Module_Architecture`](01_Module_Architecture.md).
- Tag namespace — `/CONTRIBUTING.md` → Gameplay Tag Philosophy.
- ADR — [`../Architecture_Decisions/ADR_0003_GAS_First`](../Architecture_Decisions/ADR_0003_GAS_First.md).
