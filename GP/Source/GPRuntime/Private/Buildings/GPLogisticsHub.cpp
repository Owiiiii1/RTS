// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPLogisticsHub.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Effects/GPGE_UnitCap_Plus5.h"
#include "GameplayEffect.h"
#include "Player/GPPlayerState.h"
#include "Tags/GPGameplayTags.h"

AGP_LogisticsHub::AGP_LogisticsHub()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(80.0f, 120.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CapsuleComponent->SetGenerateOverlapEvents(false);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->SetSimulatePhysics(false);

	AttachNavigationObstacleToRoot();
	if (NavigationObstacle)
	{
		// Rough LogisticsHub footprint — BP may retune freely.
		NavigationObstacle->SetBoxExtent(FVector(140.0f, 140.0f, 120.0f));
	}
	if (PlacementFootprintBounds)
	{
		// Native 4×4 BuildGrid (800×800 cm). BP children may retune extent/scale.
		PlacementFootprintBounds->SetBoxExtent(FVector(400.0f, 400.0f, 20.0f));
	}

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	PresentationRoot->SetupAttachment(CapsuleComponent);
	PresentationRoot->SetCanEverAffectNavigation(false);

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPTags.Building_Type_LogisticsHub.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Building_Type_LogisticsHub);
	}
}

UCapsuleComponent* AGP_LogisticsHub::GetCapsuleComponent() const
{
	return CapsuleComponent;
}

USceneComponent* AGP_LogisticsHub::GetPresentationRoot() const
{
	return PresentationRoot;
}

void AGP_LogisticsHub::BeginPlay()
{
	Super::BeginPlay();
	TryApplyUnitCapBonus();
}

void AGP_LogisticsHub::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveUnitCapBonus();
	Super::EndPlay(EndPlayReason);
}

void AGP_LogisticsHub::NotifyTeamIdChanged(int32 OldTeamId, int32 NewTeamId)
{
	Super::NotifyTeamIdChanged(OldTeamId, NewTeamId);
	TryApplyUnitCapBonus();
}

void AGP_LogisticsHub::NotifyAuthorityDeath()
{
	RemoveUnitCapBonus();
	Super::NotifyAuthorityDeath();
}

AGP_PlayerState* AGP_LogisticsHub::ResolveOwningPlayerStateForHubBonus() const
{
	if (AGP_PlayerState* OwnerPS = Cast<AGP_PlayerState>(GetOwner()))
	{
		return OwnerPS;
	}

	if (GetTeamId() < 1)
	{
		return nullptr;
	}

	return AGP_PlayerState::FindAuthoritativeForTeam(GetWorld(), GetTeamId());
}

void AGP_LogisticsHub::TryApplyUnitCapBonus()
{
	if (!HasAuthority() || bUnitCapBonusApplied || IsDead())
	{
		return;
	}

	AGP_PlayerState* PS = ResolveOwningPlayerStateForHubBonus();
	if (!IsValid(PS))
	{
		if (GetTeamId() >= 1)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("GP UnitCap Hub bonus unresolved owner: Hub=%s TeamId=%d Owner=%s"),
				*GetName(),
				GetTeamId(),
				*GetNameSafe(GetOwner()));
		}
		return;
	}

	UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent();
	if (ASC == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP UnitCap Hub bonus missing ASC: Hub=%s PS=%s"),
			*GetName(),
			*GetNameSafe(PS));
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec =
		ASC->MakeOutgoingSpec(UGP_GE_UnitCap_Plus5::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("GP UnitCap Hub Plus5 spec invalid: Hub=%s PS=%s"),
			*GetName(),
			*GetNameSafe(PS));
		return;
	}

	UnitCapBonusHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!UnitCapBonusHandle.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("GP UnitCap Hub Plus5 apply failed: Hub=%s PS=%s"),
			*GetName(),
			*GetNameSafe(PS));
		return;
	}

	bUnitCapBonusApplied = true;
	UnitCapBonusOwnerWeak = PS;

	const UGP_PlayerAttributeSet* Attr = PS->GetPlayerAttributeSet();
	UE_LOG(LogTemp, Log,
		TEXT("GP UnitCap Hub Plus5 applied: Hub=%s PS=%s MaxUnits=%.0f"),
		*GetName(),
		*GetNameSafe(PS),
		Attr != nullptr ? Attr->GetMaxUnits() : 0.0f);
}

void AGP_LogisticsHub::RemoveUnitCapBonus()
{
	if (!HasAuthority() || !bUnitCapBonusApplied)
	{
		return;
	}

	AGP_PlayerState* PS = UnitCapBonusOwnerWeak.Get();
	if (!IsValid(PS))
	{
		PS = ResolveOwningPlayerStateForHubBonus();
	}

	if (IsValid(PS))
	{
		if (UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent())
		{
			if (UnitCapBonusHandle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(UnitCapBonusHandle);
			}
			else
			{
				ASC->RemoveActiveGameplayEffectBySourceEffect(
					UGP_GE_UnitCap_Plus5::StaticClass(),
					ASC,
					1);
			}
		}
	}

	UnitCapBonusHandle.Invalidate();
	bUnitCapBonusApplied = false;
	UnitCapBonusOwnerWeak.Reset();

	UE_LOG(LogTemp, Log,
		TEXT("GP UnitCap Hub Plus5 removed: Hub=%s PS=%s MaxUnits=%.0f"),
		*GetName(),
		*GetNameSafe(PS),
		(IsValid(PS) && PS->GetPlayerAttributeSet() != nullptr)
			? PS->GetPlayerAttributeSet()->GetMaxUnits()
			: 0.0f);
}
