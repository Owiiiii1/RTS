// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPDefensiveTurret.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitCommandComponent.h"

AGP_DefensiveTurret::AGP_DefensiveTurret()
{
	DefaultMaxHealth = 400.0f;
	DefaultHealth = 400.0f;
	DefaultDamage = 20.0f;
	DefaultAttackCooldown = 1.0f;
	DefaultAttackRange = 600.0f;
	FallbackFogOfWarSightRadiusCm = 900.0f;
	bFallbackGrantsFogOfWarVision = true;

	if (UGP_UnitCommandComponent* Command = GetUnitCommandComponent())
	{
		// Stationary: acquire only inside fire range so Attack never needs approach.
		Command->AutoAcquireSightRangeCm = 600.0f;
		Command->AttackFacingRotationSpeedDegreesPerSecond = 360.0f;
	}

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(60.0f, 100.0f);
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
		NavigationObstacle->SetBoxExtent(FVector(90.0f, 90.0f, 100.0f));
	}
	if (PlacementFootprintBounds)
	{
		// Native 2×2 BuildGrid (400×400 cm). BP children may retune extent/scale.
		PlacementFootprintBounds->SetBoxExtent(FVector(200.0f, 200.0f, 20.0f));
	}

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	PresentationRoot->SetupAttachment(CapsuleComponent);
	PresentationRoot->SetCanEverAffectNavigation(false);

	CombatOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("CombatOrigin"));
	CombatOrigin->SetupAttachment(CapsuleComponent);
	CombatOrigin->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	CombatOrigin->SetCanEverAffectNavigation(false);
	CombatOrigin->bEditableWhenInherited = true;

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPTags.Building_Type_DefensiveTurret.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Building_Type_DefensiveTurret);
	}
}

UCapsuleComponent* AGP_DefensiveTurret::GetCapsuleComponent() const
{
	return CapsuleComponent;
}

USceneComponent* AGP_DefensiveTurret::GetPresentationRoot() const
{
	return PresentationRoot;
}

USceneComponent* AGP_DefensiveTurret::GetCombatOrigin() const
{
	return CombatOrigin;
}
