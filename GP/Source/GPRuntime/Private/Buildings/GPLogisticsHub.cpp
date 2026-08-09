// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPLogisticsHub.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
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
