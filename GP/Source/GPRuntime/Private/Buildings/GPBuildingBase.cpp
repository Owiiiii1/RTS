// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPBuildingBase.h"

#include "Components/BoxComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "Tags/GPGameplayTags.h"

AGP_BuildingBase::AGP_BuildingBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	NavigationObstacle = CreateDefaultSubobject<UBoxComponent>(TEXT("NavigationObstacle"));
	ConfigureNavigationObstacleDefaults();
	// Attachment deferred until derived classes set Capsule root (PostInitializeComponents).

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	CapabilityTags.Reset();
	if (GPTags.Capability_Selectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Selectable);
	}
	if (GPTags.Capability_Inspectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Inspectable);
	}
	if (GPTags.Selection_Type_Building.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Selection_Type_Building);
	}
	if (GPTags.Unit_Type_Building.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Unit_Type_Building);
	}
}

void AGP_BuildingBase::ConfigureNavigationObstacleDefaults()
{
	if (!NavigationObstacle)
	{
		return;
	}

	NavigationObstacle->SetBoxExtent(FVector(140.0f, 140.0f, 120.0f));
	NavigationObstacle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NavigationObstacle->SetCollisionObjectType(ECC_WorldStatic);
	NavigationObstacle->SetCollisionResponseToAllChannels(ECR_Ignore);
	NavigationObstacle->SetGenerateOverlapEvents(false);
	NavigationObstacle->SetSimulatePhysics(false);
	NavigationObstacle->SetHiddenInGame(true);
	NavigationObstacle->SetVisibility(true);
	NavigationObstacle->SetCanEverAffectNavigation(true);
	NavigationObstacle->bDynamicObstacle = true;
	NavigationObstacle->SetAreaClassOverride(UNavArea_Null::StaticClass());
}

void AGP_BuildingBase::AttachNavigationObstacleToRoot()
{
	if (!NavigationObstacle)
	{
		return;
	}

	USceneComponent* Root = GetRootComponent();
	if (Root == nullptr)
	{
		return;
	}

	if (NavigationObstacle->GetAttachParent() != Root)
	{
		NavigationObstacle->SetupAttachment(Root);
	}
}

void AGP_BuildingBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AttachNavigationObstacleToRoot();
}
