// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitBase.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Combat/GPDamageApplication.h"
#include "Command/GPUnitCommand.h"
#include "Effects/GPGE_DamageBasic.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitCommandComponent.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGPUnitCommand, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogGPCombat, Log, All);

namespace GPUnitCommandPrivate
{
	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:
			return TEXT("None");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_Authority:
			return TEXT("Authority");
		default:
			return TEXT("Unknown");
		}
	}
}

namespace GPCombatPrivate
{
	static float SanitizeMaxHealth(float Value)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			return 0.0f;
		}
		return Value;
	}

	static float SanitizeHealth(float Value, float MaxHealth)
	{
		if (!FMath::IsFinite(Value))
		{
			return 0.0f;
		}
		return FMath::Clamp(Value, 0.0f, MaxHealth);
	}

	static float SanitizeNonNegative(float Value)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			return 0.0f;
		}
		return Value;
	}

	static float SanitizeResistance(float Value)
	{
		if (!FMath::IsFinite(Value))
		{
			return 0.0f;
		}
		return FMath::Clamp(Value, 0.0f, 1.0f);
	}

	static float SanitizeCooldown(float Value)
	{
		if (!FMath::IsFinite(Value) || Value < 0.05f)
		{
			return 0.05f;
		}
		return Value;
	}

	static float SanitizeRange(float Value)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			return 0.0f;
		}
		return Value;
	}

	static bool IsSameTeamHostileReject(const AGP_UnitBase& Source, const AGP_UnitBase& Target)
	{
		const int32 SourceTeam = Source.GetTeamId();
		const int32 TargetTeam = Target.GetTeamId();
		return SourceTeam >= 1 && TargetTeam >= 1 && SourceTeam == TargetTeam;
	}
}

AGP_UnitBase::AGP_UnitBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	UnitCommandComponent = CreateDefaultSubobject<UGP_UnitCommandComponent>(TEXT("UnitCommandComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UGP_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetProjectReplicationMode(EGameplayEffectReplicationMode::Mixed);

	UnitAttributeSet = CreateDefaultSubobject<UGP_UnitAttributeSet>(TEXT("UnitAttributeSet"));
}

UGP_UnitCommandComponent* AGP_UnitBase::GetUnitCommandComponent() const
{
	return UnitCommandComponent;
}

UAbilitySystemComponent* AGP_UnitBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UGP_AbilitySystemComponent* AGP_UnitBase::GetGPAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UGP_UnitAttributeSet* AGP_UnitBase::GetUnitAttributeSet() const
{
	return UnitAttributeSet;
}

bool AGP_UnitBase::IsDead() const
{
	return bIsDead;
}

FGP_OnUnitDied& AGP_UnitBase::OnUnitDied()
{
	return UnitDiedDelegate;
}

void AGP_UnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AGP_UnitBase, TeamId, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_UnitBase, bIsDead, COND_None, REPNOTIFY_OnChanged);
}

void AGP_UnitBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystemActorInfo();
	InitializeCombatAttributesIfNeeded();
}

void AGP_UnitBase::InitializeAbilitySystemActorInfo()
{
	if (AbilitySystemComponent == nullptr)
	{
		ensureMsgf(false, TEXT("AGP_UnitBase::InitializeAbilitySystemActorInfo: AbilitySystemComponent is null."));
		UE_LOG(LogGPCombat, Error,
			TEXT("GP UnitASCInitialized: Unit=%s Failed Reason=MissingASC"),
			*GetName());
		return;
	}

	const AActor* CurrentOwner = AbilitySystemComponent->GetOwnerActor();
	const AActor* CurrentAvatar = AbilitySystemComponent->GetAvatarActor();
	if (CurrentOwner == this && CurrentAvatar == this)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	const UWorld* World = GetWorld();
	UE_LOG(LogGPCombat, Log,
		TEXT("GP UnitASCInitialized: Unit=%s ASC=%s Role=%s NetMode=%s"),
		*GetName(),
		*AbilitySystemComponent->GetName(),
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(World != nullptr ? World->GetNetMode() : NM_MAX));
}

void AGP_UnitBase::InitializeCombatAttributesIfNeeded()
{
	if (!HasAuthority() || bCombatAttributesInitialized)
	{
		return;
	}

	if (AbilitySystemComponent == nullptr || UnitAttributeSet == nullptr)
	{
		UE_LOG(LogGPCombat, Error,
			TEXT("GP UnitCombatAttributesInitialized: Unit=%s Failed Reason=MissingASCOrAttributeSet"),
			*GetName());
		return;
	}

	if (AbilitySystemComponent->GetOwnerActor() != this || AbilitySystemComponent->GetAvatarActor() != this)
	{
		UE_LOG(LogGPCombat, Warning,
			TEXT("GP UnitCombatAttributesInitialized: Unit=%s Deferred Reason=ActorInfoNotReady"),
			*GetName());
		return;
	}

	const float MaxHealth = GPCombatPrivate::SanitizeMaxHealth(DefaultMaxHealth);
	const float Health = GPCombatPrivate::SanitizeHealth(DefaultHealth, MaxHealth);
	const float Damage = GPCombatPrivate::SanitizeNonNegative(DefaultDamage);
	const float Armor = GPCombatPrivate::SanitizeNonNegative(DefaultArmor);
	const float Resistance = GPCombatPrivate::SanitizeResistance(DefaultDamageResistance);
	const float Cooldown = GPCombatPrivate::SanitizeCooldown(DefaultAttackCooldown);
	const float Range = GPCombatPrivate::SanitizeRange(DefaultAttackRange);

	AbilitySystemComponent->SetNumericAttributeBase(UGP_UnitAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), Health);
	AbilitySystemComponent->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), Damage);
	AbilitySystemComponent->SetNumericAttributeBase(UGP_UnitAttributeSet::GetArmorAttribute(), Armor);
	AbilitySystemComponent->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageResistanceAttribute(), Resistance);
	AbilitySystemComponent->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackCooldownAttribute(), Cooldown);
	AbilitySystemComponent->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackRangeAttribute(), Range);

	bCombatAttributesInitialized = true;

	const UWorld* World = GetWorld();
	UE_LOG(LogGPCombat, Log,
		TEXT("GP UnitCombatAttributesInitialized: Unit=%s Health=%.2f MaxHealth=%.2f Damage=%.2f Armor=%.2f Resistance=%.2f Cooldown=%.2f AttackRange=%.2f Role=%s NetMode=%s"),
		*GetName(),
		Health,
		MaxHealth,
		Damage,
		Armor,
		Resistance,
		Cooldown,
		Range,
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(World != nullptr ? World->GetNetMode() : NM_MAX));
}

int32 AGP_UnitBase::GetTeamId() const
{
	return TeamId;
}

void AGP_UnitBase::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;
}

bool AGP_UnitBase::IsNeutral() const
{
	return TeamId == 0;
}

bool AGP_UnitBase::HasAssignedTeam() const
{
	return TeamId >= 0;
}

const FGameplayTagContainer& AGP_UnitBase::GetCapabilityTags() const
{
	return CapabilityTags;
}

bool AGP_UnitBase::HasCapabilityTag(FGameplayTag CapabilityTag) const
{
	if (!CapabilityTag.IsValid())
	{
		return false;
	}

	return CapabilityTags.HasTagExact(CapabilityTag);
}

bool AGP_UnitBase::IsGameplaySelectable() const
{
	if (bIsDead)
	{
		return false;
	}

	return HasCapabilityTag(FGPGameplayTags::Get().Capability_Selectable);
}

bool AGP_UnitBase::IsGameplayInspectable() const
{
	return HasCapabilityTag(FGPGameplayTags::Get().Capability_Inspectable);
}

bool AGP_UnitBase::IsSelectionTypeUnit() const
{
	return HasCapabilityTag(FGPGameplayTags::Get().Selection_Type_Unit);
}

bool AGP_UnitBase::IsSelectionTypeBuilding() const
{
	return HasCapabilityTag(FGPGameplayTags::Get().Selection_Type_Building);
}

void AGP_UnitBase::ReceiveCommand(const FGP_UnitCommand& Command)
{
	if (!HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World != nullptr ? World->GetNetMode() : NM_MAX;

	if (IsDead())
	{
		UE_LOG(LogGPUnitCommand, Log,
			TEXT("GP UnitCommandRejected: Unit=%s Team=%d Tag=%s Reason=UnitDead Role=%s NetMode=%s"),
			*GetName(),
			GetTeamId(),
			*Command.CommandTag.ToString(),
			GPUnitCommandPrivate::RoleToString(GetLocalRole()),
			GPUnitCommandPrivate::NetModeToString(NetMode));
		return;
	}

	UE_LOG(LogGPUnitCommand, Log,
		TEXT("GP UnitCommand Received: Unit=%s Team=%d Tag=%s TargetActor=%s Loc=%s Queue=%s Role=%s NetMode=%s"),
		*GetName(),
		GetTeamId(),
		*Command.CommandTag.ToString(),
		*GetNameSafe(Command.TargetActor),
		*Command.TargetLocation.ToCompactString(),
		Command.bQueue ? TEXT("true") : TEXT("false"),
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(NetMode));

	if (UnitCommandComponent)
	{
		UnitCommandComponent->HandleCommand(Command);
	}
	else
	{
		UE_LOG(LogGPUnitCommand, Warning,
			TEXT("GP UnitCommand ForwardFailed: Unit=%s Reason=MissingUnitCommandComponent Tag=%s"),
			*GetName(),
			*Command.CommandTag.ToString());
	}
}

bool AGP_UnitBase::ApplyDamageFromUnit(AGP_UnitBase* SourceUnit, FGP_DamageApplicationResult& OutResult)
{
	OutResult = FGP_DamageApplicationResult();

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World != nullptr ? World->GetNetMode() : NM_MAX;

	auto LogReject = [&](const TCHAR* Reason)
	{
		OutResult.RejectReason = Reason;
		UE_LOG(LogGPCombat, Log,
			TEXT("GP DamageApplyRejected: Source=%s Target=%s Reason=%s Role=%s NetMode=%s"),
			*GetNameSafe(SourceUnit),
			*GetName(),
			Reason,
			GPUnitCommandPrivate::RoleToString(GetLocalRole()),
			GPUnitCommandPrivate::NetModeToString(NetMode));
	};

	if (!HasAuthority())
	{
		LogReject(TEXT("NoAuthority"));
		return false;
	}

	if (SourceUnit == nullptr || !IsValid(SourceUnit))
	{
		LogReject(TEXT("InvalidSource"));
		return false;
	}

	if (!IsValid(this))
	{
		LogReject(TEXT("InvalidTarget"));
		return false;
	}

	if (SourceUnit == this)
	{
		LogReject(TEXT("SelfTarget"));
		return false;
	}

	if (SourceUnit->GetWorld() != GetWorld())
	{
		LogReject(TEXT("DifferentWorld"));
		return false;
	}

	if (SourceUnit->IsDead())
	{
		LogReject(TEXT("SourceDead"));
		return false;
	}

	if (IsDead())
	{
		LogReject(TEXT("TargetDead"));
		return false;
	}

	if (GPCombatPrivate::IsSameTeamHostileReject(*SourceUnit, *this))
	{
		LogReject(TEXT("FriendlyFire"));
		return false;
	}

	UGP_AbilitySystemComponent* SourceASC = SourceUnit->GetGPAbilitySystemComponent();
	UGP_AbilitySystemComponent* TargetASC = GetGPAbilitySystemComponent();
	if (SourceASC == nullptr || TargetASC == nullptr)
	{
		LogReject(TEXT("MissingASC"));
		return false;
	}

	if (SourceUnit->GetUnitAttributeSet() == nullptr || GetUnitAttributeSet() == nullptr)
	{
		LogReject(TEXT("MissingUnitAttributeSet"));
		return false;
	}

	const float SourceDamage = SourceUnit->GetUnitAttributeSet()->GetDamage();
	const float TargetArmor = GetUnitAttributeSet()->GetArmor();
	const float TargetResistance = GetUnitAttributeSet()->GetDamageResistance();

	UE_LOG(LogGPCombat, Log,
		TEXT("GP DamageApplyAttempt: Source=%s Target=%s RawDamage=%.2f Armor=%.2f Resistance=%.2f Role=%s NetMode=%s"),
		*SourceUnit->GetName(),
		*GetName(),
		SourceDamage,
		TargetArmor,
		TargetResistance,
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(NetMode));

	if (!GPDamageApplication::ApplyDamageEffect(
		SourceASC,
		TargetASC,
		UGP_GE_Damage_Basic::StaticClass(),
		OutResult))
	{
		LogReject(*OutResult.RejectReason);
		return false;
	}

	UE_LOG(LogGPCombat, Log,
		TEXT("GP DamageApplied: Source=%s Target=%s RawDamage=%.2f AppliedDamage=%.2f HealthBefore=%.2f HealthAfter=%.2f Armor=%.2f Resistance=%.2f IsDead=%s Role=%s NetMode=%s"),
		*SourceUnit->GetName(),
		*GetName(),
		OutResult.RawDamage,
		OutResult.FinalDamage,
		OutResult.HealthBefore,
		OutResult.HealthAfter,
		TargetArmor,
		TargetResistance,
		IsDead() ? TEXT("true") : TEXT("false"),
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(NetMode));

	return true;
}

void AGP_UnitBase::HandleGASDeath(const FGameplayEffectModCallbackData& Data)
{
	(void)Data;
	HandleDeathInternal();
}

void AGP_UnitBase::HandleDeathInternal()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDead || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	bIsDead = true;

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World != nullptr ? World->GetNetMode() : NM_MAX;
	const float Health = UnitAttributeSet != nullptr ? UnitAttributeSet->GetHealth() : 0.0f;

	UE_LOG(LogGPCombat, Log,
		TEXT("GP UnitDeathStarted: Unit=%s Team=%d Health=%.2f Role=%s NetMode=%s"),
		*GetName(),
		GetTeamId(),
		Health,
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(NetMode));

	if (AbilitySystemComponent != nullptr)
	{
		const FGameplayTag DeadTag = FGPGameplayTags::Get().Unit_State_Dead;
		if (DeadTag.IsValid() && !AbilitySystemComponent->HasMatchingGameplayTag(DeadTag))
		{
			AbilitySystemComponent->AddLooseGameplayTag(DeadTag, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	if (UnitCommandComponent != nullptr)
	{
		UnitCommandComponent->NotifyOwnerDied();
	}

	SetActorEnableCollision(false);

	UnitDiedDelegate.Broadcast(this);

	const float LifeSpan = FMath::IsFinite(DeadActorLifeSpan) ? FMath::Max(0.0f, DeadActorLifeSpan) : 0.0f;
	if (LifeSpan > 0.0f)
	{
		SetLifeSpan(LifeSpan);
	}

	UE_LOG(LogGPCombat, Log,
		TEXT("GP UnitDied: Unit=%s Team=%d Health=%.2f LifeSpan=%.2f IsDead=true Role=%s NetMode=%s"),
		*GetName(),
		GetTeamId(),
		Health,
		LifeSpan,
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(NetMode));
}

void AGP_UnitBase::ApplyClientDeadPresentation()
{
	SetActorEnableCollision(false);
}

void AGP_UnitBase::OnRep_IsDead()
{
	if (bIsDead)
	{
		ApplyClientDeadPresentation();
	}
}

void AGP_UnitBase::OnRep_TeamId()
{
}

#if !UE_BUILD_SHIPPING
namespace GPCombatConsolePrivate
{
	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		return GPUnitCommandPrivate::NetModeToString(NetMode);
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		return GPUnitCommandPrivate::RoleToString(Role);
	}

	static void ApplyCombatStats(
		AGP_UnitBase* Unit,
		float Health,
		float MaxHealth,
		float Damage,
		float Armor,
		float Resistance,
		float Cooldown,
		float Range)
	{
		if (Unit == nullptr || !Unit->HasAuthority())
		{
			return;
		}

		UGP_AbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent();
		if (ASC == nullptr)
		{
			return;
		}

		const float SanitizedMax = GPCombatPrivate::SanitizeMaxHealth(MaxHealth);
		const float SanitizedHealth = GPCombatPrivate::SanitizeHealth(Health, SanitizedMax);
		const float SanitizedDamage = GPCombatPrivate::SanitizeNonNegative(Damage);
		const float SanitizedArmor = GPCombatPrivate::SanitizeNonNegative(Armor);
		const float SanitizedRes = GPCombatPrivate::SanitizeResistance(Resistance);
		const float SanitizedCd = GPCombatPrivate::SanitizeCooldown(Cooldown);
		const float SanitizedRange = GPCombatPrivate::SanitizeRange(Range);

		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetMaxHealthAttribute(), SanitizedMax);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), SanitizedHealth);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), SanitizedDamage);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetArmorAttribute(), SanitizedArmor);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageResistanceAttribute(), SanitizedRes);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackCooldownAttribute(), SanitizedCd);
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetAttackRangeAttribute(), SanitizedRange);
	}

	static AGP_UnitBase* FindFirstAuthorityAliveUnit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (Unit != nullptr && Unit->HasAuthority() && !Unit->IsDead())
			{
				return Unit;
			}
		}

		return nullptr;
	}

	static AGP_UnitBase* FindFirstAuthorityUnit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (Unit != nullptr && Unit->HasAuthority())
			{
				return Unit;
			}
		}

		return nullptr;
	}

	static AGP_UnitBase* FindCombatSource(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (Unit != nullptr && Unit->HasAuthority() && !Unit->IsDead() && Unit->GetTeamId() >= 1)
			{
				return Unit;
			}
		}

		return nullptr;
	}

	static AGP_UnitBase* FindCombatTarget(UWorld* World, AGP_UnitBase* Source)
	{
		if (World == nullptr || Source == nullptr)
		{
			return nullptr;
		}

		AGP_UnitBase* NeutralFallback = nullptr;
		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (Unit == nullptr || Unit == Source || !Unit->HasAuthority() || Unit->IsDead())
			{
				continue;
			}

			if (Unit->GetWorld() != Source->GetWorld())
			{
				continue;
			}

			if (Unit->GetTeamId() != Source->GetTeamId())
			{
				if (Unit->GetTeamId() >= 1)
				{
					return Unit;
				}

				if (NeutralFallback == nullptr && Unit->GetTeamId() == 0)
				{
					NeutralFallback = Unit;
				}
			}
		}

		return NeutralFallback;
	}

	static void LogInspect(AGP_UnitBase* Unit)
	{
		if (Unit == nullptr)
		{
			UE_LOG(LogGPCombat, Warning, TEXT("GP Combat.Inspect: no authority unit found"));
			return;
		}

		const UGP_AbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent();
		const UGP_UnitAttributeSet* Attrs = Unit->GetUnitAttributeSet();
		const bool bASCValid = ASC != nullptr;
		const bool bActorInfoValid = bASCValid
			&& ASC->GetOwnerActor() == Unit
			&& ASC->GetAvatarActor() == Unit;
		const bool bDeadTag = bASCValid
			&& ASC->HasMatchingGameplayTag(FGPGameplayTags::Get().Unit_State_Dead);

		const UWorld* World = Unit->GetWorld();
		UE_LOG(LogGPCombat, Log,
			TEXT("GP Combat.Inspect: Unit=%s Team=%d IsDead=%s ASC=%s ActorInfoValid=%s Health=%.2f MaxHealth=%.2f Damage=%.2f Armor=%.2f Resistance=%.2f Cooldown=%.2f AttackRange=%.2f DeadTagPresent=%s Role=%s NetMode=%s"),
			*Unit->GetName(),
			Unit->GetTeamId(),
			Unit->IsDead() ? TEXT("true") : TEXT("false"),
			bASCValid ? TEXT("valid") : TEXT("invalid"),
			bActorInfoValid ? TEXT("true") : TEXT("false"),
			Attrs != nullptr ? Attrs->GetHealth() : -1.0f,
			Attrs != nullptr ? Attrs->GetMaxHealth() : -1.0f,
			Attrs != nullptr ? Attrs->GetDamage() : -1.0f,
			Attrs != nullptr ? Attrs->GetArmor() : -1.0f,
			Attrs != nullptr ? Attrs->GetDamageResistance() : -1.0f,
			Attrs != nullptr ? Attrs->GetAttackCooldown() : -1.0f,
			Attrs != nullptr ? Attrs->GetAttackRange() : -1.0f,
			bDeadTag ? TEXT("true") : TEXT("false"),
			RoleToString(Unit->GetLocalRole()),
			NetModeToString(World != nullptr ? World->GetNetMode() : NM_MAX));
	}

	static void CombatInspect(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		AGP_UnitBase* Unit = FindFirstAuthorityAliveUnit(World);
		if (Unit == nullptr)
		{
			Unit = FindFirstAuthorityUnit(World);
		}
		LogInspect(Unit);
	}

	static void CombatSetStats(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPCombat, Warning, TEXT("GP Combat.SetStats: missing world"));
			return;
		}

		// gp.Combat.SetStats [Source|Target] Health MaxHealth Damage Armor Resistance Cooldown AttackRange
		int32 ValueOffset = 0;
		FString Selector = TEXT("Source");
		if (Args.Num() >= 8
			&& (Args[0].Equals(TEXT("Source"), ESearchCase::IgnoreCase)
				|| Args[0].Equals(TEXT("Target"), ESearchCase::IgnoreCase)))
		{
			Selector = Args[0];
			ValueOffset = 1;
		}

		if (Args.Num() - ValueOffset < 7)
		{
			UE_LOG(LogGPCombat, Warning,
				TEXT("GP Combat.SetStats usage: gp.Combat.SetStats [Source|Target] Health MaxHealth Damage Armor Resistance Cooldown AttackRange"));
			return;
		}

		const float Health = FCString::Atof(*Args[ValueOffset + 0]);
		const float MaxHealth = FCString::Atof(*Args[ValueOffset + 1]);
		const float Damage = FCString::Atof(*Args[ValueOffset + 2]);
		const float Armor = FCString::Atof(*Args[ValueOffset + 3]);
		const float Resistance = FCString::Atof(*Args[ValueOffset + 4]);
		const float Cooldown = FCString::Atof(*Args[ValueOffset + 5]);
		const float Range = FCString::Atof(*Args[ValueOffset + 6]);

		AGP_UnitBase* Source = FindCombatSource(World);
		AGP_UnitBase* Target = FindCombatTarget(World, Source);
		AGP_UnitBase* Unit = nullptr;

		if (Selector.Equals(TEXT("Target"), ESearchCase::IgnoreCase))
		{
			Unit = Target;
			if (Unit == nullptr)
			{
				Unit = FindFirstAuthorityAliveUnit(World);
			}
		}
		else
		{
			Unit = Source;
			if (Unit == nullptr)
			{
				Unit = FindFirstAuthorityAliveUnit(World);
			}
		}

		if (Unit == nullptr || Unit->IsDead())
		{
			UE_LOG(LogGPCombat, Warning, TEXT("GP Combat.SetStats: no authority alive unit for selector=%s"), *Selector);
			return;
		}

		const UGP_UnitAttributeSet* Attrs = Unit->GetUnitAttributeSet();
		UE_LOG(LogGPCombat, Log,
			TEXT("GP Combat.SetStats Before: Selector=%s Unit=%s Health=%.2f MaxHealth=%.2f Damage=%.2f Armor=%.2f Resistance=%.2f Cooldown=%.2f AttackRange=%.2f"),
			*Selector,
			*Unit->GetName(),
			Attrs != nullptr ? Attrs->GetHealth() : -1.0f,
			Attrs != nullptr ? Attrs->GetMaxHealth() : -1.0f,
			Attrs != nullptr ? Attrs->GetDamage() : -1.0f,
			Attrs != nullptr ? Attrs->GetArmor() : -1.0f,
			Attrs != nullptr ? Attrs->GetDamageResistance() : -1.0f,
			Attrs != nullptr ? Attrs->GetAttackCooldown() : -1.0f,
			Attrs != nullptr ? Attrs->GetAttackRange() : -1.0f);

		ApplyCombatStats(Unit, Health, MaxHealth, Damage, Armor, Resistance, Cooldown, Range);

		Attrs = Unit->GetUnitAttributeSet();
		UE_LOG(LogGPCombat, Log,
			TEXT("GP Combat.SetStats After: Selector=%s Unit=%s Health=%.2f MaxHealth=%.2f Damage=%.2f Armor=%.2f Resistance=%.2f Cooldown=%.2f AttackRange=%.2f"),
			*Selector,
			*Unit->GetName(),
			Attrs != nullptr ? Attrs->GetHealth() : -1.0f,
			Attrs != nullptr ? Attrs->GetMaxHealth() : -1.0f,
			Attrs != nullptr ? Attrs->GetDamage() : -1.0f,
			Attrs != nullptr ? Attrs->GetArmor() : -1.0f,
			Attrs != nullptr ? Attrs->GetDamageResistance() : -1.0f,
			Attrs != nullptr ? Attrs->GetAttackCooldown() : -1.0f,
			Attrs != nullptr ? Attrs->GetAttackRange() : -1.0f);
	}

	static bool ResolveSourceTarget(UWorld* World, AGP_UnitBase*& OutSource, AGP_UnitBase*& OutTarget)
	{
		OutSource = FindCombatSource(World);
		OutTarget = FindCombatTarget(World, OutSource);
		if (OutSource == nullptr || OutTarget == nullptr)
		{
			UE_LOG(LogGPCombat, Warning,
				TEXT("GP Combat: unable to resolve source/target (Source=%s Target=%s)"),
				*GetNameSafe(OutSource),
				*GetNameSafe(OutTarget));
			return false;
		}

		UE_LOG(LogGPCombat, Log,
			TEXT("GP Combat Select: Source=%s Team=%d Target=%s Team=%d"),
			*OutSource->GetName(),
			OutSource->GetTeamId(),
			*OutTarget->GetName(),
			OutTarget->GetTeamId());
		return true;
	}

	static void CombatApplyDamage(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 1)
		{
			UE_LOG(LogGPCombat, Warning, TEXT("GP Combat.ApplyDamage usage: gp.Combat.ApplyDamage Amount"));
			return;
		}

		AGP_UnitBase* Source = nullptr;
		AGP_UnitBase* Target = nullptr;
		if (!ResolveSourceTarget(World, Source, Target))
		{
			return;
		}

		UGP_AbilitySystemComponent* SourceASC = Source->GetGPAbilitySystemComponent();
		const UGP_UnitAttributeSet* SourceAttrs = Source->GetUnitAttributeSet();
		if (SourceASC == nullptr || SourceAttrs == nullptr)
		{
			UE_LOG(LogGPCombat, Warning, TEXT("GP Combat.ApplyDamage: source missing ASC/attrs"));
			return;
		}

		const float Amount = FCString::Atof(*Args[0]);
		const float PreviousDamage = SourceAttrs->GetDamage();
		const float TemporaryDamage = GPCombatPrivate::SanitizeNonNegative(Amount);

		SourceASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), TemporaryDamage);

		FGP_DamageApplicationResult Result;
		const bool bApplied = Target->ApplyDamageFromUnit(Source, Result);

		SourceASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), PreviousDamage);

		UE_LOG(LogGPCombat, Log,
			TEXT("GP Combat.ApplyDamage Done: Applied=%s HealthBefore=%.2f HealthAfter=%.2f AppliedDamage=%.2f Reject=%s RestoredSourceDamage=%.2f"),
			bApplied ? TEXT("true") : TEXT("false"),
			Result.HealthBefore,
			Result.HealthAfter,
			Result.FinalDamage,
			*Result.RejectReason,
			PreviousDamage);
	}

	static void CombatKillTarget(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPCombat, Warning, TEXT("GP Combat.KillTarget: missing world"));
			return;
		}

		AGP_UnitBase* Source = nullptr;
		AGP_UnitBase* Target = nullptr;
		if (!ResolveSourceTarget(World, Source, Target))
		{
			return;
		}

		UGP_AbilitySystemComponent* SourceASC = Source->GetGPAbilitySystemComponent();
		const UGP_UnitAttributeSet* SourceAttrs = Source->GetUnitAttributeSet();
		const UGP_UnitAttributeSet* TargetAttrs = Target->GetUnitAttributeSet();
		if (SourceASC == nullptr || SourceAttrs == nullptr || TargetAttrs == nullptr)
		{
			UE_LOG(LogGPCombat, Warning, TEXT("GP Combat.KillTarget: missing ASC/attrs"));
			return;
		}

		const float PreviousDamage = SourceAttrs->GetDamage();
		const float TargetHealth = TargetAttrs->GetHealth();
		const float TargetArmor = FMath::Max(0.0f, TargetAttrs->GetArmor());
		const float TargetRes = FMath::Clamp(TargetAttrs->GetDamageResistance(), 0.0f, 1.0f);
		const float SurvivabilityFactor = FMath::Max(0.05f, 1.0f - TargetRes);
		const float RequiredRaw = (TargetHealth / SurvivabilityFactor) + TargetArmor + 1.0f;
		const float TemporaryDamage = GPCombatPrivate::SanitizeNonNegative(RequiredRaw);

		SourceASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), TemporaryDamage);

		FGP_DamageApplicationResult Result;
		const bool bApplied = Target->ApplyDamageFromUnit(Source, Result);

		SourceASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetDamageAttribute(), PreviousDamage);

		UE_LOG(LogGPCombat, Log,
			TEXT("GP Combat.KillTarget Done: Applied=%s TempDamage=%.2f HealthBefore=%.2f HealthAfter=%.2f TargetDead=%s Reject=%s"),
			bApplied ? TEXT("true") : TEXT("false"),
			TemporaryDamage,
			Result.HealthBefore,
			Result.HealthAfter,
			Target->IsDead() ? TEXT("true") : TEXT("false"),
			*Result.RejectReason);
	}

	static FAutoConsoleCommandWithWorldAndArgs GCombatInspectCommand(
		TEXT("gp.Combat.Inspect"),
		TEXT("Inspect first authority alive unit combat/ASC state (fallback: first authority unit)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CombatInspect));

	static FAutoConsoleCommandWithWorldAndArgs GCombatSetStatsCommand(
		TEXT("gp.Combat.SetStats"),
		TEXT("gp.Combat.SetStats [Source|Target] Health MaxHealth Damage Armor Resistance Cooldown AttackRange"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CombatSetStats));

	static FAutoConsoleCommandWithWorldAndArgs GCombatApplyDamageCommand(
		TEXT("gp.Combat.ApplyDamage"),
		TEXT("gp.Combat.ApplyDamage Amount — apply real GE damage from Team1 source to valid target."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CombatApplyDamage));

	static FAutoConsoleCommandWithWorldAndArgs GCombatKillTargetCommand(
		TEXT("gp.Combat.KillTarget"),
		TEXT("Kill selected combat target through real GE/MMC path."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CombatKillTarget));
}
#endif
